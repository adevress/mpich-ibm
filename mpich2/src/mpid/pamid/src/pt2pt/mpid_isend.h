/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpid_isend.h
 * \brief ???
 */

#ifndef __src_pt2pt_mpid_isend_h__
#define __src_pt2pt_mpid_isend_h__


#define MPID_Isend          MPID_Isend_inline


static inline unsigned
MPIDI_Context_hash(pami_task_t rank, unsigned ctxt, unsigned ncontexts)
{
  return (( rank + ctxt ) & (ncontexts-1));
}
static inline void
MPIDI_Context_endpoint(MPID_Request * req, pami_endpoint_t * e)
{
  pami_task_t remote = MPIDI_Request_getPeerRank(req);
  pami_task_t local  = MPIR_Process.comm_world->rank;
  unsigned    rctxt  = MPIDI_Context_hash(local, req->comm->context_id, MPIDI_Process.avail_contexts);

  pami_result_t rc;
  rc = PAMI_ENDPOINT_CREATE(MPIDI_Client, remote, rctxt, e);
  MPID_assert(rc == PAMI_SUCCESS);
}
static inline pami_context_t
MPIDI_Context_local(MPID_Request * req)
{
  pami_task_t remote = MPIDI_Request_getPeerRank(req);
  unsigned    lctxt  = MPIDI_Context_hash(remote, req->comm->context_id, MPIDI_Process.avail_contexts);
  MPID_assert(lctxt < MPIDI_Process.avail_contexts);
  return MPIDI_Context[lctxt];
}


/**
 * \brief ADI level implemenation of MPI_Isend()
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 *
 * This is a slight variation on mpid_send.c - basically, we *always*
 * want to return a send request even if the request is already
 * complete (as is in the case of sending to a NULL rank).
 */
static inline int
MPID_Isend_inline(const void    * buf,
                  int             count,
                  MPI_Datatype    datatype,
                  int             rank,
                  int             tag,
                  MPID_Comm     * comm,
                  int             context_offset,
                  MPID_Request ** request)
{
  /* ---------------------------------------------------- */
  /* special case: PROC null handled by handoff function  */
  /* ---------------------------------------------------- */

  /* --------------------- */
  /* create a send request */
  /* --------------------- */
  MPID_Request * sreq = *request = MPIDI_Request_create2_fast();

  unsigned context_id = comm->context_id;
  /* match info */
  MPIDI_Request_setMatch(sreq, tag, comm->rank, context_id+context_offset);

  /* data buffer info */
  sreq->mpid.userbuf        = (void*)buf;
  sreq->mpid.userbufcount   = count;
  sreq->mpid.datatype       = datatype;

  /* Enable passing in MPI_PROC_NULL, do the translation in the
     handoff function */
  MPIDI_Request_setPeerRank(sreq, rank);

  unsigned ncontexts = MPIDI_Process.avail_contexts;
  /* communicator & destination info */
  sreq->comm = comm;
  sreq->kind = MPID_REQUEST_SEND;
  MPIR_Comm_add_ref(comm);

  if (likely(MPIDI_Process.context_post > 0))
    {
      pami_context_t context = MPIDI_Context[MPIDI_Context_hash(rank, context_id, ncontexts)];
      pami_result_t rc;
      rc = PAMI_Context_post(context, &sreq->mpid.post_request, MPIDI_Isend_handoff, sreq);
      MPID_assert(rc == PAMI_SUCCESS);
    }
  else
    {
      MPIDI_Isend_handoff(MPIDI_Context[0], sreq);
    }

  return MPI_SUCCESS;
}


#endif
