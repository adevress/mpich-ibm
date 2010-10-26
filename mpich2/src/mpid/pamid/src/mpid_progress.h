/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_progress.h
 * \brief ???
 */

#ifndef __src_mpid_progress_h__
#define __src_mpid_progress_h__


/**
 * \addtogroup MPID_PROGRESS
 * \{
 */

int  MPID_Progress_wait(MPID_Progress_state * state);
int  MPID_Progress_poke();
int  MPID_Progress_test();

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
 * is already complete.  Any ssytem *not* using this macro *must* use
 * a similar check before waiting.
 */
#define MPID_PROGRESS_WAIT_WHILE(COND)          \
({                                              \
   if (COND)                                    \
     {                                          \
       MPID_Progress_state __state;             \
       MPID_Progress_start(&__state);           \
       while (COND)                             \
         MPID_Progress_wait(&__state);          \
       MPID_Progress_end(&__state);             \
     }                                          \
    MPI_SUCCESS;                                \
})

/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state The previously seen state of advance
 */
#define MPID_Progress_start(state) ({ (*(state)).val = MPIDI_Progress_requests; })

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
#define MPIDI_Progress_signal() ({ ++MPIDI_Progress_requests; })

/** \} */


#endif
