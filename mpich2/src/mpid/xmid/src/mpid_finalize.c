/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/misc/mpid_finalize.c
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
  MPID_Comm * comm;
  MPID_Comm_get_ptr(MPI_COMM_WORLD, comm);

  PMPI_Barrier(MPI_COMM_WORLD);

  /* ------------------------- */
  /* shutdown request queues   */
  /* ------------------------- */
  MPIDI_Recvq_finalize();

  XMIX_Context_destroy(MPIDI_Context, NUM_CONTEXTS);

  xmi_result_t rc;
  rc = XMI_Client_finalize(MPIDI_Client);
  MPID_assert(rc == XMI_SUCCESS);

  return MPI_SUCCESS;
}
