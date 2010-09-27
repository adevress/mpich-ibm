/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_request.c
 * \brief Accessors and actors for MPID Requests
 */
#include "mpidimpl.h"

#ifndef MPID_REQUEST_PREALLOC
#define MPID_REQUEST_PREALLOC 8
#endif

/**
 * \defgroup MPID_REQUEST MPID Request object management
 *
 * Accessors and actors for MPID Requests
 */


/* these are referenced by src/mpi/pt2pt/wait.c in PMPI_Wait! */
MPID_Request MPID_Request_direct[MPID_REQUEST_PREALLOC];
MPIU_Object_alloc_t MPID_Request_mem =
  {
    0, 0, 0, 0, MPID_REQUEST, sizeof(MPID_Request),
    MPID_Request_direct,
    MPID_REQUEST_PREALLOC
  };


#if 0
/**
 * \brief Create and initialize a new request
 */

MPID_Request *
MPID_Request_create()
{
  MPID_Request * req;

  MPIDI_Request_tls_alloc(req);
  if (req == NULL)
    MPID_Abort(NULL, MPI_ERR_NO_SPACE, -1, "Cannot allocate Request");

  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPIU_Object_set_ref(req, 1);
  MPID_cc_set(&req->cc, 1);
  req->cc_ptr            = &req->cc;
  req->status.MPI_SOURCE = MPI_UNDEFINED;
  req->status.MPI_TAG    = MPI_UNDEFINED;
  req->status.MPI_ERROR  = MPI_SUCCESS;
  req->status.count      = 0;
  req->status.cancelled  = FALSE;
  req->comm              = NULL;

  struct MPIDI_Request* mpid = &req->mpid;
  memset(mpid, 0x00, sizeof(struct MPIDI_Request));
#if 0
  /* These two commands are not needed as long as the constants are 0.
     There are comments to that effect in their definitions. */
  mpid->state = MPIDI_INITIALIZED;
  MPIDI_Request_setCA(req, MPIDI_CA_COMPLETE);
#endif

  return req;
}
#endif

static inline void
MPIDI_Request_try_free(MPID_Request *req)
{
  if ( (MPIU_Object_get_ref(req) == 0) && (MPID_cc_is_complete(&req->cc)) )
    {
      if (req->comm)              MPIR_Comm_release(req->comm, 0);
      if (req->mpid.datatype_ptr) MPID_Datatype_release(req->mpid.datatype_ptr);
      MPIDI_Request_tls_free(req);
    }
}


/* *********************************************************************** */
/*           destroy a request                                             */
/* *********************************************************************** */

void
MPID_Request_release(MPID_Request *req)
{
  int ref_count;
  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPIU_Object_release_ref(req, &ref_count);
  MPID_assert(MPIU_Object_get_ref(req) >= 0);
  MPIDI_Request_try_free(req);
}

/* *********************************************************************** */
/*            Dealing with completion counts                               */
/* *********************************************************************** */

void
MPIDI_Request_complete(MPID_Request *req)
{
  int cc;
  MPIDI_Request_decrement_cc(req, &cc);
  if (MPID_cc_is_complete(&req->cc)) /* decrement completion count; if 0, signal progress engine */
    {
      MPIDI_Request_try_free(req);
      MPIDI_Progress_signal();
    }
}

void
MPID_Request_set_completed(MPID_Request *req)
{
  MPID_cc_set(&req->cc, 0);
  MPIDI_Request_try_free(req);
  MPIDI_Progress_signal();
}
