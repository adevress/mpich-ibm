/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/onesided/mpid_win_fence.c
 * \brief MPI-DCMF MPI_Win_fence functionality
 */

#include "mpid_onesided.h"

/**
 * \page fence_design MPID_Win_fence Design
 *
 * A fence / RMA / fence sequence is as follows:
 *
 * The following assumes that all nodes are in-sync with respect to
 * synchronization primitives. If not, nodes will fail their
 * synchronization calls as appropriate.
 *
 * <B>All nodes in window communicator call MPI_Win_fence</B>
 *
 * \e ASSUMPTION: Each node's window is in state MPID_EPOTYPE_NONE.
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to enter a
 * \e FENCE access/exposure epoch. These checks include testing that
 * no other epoch is currently in affect.
 * - If MPI_MODE_NOSUCCEED is asserted, fail with MPI_ERR_RMA_SYNC.
 * - If the local window is currenty locked then wait for the
 * lock to be released (calling advance in the loop).
 * - Set epoch type to MPID_EPOTYPE_FENCE, epoch size to window
 * communicator size, and save MPI_MODE_* assertions.
 * - Call NMPI_Barrier on the window communicator to wait for all
 * nodes to reach this point.
 *
 * <B>One or more nodes invoke RMA operation(s)</B>
 *
 * - See respective RMA operation calls for details
 *
 * <B>All nodes in window communicator call MPI_Win_fence</B>
 *
 * \e ASSUMPTION: Each node's window is in state MPID_EPOTYPE_FENCE.
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to end a
 * \e FENCE access/exposure epoch. These checks include testing that
 * a \e FENCE epoch is currently in affect.
 * - If MPI_MODE_NOPRECEDE is asserted, fail with MPI_ERR_RMA_SYNC.
 * - \e MPID_assert_debug that the local window is not locked.
 * - Call NMPI_Allreduce on the window communicator to sum
 * the \e rma_sends
 * fields in the collective info array. This also waits for all
 * nodes to reach this point. This operation provides each node with
 * the total number of RMA operations that other nodes sent to it.
 * - wait for the number of RMA operations received to equal the
 * number of RMA operations that were sent to us, calling advance
 * in the loop.
 * - Reset epoch information in window to indicate the epoch has ended.
 */
/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_fence
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_FENCE function
 *
 * Begin or end an access/exposure epoch on nodes in the window communicator.
 * If begin,
 *	- perform a Barrier until all other nodes pass through the MPI_Win_fence.
 * If end:
 *	- perform an Allreduce on the rma_sends element of the collective info array.
 *	- while rma_sends (to us) > rma_recvs call advance.
 *
 * \param[in] assert	Synchronization hints
 * \param[in] win_ptr	Window
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC,  or error returned from
 *	NMPI_Allreduce or NMPI_Barrier.
 *
 * \ref fence_design
 */
int MPID_Win_fence(int assert, MPID_Win *win_ptr)
{
	int mpi_errno = MPI_SUCCESS;
	int rank = win_ptr->_dev.comm_ptr->rank;
	MPIU_THREADPRIV_DECL;
	MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_FENCE);

	MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_FENCE);
	MPIU_THREADPRIV_GET;
	MPIR_Nest_incr();

	/*
	 * FENCE must always (effectively) barrier, so since we don't know
	 * what other ranks were doing we must always perform the ALLREDUCE
	 * and use it to both barrier and compute outstanding RMAs (even if
	 * there are none). We also must wait for outstanding RMAs, although
	 * that should be NOOP if there were no RMAs performed (under the fence).
	 */
	if (win_ptr->_dev.as_origin.epoch_type != win_ptr->_dev.as_target.epoch_type) {
		/* --BEGIN ERROR HANDLING-- */
		MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
				goto fn_fail, "**rmasync");
		/* --END ERROR HANDLING-- */
		/* should be able to assert that no RMAs are pending... */
	}
	if ((assert & MPI_MODE_NOPRECEDE) &&
			win_ptr->_dev.as_origin.epoch_type != MPID_EPOTYPE_NONE) {
		/* --BEGIN ERROR HANDLING-- */
		MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
				goto fn_fail, "**rmasync");
		/* --END ERROR HANDLING-- */
		/* should be able to assert that no RMAs are pending... */
	}
	if (!(assert & MPI_MODE_NOPRECEDE) &&
			win_ptr->_dev.as_origin.epoch_type != MPID_EPOTYPE_FENCE &&
			win_ptr->_dev.as_origin.epoch_type != MPID_EPOTYPE_REFENCE &&
			win_ptr->_dev.as_origin.epoch_type != MPID_EPOTYPE_NONE) {
		/* --BEGIN ERROR HANDLING-- */
		MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
				goto fn_fail, "**rmasync");
		/* --END ERROR HANDLING-- */
	}

	mpi_errno = NMPI_Allreduce((void *)win_ptr->_dev.as_origin.rmas,
				(void *)win_ptr->_dev.as_target.rmas,
				MPIDU_comm_size(win_ptr),
				MPI_INT, MPI_SUM,
				win_ptr->_dev.comm_ptr->handle);
	if (mpi_errno) {
		char buf[MPI_MAX_ERROR_STRING];
		int buf_len;
		PMPI_Error_string(mpi_errno, buf, &buf_len);
		if (1) fprintf(stderr, "%d: MPID_Win_fence failed NMPI_Allreduce: %s\n", mpid_my_lpid, buf);
		MPIU_ERR_POP(mpi_errno);
	}
	MPID_assert_debug(MPIDU_is_lock_free(win_ptr));
	MPIDU_Progress_spin(win_ptr->_dev.my_get_pends > 0 ||
				win_ptr->_dev.my_rma_pends > 0 ||
				win_ptr->_dev.my_rma_recvs <
					win_ptr->_dev.as_target.rmas[rank]);
	if ((win_ptr->_dev.as_origin.epoch_assert & MPI_MODE_NOPUT) &&
				win_ptr->_dev.my_rma_recvs > 0) {
		/* TBD: handled earlier? */
	}
	win_ptr->_dev.epoch_rma_ok = 0;
	epoch_clear(win_ptr, -1, 0);
	epoch_clear(win_ptr, -1, 1);
	epoch_end_cb(win_ptr); /* might start exposure epoch via LOCK... */

	if (!(assert & MPI_MODE_NOSUCCEED)) {
		/* this may imply a new epoch following... */

		MPIDU_Spin_lock_free(win_ptr);
		win_ptr->_dev.as_origin.epoch_type =
			win_ptr->_dev.as_target.epoch_type = MPID_EPOTYPE_REFENCE;
		win_ptr->_dev.as_origin.epoch_size =
			win_ptr->_dev.as_target.epoch_size = MPIDU_comm_size(win_ptr);
		win_ptr->_dev.as_origin.epoch_assert =
			win_ptr->_dev.as_target.epoch_assert = assert;
		win_ptr->_dev.as_origin.sync_count =
			win_ptr->_dev.as_target.sync_count = 0;	// not used
		win_ptr->_dev.epoch_rma_ok = 1;
		win_ptr->_dev.my_rma_recvs = 0;
		/* wait for everyone else to reach this point */
		mpi_errno = NMPI_Barrier(win_ptr->_dev.comm_ptr->handle);
		if (mpi_errno) { MPIU_ERR_POP(mpi_errno); }
		if (win_ptr->_dev.as_origin.epoch_assert & MPI_MODE_NOSTORE) {
			/* TBD: anything to optimize? */
		}
		if (win_ptr->_dev.as_origin.epoch_assert & MPI_MODE_NOPUT) {
			/* handled later */
		}
	} else {
		/* already done by epoch_clear()... */
		win_ptr->_dev.as_origin.epoch_type =
			win_ptr->_dev.as_target.epoch_type = MPID_EPOTYPE_NONE;
	}

fn_exit:
	MPIR_Nest_decr();
	MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_FENCE);
	return mpi_errno;
	/* --BEGIN ERROR HANDLING-- */
fn_fail:
	goto fn_exit;
	/* --END ERROR HANDLING-- */
}
