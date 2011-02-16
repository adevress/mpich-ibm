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
  pami_result_t rc;
#ifdef USE_PAMI_COMM_THREADS
  if (likely(MPIDI_Process.comm_threads))
    {
      /** \todo Remove this hack when ticket #235 is finished */
      unsigned i;
      for (i=0; i<MPIDI_Process.avail_contexts; ++i) {
        if (unlikely(PAMI_Context_trylock(MPIDI_Context[i]) == PAMI_SUCCESS))
          {
            rc = PAMI_Context_advance(MPIDI_Context[i], 1);
            MPID_assert(rc == PAMI_SUCCESS);
            rc = PAMI_Context_unlock(MPIDI_Context[i]);
            MPID_assert(rc == PAMI_SUCCESS);
          }
      }
    }
  else
#endif
    {
      rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, loop_count);
      MPID_assert(rc == PAMI_SUCCESS);
    }
  MPIU_THREAD_CS_YIELD(ALLFUNC,);

  return MPI_SUCCESS;
}

/** \} */


#endif
