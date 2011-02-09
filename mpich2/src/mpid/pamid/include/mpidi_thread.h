/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_thread.h
 * \brief ???
 *
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "mpidi_mutex.h"


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


#if (MPICH_THREAD_LEVEL != MPI_THREAD_MULTIPLE)
#error MPICH_THREAD_LEVEL should be MPI_THREAD_MULTIPLE
#endif
#ifndef HAVE_RUNTIME_THREADCHECK
#error Need HAVE_RUNTIME_THREADCHECK
#endif



#define MPIDI_CS_ENTER(m) ({ if (MPIR_ThreadInfo.isThreaded) {                         MPIDI_Mutex_acquire(m); } })
#define MPIDI_CS_EXIT(m)  ({ if (MPIR_ThreadInfo.isThreaded) { MPIDI_Mutex_release(m);                         } })
#define MPIDI_CS_YIELD(m) ({ if (MPIR_ThreadInfo.isThreaded) { MPIDI_Mutex_release(m); MPIDI_Mutex_acquire(m); } })



#define MPIU_THREAD_CS_INIT     ({ MPIDI_Mutex_initialize(); })
#define MPIU_THREAD_CS_FINALIZE

#define MPIU_THREADSAFE_INIT_DECL(_var) static volatile int _var=1
#define MPIU_THREADSAFE_INIT_BLOCK_BEGIN(_var)  \
    MPIU_THREAD_CS_ENTER(INITFLAG,);            \
    if (_var)                                   \
      {
#define MPIU_THREADSAFE_INIT_CLEAR(_var) _var=0
#define MPIU_THREADSAFE_INIT_BLOCK_END(_var)    \
      }                                         \
    MPIU_THREAD_CS_EXIT(INITFLAG,)


#define MPIU_THREAD_CS_ENTER(name,_context) MPIU_THREAD_CS_##name##_ENTER(_context)
#define MPIU_THREAD_CS_EXIT(name,_context)  MPIU_THREAD_CS_##name##_EXIT (_context)
#define MPIU_THREAD_CS_YIELD(name,_context) MPIU_THREAD_CS_##name##_YIELD(_context)

#if MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_GLOBAL

/* There is a single, global lock, held for the duration of an MPI call */
#define MPIU_THREAD_CS_ALLFUNC_ENTER(_context)      MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_ALLFUNC_EXIT(_context)       MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_ALLFUNC_YIELD(_context)      MPIDI_CS_YIELD(0)
#define MPIU_THREAD_CS_INIT_ENTER(_context)         MPIDI_Mutex_acquire(0)
#define MPIU_THREAD_CS_INIT_EXIT(_context)          MPIDI_Mutex_release(0)

#define MPIU_THREAD_CS_CONTEXTID_ENTER(_context)
#define MPIU_THREAD_CS_CONTEXTID_EXIT(_context)
#define MPIU_THREAD_CS_CONTEXTID_YIELD(_context)    MPIDI_CS_YIELD(0)
#define MPIU_THREAD_CS_HANDLEALLOC_ENTER(_context)
#define MPIU_THREAD_CS_HANDLEALLOC_EXIT(_context)
#define MPIU_THREAD_CS_HANDLE_ENTER(_context)
#define MPIU_THREAD_CS_HANDLE_EXIT(_context)
#define MPIU_THREAD_CS_INITFLAG_ENTER(_context)
#define MPIU_THREAD_CS_INITFLAG_EXIT(_context)
#define MPIU_THREAD_CS_MEMALLOC_ENTER(_context)
#define MPIU_THREAD_CS_MEMALLOC_EXIT(_context)
#define MPIU_THREAD_CS_MPI_OBJ_ENTER(_context)
#define MPIU_THREAD_CS_MPI_OBJ_EXIT(_context)
#define MPIU_THREAD_CS_MSGQUEUE_ENTER(_context)
#define MPIU_THREAD_CS_MSGQUEUE_EXIT(_context)
#define MPIU_THREAD_CS_PAMI_ENTER(_context)
#define MPIU_THREAD_CS_PAMI_EXIT(_context)

#elif MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT

#define MPIU_THREAD_CS_ALLFUNC_ENTER(_context)
#define MPIU_THREAD_CS_ALLFUNC_EXIT(_context)
#define MPIU_THREAD_CS_ALLFUNC_YIELD(_context)
#define MPIU_THREAD_CS_INIT_ENTER(_context)
#define MPIU_THREAD_CS_INIT_EXIT(_context)

#define MPIU_THREAD_CS_CONTEXTID_ENTER(_context)    MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_CONTEXTID_EXIT(_context)     MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_CONTEXTID_YIELD(_context)    MPIDI_CS_YIELD(0)
#define MPIU_THREAD_CS_HANDLEALLOC_ENTER(_context)  MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_HANDLEALLOC_EXIT(_context)   MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_HANDLE_ENTER(_context)       MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_HANDLE_EXIT(_context)        MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_INITFLAG_ENTER(_context)     MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_INITFLAG_EXIT(_context)      MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_MEMALLOC_ENTER(_context)     MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_MEMALLOC_EXIT(_context)      MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_MPI_OBJ_ENTER(context_)      MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_MPI_OBJ_EXIT(context_)       MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_MSGQUEUE_ENTER(_context)     MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_MSGQUEUE_EXIT(_context)      MPIDI_CS_EXIT (0)
#define MPIU_THREAD_CS_PAMI_ENTER(_context)         MPIDI_CS_ENTER(0)
#define MPIU_THREAD_CS_PAMI_EXIT(_context)          MPIDI_CS_EXIT (0)

#endif /* MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_GLOBAL */


#endif /* !MPICH_MPIDTHREAD_H_INCLUDED */
