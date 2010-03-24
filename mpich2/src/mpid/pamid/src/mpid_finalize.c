/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_finalize.c
 * \brief Normal job termination code
 */
#include "mpidimpl.h"

/**
 * \brief Shut down the system
 *
 * At this time, no attempt is made to free memory being used for MPI structures.
 * \return MPI_SUCCESS
*/
int MPID_Finalize()
{
  /** \todo Remove this when #72 is fixed */
  MPIR_ThreadInfo.isThreaded = 0;


  MPID_Comm * comm;
  MPID_Comm_get_ptr(MPI_COMM_WORLD, comm);

  PMPI_Barrier(MPI_COMM_WORLD);

  /* ------------------------- */
  /* shutdown request queues   */
  /* ------------------------- */
  MPIDI_Recvq_finalize();

  PAMIX_Context_destroy(MPIDI_Context, NUM_CONTEXTS);

  pami_result_t rc;
  rc = PAMI_Client_finalize(MPIDI_Client);
  MPID_assert(rc == PAMI_SUCCESS);

  return MPI_SUCCESS;
}
