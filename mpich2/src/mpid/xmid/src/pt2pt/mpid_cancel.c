/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpid_cancel.c
 * \brief Device interface for canceling an MPI Recv
 */
#include "mpidimpl.h"


int
MPID_Cancel_recv(MPID_Request * rreq)
{
  MPID_assert(rreq->kind == MPID_REQUEST_RECV);
  if (MPIDI_Recvq_FDPR(rreq))
    {
      rreq->status.cancelled = TRUE;
      rreq->status.count = 0;
      MPID_Request_set_completed(rreq);
    }
  /* This is successful, even if the recv isn't cancelled */
  return MPI_SUCCESS;
}


/**
 * \brief Cancel an MPI_Isend()
 *
 * \param[in] req The request element to cancel
 *
 * \return The same as MPIDI_CtrlSend()
 */
static inline void
MPIDI_postCancelReq(xmi_context_t context, MPID_Request * req)
{
  MPID_assert(req != NULL);
  unsigned      peerrank  =  MPIDI_Request_getPeerRank(req);

  MPIDI_MsgInfo cancel = {
  msginfo: {
    MPItag   : MPIDI_Request_getMatchTag(req),
    MPIrank  : MPIDI_Request_getMatchRank(req),
    MPIctxt  : MPIDI_Request_getMatchCtxt(req),
    peerrank : MPIR_Process.comm_world->rank,
    type     : MPIDI_REQUEST_TYPE_CANCEL_REQUEST,
    req      : req,
    }
  };

  xmi_endpoint_t       dest   = XMI_Client_endpoint(MPIDI_Client, peerrank, 0);
  xmi_send_immediate_t params = {
  dispatch : MPIDI_Protocols.Cancel,
  dest     : dest,
  header   : {
    iov_base: &cancel,
    iov_len: sizeof(MPIDI_MsgInfo),
    },
  };

  xmi_result_t rc;
  rc = XMI_Send_immediate(context, &params);
  MPID_assert(rc == XMI_SUCCESS);
}


int
MPID_Cancel_send(MPID_Request * sreq)
{
  int flag;
  MPID_assert(sreq != NULL);

  /* ------------------------------------------------- */
  /* Check if we already have a cancel request pending */
  /* ------------------------------------------------- */
  MPIDI_Request_cancel_pending(sreq, &flag);
  if (flag)
    return MPI_SUCCESS;

  /* ------------------------------------ */
  /* Try to cancel a send request to self */
  /* ------------------------------------ */
  if (MPIDI_Request_isSelf(sreq))
    {
      int source     = MPIDI_Request_getMatchRank(sreq);
      int tag        = MPIDI_Request_getMatchTag (sreq);
      int context_id = MPIDI_Request_getMatchCtxt(sreq);
      MPID_Request * rreq = MPIDI_Recvq_FDUR(sreq, source, tag, context_id);
      if (rreq)
        {
          MPID_assert(rreq->partner_request == sreq);
          MPID_Request_release(rreq);
          sreq->status.cancelled = TRUE;
          sreq->cc = 0;
        }
      return MPI_SUCCESS;
    }
  else
    {
      if(!sreq->comm)
        return MPI_SUCCESS;

      MPIDI_Request_increment_cc(sreq);
      MPIDI_postCancelReq(MPIDI_Context[0], sreq);

      return MPI_SUCCESS;
    }
}
