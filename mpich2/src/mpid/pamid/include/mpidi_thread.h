/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidthread.h
 * \brief ???
 *
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidthread_h__
#define __include_mpidthread_h__

/**
 * ******************************************************************
 * \brief Mutexes for thread/interrupt safety
 * ******************************************************************
 */


#define MPID_DEFINES_MPID_CS 1
#define MPIU_ISTHREADED() ({ MPIR_ThreadInfo.isThreaded; })


#ifdef MPID_CS_ENTER
#error "MPID_CS_ENTER is already defined"
#endif



#if (MPICH_THREAD_LEVEL != MPI_THREAD_MULTIPLE)


#error MPICH_THREAD_LEVEL should be MPI_THREAD_MULTIPLE


#else


#define MPID_CS_INITIALIZE()                                            \
({                                                                      \
  /* Create thread local storage for nest count that MPICH uses */      \
  MPID_Thread_tls_create(NULL, &MPIR_ThreadInfo.thread_storage, NULL);  \
})
#define MPID_CS_FINALIZE()                                              \
({                                                                      \
  /* Destroy thread local storage created during MPID_CS_INITIALIZE */  \
  MPID_Thread_tls_destroy(&MPIR_ThreadInfo.thread_storage, NULL);       \
})
#define MPID_CS_ENTER() ({ if (MPIR_ThreadInfo.isThreaded) { pami_result_t rc; rc = PAMI_Context_lock  (MPIDI_Context[0]); MPID_assert(rc == PAMI_SUCCESS); } })
#define MPID_CS_EXIT()  ({ if (MPIR_ThreadInfo.isThreaded) { pami_result_t rc; rc = PAMI_Context_unlock(MPIDI_Context[0]); MPID_assert(rc == PAMI_SUCCESS); } })
#define MPID_CS_CYCLE() ({ pami_result_t rc; rc = PAMI_Context_advancev(MPIDI_Context, NUM_CONTEXTS, 1); MPID_assert(rc == PAMI_SUCCESS); })


#define HAVE_RUNTIME_THREADCHECK
#define MPIU_THREAD_CHECK_BEGIN if (MPIR_ThreadInfo.isThreaded) {
#define MPIU_THREAD_CHECK_END   }
#define MPIU_THREAD_CS_ENTER(_name,_context) MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT(_name,_context)  MPID_CS_EXIT()
#define MPIU_THREAD_CS_YIELD(_name,_context) MPID_CS_CYCLE()
#define MPIU_THREADSAFE_INIT_DECL(_var) static volatile int _var=1
#define MPIU_THREADSAFE_INIT_STMT(_var,_stmt)   \
    if (_var)                                   \
      {                                         \
        MPIU_THREAD_CS_ENTER(INITFLAG,);        \
        _stmt; _var=0;                          \
        MPIU_THREAD_CS_EXIT(INITFLAG,);         \
      }
#define MPIU_THREADSAFE_INIT_BLOCK_BEGIN(_var)  \
    MPIU_THREAD_CS_ENTER(INITFLAG,);            \
    if (_var)                                   \
      {
#define MPIU_THREADSAFE_INIT_CLEAR(_var) _var=0
#define MPIU_THREADSAFE_INIT_BLOCK_END(_var)    \
      }                                         \
    MPIU_THREAD_CS_EXIT(INITFLAG,)


#endif


#endif /* !MPICH_MPIDTHREAD_H_INCLUDED */
