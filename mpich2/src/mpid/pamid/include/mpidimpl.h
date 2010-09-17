/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidimpl.h
 * \brief API MPID additions to MPI functions and structures
 */

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef __include_mpidimpl_h__
#define __include_mpidimpl_h__

#include <pamix.h>
#include <mpiimpl.h>
#include <mpidpre.h>
#include <mpidpost.h>


#include <mpidi_prototypes.h>
#include <mpidi_macros.h>


/**
 * \addtogroup MPID_REQUEST
 * \{
 */

#define MPIU_HANDLE_ALLOCATION_MUTEX         0
#define MPIU_HANDLE_ALLOCATION_THREAD_LOCAL  1

/* XXX DJG for TLS hack */
#define MPID_REQUEST_TLS_MAX 512

#if MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_THREAD_LOCAL

extern __thread MPID_Request * MPID_PAMID_Thread_request_handles;
extern __thread int MPID_PAMID_Thread_request_handle_count;

#  define MPIDI_Request_tls_alloc(req)                                     \
  do {                                                                     \
    int i;                                                                 \
    if (!MPID_PAMID_Thread_request_handles) {                              \
      MPID_Request *prev, *cur;                                            \
      /* batch allocate a linked list of requests */                       \
      MPIU_THREAD_CS_ENTER(HANDLEALLOC,);                                  \
      prev = MPIU_Handle_obj_alloc_unsafe(&MPID_Request_mem);              \
      prev->mpid.next = NULL;                                              \
      assert(prev);                                                        \
      for (i = 1; i < MPID_REQUEST_TLS_MAX; ++i) {                         \
	cur = MPIU_Handle_obj_alloc_unsafe(&MPID_Request_mem);             \
	assert(cur);                                                       \
	cur->mpid.next = prev;                                             \
	prev = cur;                                                        \
      }                                                                    \
      MPIU_THREAD_CS_EXIT(HANDLEALLOC,);                                   \
      MPID_PAMID_Thread_request_handles = cur;                             \
      MPID_PAMID_Thread_request_handle_count += MPID_REQUEST_TLS_MAX;      \
    }                                                                      \
    (req) = MPID_PAMID_Thread_request_handles;                             \
    MPID_PAMID_Thread_request_handles = req->mpid.next;                    \
    MPID_PAMID_Thread_request_handle_count -= 1;                           \
  } while (0)

#  define MPIDI_Request_tls_free(req)                                      \
  do {                                                                     \
    if (MPID_PAMID_Thread_request_handle_count < MPID_REQUEST_TLS_MAX) {   \
      /* push request onto the top of the stack */                         \
      req->mpid.next = MPID_PAMID_Thread_request_handles;                  \
      MPID_PAMID_Thread_request_handles = req;                             \
      MPID_PAMID_Thread_request_handle_count += 1;                         \
    }                                                                      \
    else {                                                                 \
      MPIU_Handle_obj_free(&MPID_Request_mem, req);                        \
    }                                                                     \
  } while (0)

#elif MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_MUTEX
#  define MPIDI_Request_tls_alloc(req) (req) = MPIU_Handle_obj_alloc(&MPID_Request_mem)
#  define MPIDI_Request_tls_free(req) MPIU_Handle_obj_free(&MPID_Request_mem, (req))
#else
#  error MPIU_HANDLE_ALLOCATION_METHOD not defined
#endif



#endif
