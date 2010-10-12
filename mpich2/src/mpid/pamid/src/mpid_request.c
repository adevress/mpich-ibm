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


#if MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_THREAD_LOCAL
__thread MPID_Request * MPID_PAMID_Thread_request_handles;
__thread int MPID_PAMID_Thread_request_handle_count;

void MPIDI_Request_allocate_pool()
{
  int i;
  MPID_Request *prev, *cur;
  /* batch allocate a linked list of requests */
  MPIU_THREAD_CS_ENTER(HANDLEALLOC,);
  prev = MPIU_Handle_obj_alloc_unsafe(&MPID_Request_mem);
  prev->mpid.next = NULL;
  assert(prev);
  for (i = 1; i < MPID_REQUEST_TLS_MAX; ++i) {
    cur = MPIU_Handle_obj_alloc_unsafe(&MPID_Request_mem);
    assert(cur);
    cur->mpid.next = prev;
    prev = cur;
  }
  MPIU_THREAD_CS_EXIT(HANDLEALLOC,);
  MPID_PAMID_Thread_request_handles = cur;
  MPID_PAMID_Thread_request_handle_count += MPID_REQUEST_TLS_MAX;
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
  int count;
  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPIU_Object_release_ref(req, &count);
  MPID_assert(count >= 0);
  MPIDI_Request_try_free(req);
}

/* *********************************************************************** */
/*            Dealing with completion counts                               */
/* *********************************************************************** */

void
MPIDI_Request_complete(MPID_Request *req)
{
  int count;
  MPIDI_Request_decrement_cc(req, &count);
  MPID_assert(count >= 0);
  if (count == 0) /* decrement completion count; if 0, signal progress engine */
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
