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


/**
 * \brief Create and initialize a new request
 */

MPID_Request *
MPID_Request_create()
{
  MPID_Request * req;

  req = MPIU_Handle_obj_alloc(&MPID_Request_mem);
  if (req == NULL)
    MPID_Abort(NULL, MPI_ERR_NO_SPACE, -1, "Cannot allocate Request");

  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPIU_Object_set_ref(req, 1);
  req->cc                = 1;
  req->cc_ptr            = & req->cc;
  req->status.MPI_SOURCE = MPI_UNDEFINED;
  req->status.MPI_TAG    = MPI_UNDEFINED;
  req->status.MPI_ERROR  = MPI_SUCCESS;
  req->status.count      = 0;
  req->status.cancelled  = FALSE;
  req->comm              = NULL;

  struct MPIDI_Request* mpid = &req->mpid;
  /* if (DCQuad_sizeof(MPIDI_MsgInfo) == 1) */
  /*   { */
  /*     DCQuad* q = mpid->envelope.envelope.msginfo.quad; */
  /*     q->w0 = 0; */
  /*     q->w1 = 0; */
  /*     q->w2 = 0; */
  /*     q->w3 = 0; */
  /*   } */
  /* else */
    memset(&mpid->envelope.envelope.msginfo, 0x00, sizeof(MPIDI_MsgInfo));

  mpid->envelope.envelope.length = 0;
  mpid->userbuf                  = NULL;
  mpid->uebuf                    = NULL;
  mpid->datatype_ptr             = NULL;
  mpid->cancel_pending           = FALSE;
  mpid->state                    = MPIDI_INITIALIZED;
  MPIDI_Request_setCA  (req, MPIDI_CA_COMPLETE);
  MPIDI_Request_setSelf(req, 0);
  MPIDI_Request_setType(req, MPIDI_REQUEST_TYPE_RECV);

  return req;
}

static inline void
MPIDI_Request_try_free(MPID_Request *req)
{
  if ( (req->ref_count == 0) && (MPIDI_Request_get_cc(req) == 0) )
    {
      if (req->comm)              MPIR_Comm_release(req->comm, 0);
      if (req->mpid.datatype_ptr) MPID_Datatype_release(req->mpid.datatype_ptr);
      MPIU_Handle_obj_free(&MPID_Request_mem, req);
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
  MPID_assert(req->ref_count >= 0);
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
  MPID_assert(cc >= 0);
  if (cc == 0) /* decrement completion count; if 0, signal progress engine */
    {
      MPIDI_Request_try_free(req);
      MPIDI_Progress_signal();
    }
}

void
MPID_Request_set_completed(MPID_Request *req)
{
  *(req)->cc_ptr = 0; /* force completion count to 0 */
  MPIDI_Request_try_free(req);
  MPIDI_Progress_signal();
}
