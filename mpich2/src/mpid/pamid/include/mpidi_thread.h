/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_thread.h
 * \brief ???
 *
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_thread_h__
#define __include_mpidi_thread_h__

/**
 * ******************************************************************
 * \brief Mutexes for thread/interrupt safety
 * ******************************************************************
 */
/* This file is included by mpidpre.h, so it is included before mpiimplthread.h.
 * This is intentional because it lets us override the critical section macros */

#define MPID_DEVICE_DEFINES_THREAD_CS 1

//#define MPIU_ISTHREADED() ({ MPIR_ThreadInfo.isThreaded; })


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
#define MPID_CS_CYCLE() ({ pami_result_t rc; rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, 1); MPID_assert(rc == PAMI_SUCCESS); })


#ifndef HAVE_RUNTIME_THREADCHECK 
#define HAVE_RUNTIME_THREADCHECK  1
#endif
#define MPIU_THREAD_CHECK_BEGIN if (MPIR_ThreadInfo.isThreaded) {
#define MPIU_THREAD_CHECK_END   }

/*M MPIU_THREAD_CS_ENTER - Enter a named critical section

  Input Parameters:
+ _name - name of the critical section
- _context - A context (typically an object) of the critical section

M*/
#define MPIU_THREAD_CS_ENTER(_name,_context) MPIU_THREAD_CS_ENTER_##_name(_context)

/*M MPIU_THREAD_CS_EXIT - Exit a named critical section

  Input Parameters:
+ _name - cname of the critical section
- _context - A context (typically an object) of the critical section

M*/
#define MPIU_THREAD_CS_EXIT(_name,_context) MPIU_THREAD_CS_EXIT_##_name(_context)

/*M MPIU_THREAD_CS_YIELD - Temporarily release a critical section and yield
    to other threads

  Input Parameters:
+ _name - cname of the critical section
- _context - A context (typically an object) of the critical section

M*/
#define MPIU_THREAD_CS_YIELD(_name,_context) MPIU_THREAD_CS_YIELD_##_name(_context)


#if 0
#define MPIU_THREAD_CS_ENTER(_name,_context) MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT(_name,_context)  MPID_CS_EXIT()
#define MPIU_THREAD_CS_YIELD(_name,_context) MPID_CS_CYCLE()
#endif

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


/* Definitions of the thread support for various levels of thread granularity */
#if MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_GLOBAL
/* There is a single, global lock, held for the duration of an MPI call */
#define MPIU_THREAD_CS_ENTER_ALLFUNC(_context)   MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_ALLFUNC(_context)    MPID_CS_EXIT()  
#define MPIU_THREAD_CS_YIELD_ALLFUNC(_context)   MPID_CS_CYCLE()

#define MPIU_THREAD_CS_ENTER_HANDLE(_context)
#define MPIU_THREAD_CS_EXIT_HANDLE(_context)
#define MPIU_THREAD_CS_ENTER_HANDLEALLOC(_context)
#define MPIU_THREAD_CS_EXIT_HANDLEALLOC(_context)
#define MPIU_THREAD_CS_ENTER_CONTEXTID(_context)
#define MPIU_THREAD_CS_EXIT_CONTEXTID(_context) 
#define MPIU_THREAD_CS_YIELD_CONTEXTID(_context)
#define MPIU_THREAD_CS_ENTER_DCMF(_context)
#define MPIU_THREAD_CS_EXIT_DCMF(_context) 
#define MPIU_THREAD_CS_YIELD_DCMF(_context)
#define MPIU_THREAD_CS_ENTER_RECVQ(_context)
#define MPIU_THREAD_CS_EXIT_RECVQ(_context) 
#define MPIU_THREAD_CS_ENTER_PAMI(_context)
#define MPIU_THREAD_CS_EXIT_PAMI(_context) 
#define MPIU_THREAD_CS_ENTER_MPI_OBJ(_context)
#define MPIU_THREAD_CS_EXIT_MPI_OBJ(_context)
#define MPIU_THREAD_CS_ENTER_INIT(_context)
#define MPIU_THREAD_CS_EXIT_INIT(_context) 
#define MPIU_THREAD_CS_ENTER_INITFLAG(_context)
#define MPIU_THREAD_CS_EXIT_INITFLAG(_context) 


/* MPIU_THREAD_CS_INIT will be invoked early in the top level
 * MPI_Init/MPI_Init_thread process.  It is responsible for performing any
 * initialization for the MPIU_THREAD_CS_ macros, as well as invoking
 * MPIU_THREADPRIV_INITKEY and MPIU_THREADPRIV_INIT.
 *
 * MPIR_ThreadInfo.isThreaded will be set prior to this macros invocation, based
 * on the "provided" value to MPI_Init_thread
 */
#define MPIU_THREAD_CS_INIT     MPIR_Thread_CS_Init()     /* in src/mpi/init/initthread.c */
#define MPIU_THREAD_CS_FINALIZE MPIR_Thread_CS_Finalize() /* in src/mpi/init/initthread.c */

//Locks setup to make this look like the old brief global
#elif MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT

#define MPIU_THREAD_CS_ENTER_ALLFUNC(_context)
#define MPIU_THREAD_CS_EXIT_ALLFUNC(_context)
#define MPIU_THREAD_CS_YIELD_ALLFUNC(_context) 

#define MPIU_THREAD_CS_ENTER_HANDLE(_context)       MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_HANDLE(_context)        MPID_CS_EXIT()  
#define MPIU_THREAD_CS_ENTER_HANDLEALLOC(_context)  MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_HANDLEALLOC(_context)   MPID_CS_EXIT() 
#define MPIU_THREAD_CS_ENTER_CONTEXTID(_context)    MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_CONTEXTID(_context)     MPID_CS_EXIT() 
#define MPIU_THREAD_CS_YIELD_CONTEXTID(_context)    MPID_CS_CYCLE()
#define MPIU_THREAD_CS_ENTER_RECVQ(_context)        MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_RECVQ(_context) 	    MPID_CS_EXIT() 
#define MPIU_THREAD_CS_ENTER_PAMI(_context)         MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_PAMI(_context) 	    MPID_CS_EXIT() 
#define MPIU_THREAD_CS_ENTER_MPI_OBJ(context_)      MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_MPI_OBJ(context_)  	    MPID_CS_EXIT() 
#define MPIU_THREAD_CS_ENTER_INIT(_context)         MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_INIT(_context) 	    MPID_CS_EXIT() 
#define MPIU_THREAD_CS_ENTER_INITFLAG(_context)     MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT_INITFLAG(_context) 	    MPID_CS_EXIT() 


/* MPIU_THREAD_CS_INIT will be invoked early in the top level
 * MPI_Init/MPI_Init_thread process.  It is responsible for performing any
 * initialization for the MPIU_THREAD_CS_ macros, as well as invoking
 * MPIU_THREADPRIV_INITKEY and MPIU_THREADPRIV_INIT.
 *
 * MPIR_ThreadInfo.isThreaded will be set prior to this macros invocation, based
 * on the "provided" value to MPI_Init_thread
 */
#define MPIU_THREAD_CS_INIT     MPIR_Thread_CS_Init()     /* in src/mpi/init/initthread.c */
#define MPIU_THREAD_CS_FINALIZE MPIR_Thread_CS_Finalize() /* in src/mpi/init/initthread.c */

#endif // MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_GLOBAL

#endif // (MPICH_THREAD_LEVEL != MPI_THREAD_MULTIPLE)

#endif /* !MPICH_MPIDTHREAD_H_INCLUDED */
