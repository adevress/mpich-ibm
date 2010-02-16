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

  XMI_Client_finalize (MPIDI_Client);

  return MPI_SUCCESS;
}
