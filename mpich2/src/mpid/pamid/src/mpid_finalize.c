/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_finalize.c
 * \brief Normal job termination code
 */
#include <mpidimpl.h>

/**
 * \brief Shut down the system
 *
 * At this time, no attempt is made to free memory being used for MPI structures.
 * \return MPI_SUCCESS
*/
int MPID_Finalize()
{
  int mpierrno = MPI_SUCCESS;
  MPIR_Barrier_impl(MPIR_Process.comm_world, &mpierrno);

  /* ------------------------- */
  /* shutdown request queues   */
  /* ------------------------- */
  MPIDI_Recvq_finalize();

  PAMIX_Finalize(MPIDI_Client);

  PAMI_Context_destroyv(MPIDI_Context, MPIDI_Process.avail_contexts);

  pami_result_t rc;
  rc = PAMI_Client_destroy(&MPIDI_Client);
  MPID_assert(rc == PAMI_SUCCESS);

  return MPI_SUCCESS;
}
