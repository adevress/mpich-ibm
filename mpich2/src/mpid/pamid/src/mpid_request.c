/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_request.c
 * \brief Accessors and actors for MPID Requests
 */
#include <mpidimpl.h>

#ifndef MPID_REQUEST_PREALLOC
#define MPID_REQUEST_PREALLOC 16
#endif

/**
 * \defgroup MPID_REQUEST MPID Request object management
 *
 * Accessors and actors for MPID Requests
 */


/* these are referenced by src/mpi/pt2pt/wait.c in PMPI_Wait! */
MPID_Request MPID_Request_direct[MPID_REQUEST_PREALLOC] __attribute__((__aligned__(64)));
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


void
MPIDI_Request_uncomplete(MPID_Request *req)
{
  int count;
  MPIU_Object_add_ref(req);
  MPID_cc_incr(req->cc_ptr, &count);
}


void
MPID_Request_set_completed(MPID_Request *req)
{
  MPID_cc_set(&req->cc, 0);
  MPIDI_Progress_signal();
}
