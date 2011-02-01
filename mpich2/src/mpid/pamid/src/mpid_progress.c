/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_progress.c
 * \brief Maintain the state and rules of the MPI progress semantics
 */
#include <mpidimpl.h>

/**
 * \defgroup MPID_PROGRESS MPID Progress engine
 *
 * Maintain the state and rules of the MPI progress semantics
 */


/**
 * \brief A counter to allow the detection of changes to message state.
 *
 * It is theoretically possible to miss an event: if exactly 2^32 (4
 * billion) events complete in a single call to PAMI_Context_advance(),
 * the comparison would still be true.  We assume that this will not
 * happen.
 */
unsigned MPIDI_Progress_requests;

/**
 * \brief This function advances the connection manager.
 *
 * It gets called when progress is desired (e.g. MPI_Iprobe), but
 * nobody wants to block for it.
 */
int MPID_Progress_poke()
{
  pami_result_t rc;
  rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, 1);
  MPID_assert(rc == PAMI_SUCCESS);
  return MPI_SUCCESS;
}

/**
 * \brief The same as MPID_Progress_poke()
 */
int MPID_Progress_test() __attribute__((alias("MPID_Progress_poke")));
