/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_inlines.h
 * \brief ???
 */


#ifndef __include_mpidi_inlines_h__
#define __include_mpidi_inlines_h__


#define MPID_Request_create MPID_Request_create_inline
#define MPID_Isend          MPID_Isend_inline

extern MPIU_Object_alloc_t MPID_Request_mem;

/**
 * \brief Create and initialize a new request
 */

static inline MPID_Request *
MPID_Request_create_inline()
{
  MPID_Request * req;

  MPIDI_Request_tls_alloc(req);
  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPIU_Object_set_ref(req, 1);
  MPID_cc_set_1(&req->cc);
  req->cc_ptr            = &req->cc;
  req->comm              = NULL;
  req->status.count      = 0;
  req->status.cancelled  = FALSE;
  req->status.MPI_SOURCE = MPI_UNDEFINED;
  req->status.MPI_TAG    = MPI_UNDEFINED;
  req->status.MPI_ERROR  = MPI_SUCCESS;

  struct MPIDI_Request* mpid = &req->mpid;
#if 0
  memset(mpid, 0x00, sizeof(struct MPIDI_Request));
#else
  /* These two commands are not needed as long as the constants are 0.
     There are comments to that effect in their definitions. */
  mpid->next                     = NULL;
  mpid->cancel_pending           = FALSE;
  mpid->state = MPIDI_INITIALIZED;
  MPIDI_Request_setCA(req, MPIDI_CA_COMPLETE);
#endif

  return req;
}


static inline void
MPIDI_SendMsg(MPID_Request * sreq)
{
  if (MPIDI_Process.avail_contexts > 1) {
    pami_context_t context = MPIDI_Context_local(sreq);

    pami_result_t rc;
    rc = PAMI_Context_post(context, &sreq->mpid.post_request, MPIDI_SendMsg_handoff, sreq);
    MPID_assert(rc == PAMI_SUCCESS);
  }
  else {
    MPIDI_SendMsg_handoff( MPIDI_Context[0], sreq);
  }
}


#include "../src/pt2pt/mpidi_send.h"


int
MPID_Isend_outline(const void    * buf,
                   int             count,
                   MPI_Datatype    datatype,
                   int             rank,
                   int             tag,
                   MPID_Comm     * comm,
                   int             context_offset,
                   MPID_Request ** request);


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
MPID_Isend_inline (const void    * buf,
                   int             count,
                   MPI_Datatype    datatype,
                   int             rank,
                   int             tag,
                   MPID_Comm     * comm,
                   int             context_offset,
                   MPID_Request ** request)
{
  MPID_Request * sreq = NULL;

  /* --------------------------- */
  /* special case: send-to-self  */
  /* --------------------------- */

  if (unlikely( (rank == comm->rank) ||
		/* Do we care about inter communicators ??*/
		/* (comm->comm_kind == MPID_INTERCOMM) ||*/
		(rank == MPI_PROC_NULL) ))
    {
      return MPID_Isend_outline(buf,
                                count,
                                datatype,
                                rank,
                                tag,
                                comm,
                                context_offset,
                                request);
    }

  /* --------------------- */
  /* create a send request */
  /* --------------------- */

  sreq = MPID_Request_create();

  /* match info */
  MPIDI_Request_setMatch(sreq, tag, comm->rank, comm->context_id+context_offset);

  /* data buffer info */
  sreq->mpid.userbuf      = (void*)buf;
  sreq->mpid.userbufcount = count;
  sreq->mpid.datatype     = datatype;
  MPIDI_Request_setPeerRank(sreq, comm->vcr[rank]);
  MPIDI_Request_setPeerRequest(sreq, sreq);

  /* communicator & destination info */
  sreq->comm              = comm;

  MPIR_Comm_add_ref(comm);

  /* message type info */
  sreq->kind = MPID_REQUEST_SEND;

  *request = sreq;
  MPIDI_SendMsg(sreq);
  return MPI_SUCCESS;
}

#endif
