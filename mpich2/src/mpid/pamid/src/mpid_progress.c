/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_progress.c
 * \brief Maintain the state and rules of the MPI progress semantics
 */
#include "mpidimpl.h"

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
static volatile unsigned _requests;

/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state Unused
 */
void MPID_Progress_start(MPID_Progress_state * state)
{
}


/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state Unused
 */
void MPID_Progress_end(MPID_Progress_state * state)
{
}

/**
 * \brief This function blocks until a request completes
 * \param[in] state Unused
 *
 * It does not check what has completed, only that the counter
 * changes.  That happens whenever there is a call to
 * MPIDI_Progress_signal().  It is therefore important that the ADI
 * layer include a call to MPIDI_Progress_signal() whenever something
 * occurs that a node might be waiting on.
 *
 */
int MPID_Progress_wait(MPID_Progress_state * state)
{
  pami_result_t rc;
  int x = _requests;
  while (x == _requests) {
    rc = PAMI_Context_advance(MPIDI_Context[0], 1);
    MPID_assert(rc == PAMI_SUCCESS);
  }
  return MPI_SUCCESS;
}

/**
 * \brief This function advances the connection manager.
 *
 * It gets called when progress is desired (e.g. MPI_Iprobe), but
 * nobody wants to block for it.
 */
int MPID_Progress_poke()
{
  pami_result_t rc;
  rc = PAMI_Context_advance(MPIDI_Context[0], 1);
  MPID_assert(rc == PAMI_SUCCESS);
  return MPI_SUCCESS;
}

/**
 * \brief The same as MPID_Progress_poke()
 */
int MPID_Progress_test() __attribute__((alias("MPID_Progress_poke")));

/**
 * \brief Signal MPID_Progress_wait() that something is done/changed
 *
 * It is therefore important that the ADI layer include a call to
 * MPIDI_Progress_signal() whenever something occurs that a node might
 * be waiting on.
 */
void MPIDI_Progress_signal()
{
  _requests++;
}
