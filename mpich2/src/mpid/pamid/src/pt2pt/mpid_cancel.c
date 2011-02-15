/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpid_cancel.c
 * \brief Device interface for canceling an MPI Recv
 */
#include <mpidimpl.h>


int
MPID_Cancel_recv(MPID_Request * rreq)
{
  MPID_assert(rreq->kind == MPID_REQUEST_RECV);
  if (MPIDI_Recvq_FDPR(rreq))
    {
      rreq->status.cancelled = TRUE;
      rreq->status.count = 0;
      MPIDI_Request_complete(rreq);
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
MPIDI_CancelReq_post(pami_context_t context, void * _req)
{
  MPID_Request * req = (MPID_Request*)_req;
  MPID_assert(req != NULL);

  /* ------------------------------------------------- */
  /* Check if we already have a cancel request pending */
  /* ------------------------------------------------- */
  int flag;
  MPIDI_Request_cancel_pending(req, &flag);
  if (flag)
    {
      MPIDI_Request_complete(req);
      return PAMI_SUCCESS;
    }

  MPIDI_MsgInfo cancel = {
  MPItag   : MPIDI_Request_getMatchTag(req),
  MPIrank  : MPIDI_Request_getMatchRank(req),
  MPIctxt  : MPIDI_Request_getMatchCtxt(req),
  control  : MPIDI_CONTROL_CANCEL_REQUEST,
  req      : req,
  };

  pami_endpoint_t dest;
  MPIDI_Context_endpoint(req, &dest);

  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols_Cancel,
  dest     : dest,
  header   : {
    iov_base: &cancel,
    iov_len: sizeof(MPIDI_MsgInfo),
    },
  };

  TRACE_ERR("Running posted cancel for request=%p\n", req);
  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);

  return PAMI_SUCCESS;
}


int
MPID_Cancel_send(MPID_Request * sreq)
{
  MPID_assert(sreq != NULL);

  if(!sreq->comm)
    return MPI_SUCCESS;

  MPIDI_Request_uncomplete(sreq);
  /* TRACE_ERR("Posting cancel for request=%p   cc(curr)=%d ref(curr)=%d\n", sreq, val+1, MPIU_Object_get_ref(sreq)); */

  if (likely(MPIDI_Process.context_post > 1))
    {
      /* This leaks intentionally.  At this time, the amount of work
       * required to avoid a leak here just isn't worth it.
       * Hopefully people aren't cancelling sends too much.
       */
      pami_work_t  * work    = malloc(sizeof(pami_work_t));
      pami_context_t context = MPIDI_Context_local(sreq);
      PAMI_Context_post(context, work, MPIDI_CancelReq_post, sreq);
    }
  else
    {
      MPIDI_CancelReq_post(MPIDI_Context[0], sreq);
    }

  return MPI_SUCCESS;
}
