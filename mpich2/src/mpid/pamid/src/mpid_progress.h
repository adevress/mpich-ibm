/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_progress.h
 * \brief ???
 */

#ifndef __src_mpid_progress_h__
#define __src_mpid_progress_h__


/**
 * \defgroup MPID_PROGRESS MPID Progress engine
 *
 * Maintain the state and rules of the MPI progress semantics
 *
 * \addtogroup MPID_PROGRESS
 * \{
 */


/** \brief The same as MPID_Progress_wait(), since it does not block */
#define MPID_Progress_test() MPID_Progress_wait_inline(1)
/** \brief The same as MPID_Progress_wait(), since it does not block */
#define MPID_Progress_poke() MPID_Progress_wait_inline(1)


/**
 * \brief A macro to easily implement advancing until a specific
 * condition becomes false.
 *
 * \param[in] COND This is not a true parameter.  It is *specifically*
 * designed to be evaluated several times, allowing for the result to
 * change.  The condition would generally look something like
 * "(cb.client == 0)".  This would be used as the condition on a while
 * loop.
 *
 * \returns MPI_SUCCESS
 *
 * This correctly checks the condition before attempting to loop,
 * since the call to MPID_Progress_wait() may not return if the event
 * is already complete.  Any system *not* using this macro *must* use
 * a similar check before waiting.
 */
#define MPID_PROGRESS_WAIT_WHILE(COND)          \
({                                              \
  while (COND)                                  \
    MPID_Progress_wait(&__state);               \
  MPI_SUCCESS;                                  \
})


/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state The previously seen state of advance
 */
#define MPID_Progress_start(state)

/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state The previously seen state of advance
 */
#define MPID_Progress_end(state)

/**
 * \brief Signal MPID_Progress_wait() that something is done/changed
 *
 * It is therefore important that the ADI layer include a call to
 * MPIDI_Progress_signal() whenever something occurs that a node might
 * be waiting on.
 */
#define MPIDI_Progress_signal()


#define MPID_Progress_wait(state) MPID_Progress_wait_inline(100)


void MPIDI_Progress_init();
void MPIDI_Progress_async_start(pami_context_t context, void *cookie);
void MPIDI_Progress_async_end  (pami_context_t context, void *cookie);
void MPIDI_Progress_async_poll ();


/**
 * \brief This function blocks until a request completes
 * \param[in] state The previously seen state of advance
 *
 * It does not check what has completed, only that the counter
 * changes.  That happens whenever there is a call to
 * MPIDI_Progress_signal().  It is therefore important that the ADI
 * layer include a call to MPIDI_Progress_signal() whenever something
 * occurs that a node might be waiting on.
 *
 */
static inline int
MPID_Progress_wait_inline(unsigned loop_count)
{
  pami_result_t rc = 0;
#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
  if (unlikely(MPIDI_Process.async_progress_active == 0)) {
    /* This just assumes that people will want the thread-safe version when using the per-obj code. */
    rc = PAMI_Context_trylock_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, 1);
    MPID_assert( (rc == PAMI_SUCCESS) || (rc == PAMI_EAGAIN) );
  }
#else
  rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, loop_count);
  MPID_assert( (rc == PAMI_SUCCESS) || (rc == PAMI_EAGAIN) );
#ifdef MPIDI_SINGLE_CONTEXT_ASYNC_PROGRESS
  if (rc == PAMI_EAGAIN) {
       MPIDI_CS_SCHED_YIELD(0);
  } else
#endif
  MPIU_THREAD_CS_YIELD(ALLFUNC,);
#endif

  return MPI_SUCCESS;
}

/** \} */


#endif
