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
#define HAVE_RUNTIME_THREADCHECK
#if 0
/**
 * ******************************************************************
 * \brief Mutexes for thread/interrupt safety
 * ******************************************************************
 */

/* This file is included by mpidpre.h, so it is included before mpiimplthread.h.
 * This is intentional because it lets us override the critical section macros */

#if (MPICH_THREAD_LEVEL != MPI_THREAD_MULTIPLE)


#error MPICH_THREAD_LEVEL should be MPI_THREAD_MULTIPLE


#else


/* suppress default macro definitions */
#define MPID_DEVICE_DEFINES_THREAD_CS 1

/* FIXME this is set/unset by the MPICH2 top level configure, shouldn't be
 * defined here as well... */
#define HAVE_RUNTIME_THREADCHECK

/************** BEGIN PUBLIC INTERFACE ***************/

/* assumes an MPIU_THREADPRIV_DECL is present in an enclosing scope */
#define MPIU_THREAD_CS_INIT         \
    do {                            \
        MPIU_THREADPRIV_INITKEY;    \
        MPIU_THREADPRIV_INIT;       \
    } while (0)
#define MPIU_THREAD_CS_FINALIZE  do{}while(0)

/* definitions for main macro maze entry/exit */
#define MPIU_THREAD_CS_ENTER(_name,_context) MPID_CS_ENTER()
#define MPIU_THREAD_CS_EXIT(_name,_context)  MPID_CS_EXIT()
#define MPIU_THREAD_CS_YIELD(_name,_context) MPID_CS_CYCLE()

/* see FIXME in mpiimplthread.h if you are hacking on THREADSAFE_INIT code */
#define MPIU_THREADSAFE_INIT_DECL(_var) static volatile int _var=1
#define MPIU_THREADSAFE_INIT_STMT(_var,_stmt)   \
    do {                                        \
        if (_var) {                             \
            MPIU_THREAD_CS_ENTER(INITFLAG,);    \
            _stmt;                              \
            _var=0;                             \
            MPIU_THREAD_CS_EXIT(INITFLAG,);     \
        }                                       \
    while (0)
#define MPIU_THREADSAFE_INIT_BLOCK_BEGIN(_var)  \
    MPIU_THREAD_CS_ENTER(INITFLAG,);            \
    if (_var) {
#define MPIU_THREADSAFE_INIT_CLEAR(_var) _var=0
#define MPIU_THREADSAFE_INIT_BLOCK_END(_var)    \
    }                                           \
    MPIU_THREAD_CS_EXIT(INITFLAG,)

/************** END PUBLIC INTERFACE ***************/
/* everything that follows is just implementation details */


/* definitions for main macro maze entry/exit */
#define MPID_CS_ENTER() ({ if (MPIR_ThreadInfo.isThreaded) { pami_result_t rc; rc = PAMI_Context_lock  (MPIDI_Context[0]); MPID_assert(rc == PAMI_SUCCESS); } })
#define MPID_CS_EXIT()  ({ if (MPIR_ThreadInfo.isThreaded) { pami_result_t rc; rc = PAMI_Context_unlock(MPIDI_Context[0]); MPID_assert(rc == PAMI_SUCCESS); } })
#define MPID_CS_CYCLE() ({ pami_result_t rc; rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, 1); MPID_assert(rc == PAMI_SUCCESS); })


#endif

#endif /* #if 0 */
#endif /* !MPICH_MPIDTHREAD_H_INCLUDED */
