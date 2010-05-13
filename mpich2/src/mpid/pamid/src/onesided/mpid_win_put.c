/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_put.c
 * \brief ???
 */
#include "mpidi_onesided.h"


/**
 * \brief MPI-PAMI glue for MPI_PUT function
 *
 * Put \e origin_count number of \e origin_datatype from \e origin_addr
 * to node \e target_rank into \e target_count number of \e target_datatype
 * into window location \e target_disp offset (window displacement units)
 *
 * \param[in] origin_addr	Source buffer
 * \param[in] origin_count	Number of datatype elements
 * \param[in] origin_datatype	Source datatype
 * \param[in] target_rank	Destination rank (target)
 * \param[in] target_disp	Displacement factor in target buffer
 * \param[in] target_count	Number of target datatype elements
 * \param[in] target_datatype	Destination datatype
 * \param[in] win_ptr		Window
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC, or error returned from
 *	MPIR_Localcopy, MPID_Segment_init, or PAMI_Rput.
 *
 * \ref msginfo_usage\n
 * \ref put_design
 */
int
MPID_Put(void         *origin_addr,
         int           origin_count,
         MPI_Datatype  origin_datatype,
         int           target_rank,
         MPI_Aint      target_disp,
         int           target_count,
         MPI_Datatype  target_datatype,
         MPID_Win     *win_ptr)
{
  int mpi_errno = MPI_SUCCESS;



  return mpi_errno;
}
