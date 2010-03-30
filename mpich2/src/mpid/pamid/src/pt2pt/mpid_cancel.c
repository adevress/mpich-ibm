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
static inline pami_result_t
MPIDI_postCancelReq(pami_context_t context, void * _req)
{
  MPID_Request * req = (MPID_Request *) _req;
  MPID_assert(req != NULL);

  MPIDI_MsgInfo cancel = {
  msginfo: {
    MPItag   : MPIDI_Request_getMatchTag(req),
    MPIrank  : MPIDI_Request_getMatchRank(req),
    MPIctxt  : MPIDI_Request_getMatchCtxt(req),
    peerrank : MPIDI_Process.global.rank,
    type     : MPIDI_REQUEST_TYPE_CANCEL_REQUEST,
    req      : req,
    }
  };

  pami_endpoint_t       dest   = MPIDI_Context_endpoint(req);
  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols.Cancel,
  dest     : dest,
  header   : {
    iov_base: &cancel,
    iov_len: sizeof(MPIDI_MsgInfo),
    },
  };

  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);

  return PAMI_SUCCESS;
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

      {
        /* This leaks intentionally.  At this time, the amount of work
         * required to avoid a leak here just isn't worth it.
         * Hopefully people aren't cancelling sends too much.
         */
        pami_work_t  * work    = malloc(sizeof(pami_work_t));
        pami_context_t context = MPIDI_Context_local(sreq);
        PAMI_Context_post(context, work, MPIDI_postCancelReq, sreq);
      }

      return MPI_SUCCESS;
    }
}
