/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_accumulate.c
 * \brief ???
 */
#include "mpidi_onesided.h"
/**
 * \brief MPI-PAMI glue for MPI_ACCUMULATE function
 *
 * Perform DEST = DEST (op) SOURCE for \e origin_count number of
 * \e origin_datatype at \e origin_addr
 * to node \e target_rank into \e target_count number of \e target_datatype
 * into window location \e target_disp offset (window displacement units)
 *
 * According to the MPI Specification:
 *
 *	Each datatype argument must be a predefined datatype or
 *	a derived datatype, where all basic components are of the
 *	same predefined datatype. Both datatype arguments must be
 *	constructed from the same predefined datatype.
 *
 * \param[in] origin_addr	Source buffer
 * \param[in] origin_count	Number of datatype elements
 * \param[in] origin_datatype	Source datatype
 * \param[in] target_rank	Destination rank (target)
 * \param[in] target_disp	Displacement factor in target buffer
 * \param[in] target_count	Number of target datatype elements
 * \param[in] target_datatype	Destination datatype
 * \param[in] op		Operand to perform
 * \param[in] win_ptr		Window
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC, MPI_ERR_OP,
 *	or error returned from MPIR_Localcopy, MPID_Segment_init,
 *	mpid_queue_datatype, or PAMI_Raccumulate.
 *
 * \ref msginfo_usage\n
 * \ref accum_design
 */
int
MPID_Accumulate(void         *origin_addr,
                int           origin_count,
                MPI_Datatype  origin_datatype,
                int           target_rank,
                MPI_Aint      target_disp,
                int           target_count,
                MPI_Datatype  target_datatype,
                MPI_Op        op,
                MPID_Win     *win_ptr)
{
  return 1;
}
