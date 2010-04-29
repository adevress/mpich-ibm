/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/onesided/mpid_win_pscw.c
 * \brief MPI-DCMF MPI_Win_post/start/complete/wait(test) functionality
 */

#include "mpid_onesided.h"

/**
 * \brief convert a group rank to comm rank
 *
 * \param[in] win	MPID_Win object
 * \param[in] vc	virtual channel table
 * \param[in] grp	MPID_Group object
 * \param[in] grank	Rank within grp to convert
 * \param[out] lpidp	Optional return of lpid
 * \return	Rank within Window communicator
 */
static int mpidu_group_to_comm(MPID_Win *win, MPID_VCR *vc, MPID_Group *grp, int grank, int *lpidp) {
	int z;

	int comm_size = MPIDU_comm_size(win);
	int lpid = grp->lrank_to_lpid[grank].lpid;
	/* convert group rank to comm rank */
	for (z = 0; z < comm_size && lpid != vc[z]; ++z);
	MPID_assert_debug(z < comm_size);
	if (lpidp) *lpidp = lpid;
	return z;
}

/**
 * \brief Send (spray) a protocol message to a group of nodes.
 *
 * Send a protocol message to all members of a group (or the
 * window-comm if no group).
 *
 * Currently, this routine will only be called once per group
 * (i.e. once during an exposure or access epoch). If it ends
 * up being called more than once, it might make sense to build
 * a translation table between the group rank and the window
 * communicator rank.  Or if we can determine that the same
 * group is being used in multiple, successive, epochs. In practice,
 * it takes more work to build a translation table than to lookup
 * ranks ad-hoc.
 *
 * \param[in] win	Pointer to MPID_Win object
 * \param[in] grp	Optional pointer to MPID_Group object
 * \param[in] type	Type of message (MPID_MSGTYPE_*)
 * \return MPI_SUCCESS or error returned from DCMF_Send.
 *
 * \ref msginfo_usage
 */
static int mpidu_proto_send(MPID_Win *win, MPID_Group *grp, int type) {
        int lpid, x;
        MPIDU_Onesided_ctl_t ctl;
        int size, comm_rank;
        int mpi_errno = MPI_SUCCESS;
        MPID_VCR *vc;
        DCMF_Consistency consistency = win->_dev.my_cstcy;
	MPID_assert_debug(grp != NULL);

        /*
         * \todo Confirm this:
         * For inter-comms, we only talk to the remote nodes. For
         * intra-comms there are no remote or local nodes.
         * So, we always use win->_dev.comm_ptr->vcr (?)
         * However, we have to choose remote_size, in the case of
         * inter-comms, vs. local_size. This decision also
         * affects MPIDU_world_rank_c().
         */
        vc = MPIDU_world_vcr(win);
        MPID_assert_debug(vc != NULL);
        size = grp->size;
        /** \todo is it OK to lower consistency here? */
        consistency = DCMF_RELAXED_CONSISTENCY;

        ctl.mpid_ctl_w0 = type;
        ctl.mpid_ctl_w2 = win->_dev.comm_ptr->rank;
        for (x = 0; x < size; ++x) {
		comm_rank = mpidu_group_to_comm(win, vc, grp, x, &lpid);
                ctl.mpid_ctl_w1 = win->_dev.coll_info[comm_rank].win_handle;
                if (type == MPID_MSGTYPE_COMPLETE) {
			/* tell target how many RMAs we did */
                        ctl.mpid_ctl_w3 = win->_dev.as_origin.rmas[comm_rank];
                        win->_dev.as_origin.rmas[comm_rank] = 0;
                        win->_dev.post_counts[comm_rank] = 0;
                }
		if (lpid == mpid_my_lpid) {
			/* send to self, just process as if received */
			recv_sm_cb(NULL, (const DCQuad *)&ctl, 1, lpid, NULL, 0);
		} else {
                	mpi_errno = DCMF_Control(&bg1s_ct_proto, consistency, lpid, &ctl.ctl);
                	if (mpi_errno) { break; }
		}
        }
        return mpi_errno;
}

/**
 * \brief Test if MPI_Win_post exposure epoch has ended
 *
 * Must call advance at least once per call. Tests if all
 * MPID_MSGTYPE_COMPLETE messages have been received, and whether
 * all RMA ops that were sent have been received. If epoch does end,
 * must cleanup all structures, counters, flags, etc.
 *
 * Assumes that MPID_Progress_start() and MPID_Progress_end() bracket
 * this call, in some meaningful fashion.
 *
 * \param[in] win	Window
 * \return	TRUE if epoch has ended
 */
static int mpid_check_post_done(MPID_Win *win) {
        int rank = win->_dev.comm_ptr->rank;

        (void)MPID_Progress_test();
        if (!(win->_dev.as_target.epoch_assert & MPI_MODE_NOCHECK) &&
            (win->_dev.as_target.sync_count < win->_dev.as_target.epoch_size ||
                        win->_dev.my_rma_recvs < win->_dev.as_target.rmas[rank])) {
                return 0;
        }
	win->_dev.epoch_rma_ok = 0;
	epoch_clear(win, -1, 1);
        /*
         * Any RMA ops we initiated would be handled in a
         * Win_start/Win_complete epoch and that would have
         * zeroed our target rma_sends.
         */
        epoch_end_cb(win);
        return 1;
}

/**
 * \brief Determine if all necessary group members have sent us a POST message
 *
 * \param[in] win	Window object
 * \return	1 if all POSTs received, 0 if not
 */
static int mpid_total_posts_recv(MPID_Win *win) {
	int x, z;
	MPID_VCR *vc = MPIDU_world_vcr(win);
	MPID_Group *grp = win->start_group_ptr;
	for (x = 0; x < grp->size; ++x) {
		z = mpidu_group_to_comm(win, vc, grp, x, NULL);
		if (win->_dev.post_counts[z] == 0) {
			return 0;
		}
	}
	/* post_counts[] are cleared during COMPLETE sends */
	return 1;
}

/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_complete
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_COMPLETE function
 *
 * End the access epoch began by MPID_Win_start.
 * Sends a MPID_MSGTYPE_COMPLETE message, containing our count of
 * RMA operations sent to that node, to all nodes in the group.
 *
 * \param[in] win_ptr		Window
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC, or error returned from
 *	mpidu_proto_send.
 *
 * \ref post_design
 */
int MPID_Win_complete(MPID_Win *win_ptr)
{
        unsigned pending;
        int mpi_errno = MPI_SUCCESS;
        MPIU_THREADPRIV_DECL;
        MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_COMPLETE);

        MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_COMPLETE);
        MPIU_THREADPRIV_GET;
        MPIR_Nest_incr();

        if (win_ptr->_dev.as_origin.epoch_type != MPID_EPOTYPE_START) {
                /* --BEGIN ERROR HANDLING-- */
                MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                                        goto fn_fail, "**rmasync");
                /* --END ERROR HANDLING-- */
        }
        /*
         * MPICH2 says that we cannot return until all RMA ops
         * have completed at the origin (i.e. been sent).
	 *
	 * Since mpidu_proto_send() uses DCMF_Control(), there
	 * are no pending sends to wait for.
         */
        MPIDU_Progress_spin(win_ptr->_dev.my_rma_pends > 0 ||
			win_ptr->_dev.my_get_pends > 0);
        epoch_clear(win_ptr, -1, 0);

        if (!(win_ptr->_dev.as_origin.epoch_assert & MPI_MODE_NOCHECK)) {
                /* This zeroes the respective rma_sends counts...  */
                mpi_errno = mpidu_proto_send(win_ptr, win_ptr->start_group_ptr,
                        MPID_MSGTYPE_COMPLETE);
                if (mpi_errno) { MPIU_ERR_POP(mpi_errno); }
        }
        win_ptr->start_assert = 0;
        MPIU_Object_release_ref(win_ptr->start_group_ptr, &pending);
        win_ptr->start_group_ptr = NULL;
        epoch_end_cb(win_ptr);

fn_exit:
        MPIR_Nest_decr();
        MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_COMPLETE);
        return mpi_errno;

        /* --BEGIN ERROR HANDLING-- */
fn_fail:
        goto fn_exit;
        /* --END ERROR HANDLING-- */
}

/**
 * \page post_design MPID_Win_post-start / complete-wait Design
 *
 * MPID_Win_post and MPID_Win_start take a group object. In
 * each case this group object excludes the calling node.
 * Each node calling MPID_Win_post or MPID_Win_start may
 * specify a different group, however all nodes listed in a
 * MPID_Win_start group must call MPID_Win_post, and vice versa.
 * A node may call both MPID_Win_post and MPID_Win_start, but
 * only in that order. Likewise, MPID_Win_complete and MPID_Win_wait
 * (or MPID_Win_test) must be called in that order.
 *
 * A post-start / RMA / complete-wait sequence is as follows:
 *
 * The following assumes that all nodes are in-sync with respect to
 * synchronization primitives. If not, nodes will fail their
 * synchronization calls as appropriate.
 *
 * <B>All nodes permitting RMA access call MPI_Win_post</B>
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to enter a
 * \e POST access epoch. These checks include testing that
 * no other epoch is currently in affect.
 * - If the local window is currenty locked then wait for the
 * lock to be released (calling advance in the loop).
 * - Set epoch type to MPID_EPOTYPE_POST, epoch size to group size,
 * RMA OK flag to TRUE, and save MPI_MODE_* assertions.
 * - If MPI_MODE_NOCHECK is set, return MPI_SUCCESS.
 * - Send MPID_MSGTYPE_POST protocol messages to all nodes in group.
 * - Wait for all sends to complete (not for receives to complete).
 *
 * <B>All nodes intending to do RMA access call MPI_Win_start</B>
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to enter a
 * \e START access epoch. These checks include testing that
 * either a \e POST epoch or no epoch is currently in affect.
 * - If the current epoch is MPID_EPOTYPE_NONE and the
 * local window is currenty locked then wait for the
 * lock to be released (calling advance in the loop).
 * - Set epoch type to MPID_EPOTYPE_START or MPID_EPOTYPE_POSTSTART
 * and save MPI_MODE_* \e start assertions.
 * - Take a reference on the group and save the \e start group (pointer).
 * - If MPI_MODE_NOCHECK is set then return MPI_SUCCESS now.
 * - Wait for MPID_MSGTYPE_POST messages to be received from
 * all nodes in group.
 *
 * <B>One or more nodes invoke RMA operation(s)</B>
 *
 * - See respective RMA operation calls for details
 *
 * <B>All nodes that intended to do RMA access call MPI_Win_complete</B>
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to end a
 * \e START access epoch. These checks include testing that
 * either a \e START epoch or \e POSTSTART epoch is currently in affect.
 * - Send MPID_MSGTYPE_COMPLETE messages to all nodes in the group.
 * These messages include the count of RMA operations that were sent.
 * - Wait for all messages to send, including RMA operations that
 * had not previously gone out (MPICH2 requirement).
 * Call advance in the loop.
 * - Reset epoch info as appropriate, to state prior to MPID_Win_start
 * call (i.e. epoch type either MPID_EPOTYPE_POST or MPID_EPOTYPE_NONE).
 * - Release reference on group.
 *
 * <B>All nodes that allowed RMA access call MPI_Win_wait</B>
 *
 * MPID_Win_wait and MPID_Win_test are interchangeable, for the sake
 * of this discussion.
 *
 * - A sanity-check is done to
 * ensure that the window is in a valid state to end a
 * \e POST access epoch. These checks include testing that
 * a \e POST epoch is currently in affect.
 * - Call advance.
 * - Test if MPID_MSGTYPE_COMPLETE messages have been received from
 * all nodes in group, and that all RMA operations sent to us have been
 * received by us. MPID_Win_test will return FALSE under this condition,
 * while MPID_Win_wait will loop back to the "Call advance" step.
 * - Reset epoch info to indicate the \e POST epoch has ended.
 * - Return TRUE (MPI_SUCCESS).
 */
/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_start
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_START function
 *
 * Begin an access epoch for nodes in group.
 * Waits for all nodes in group to send us a MPID_MSGTYPE_POST message.
 *
 * \param[in] group_ptr		Group
 * \param[in] assert		Synchronization hints
 * \param[in] win_ptr		Window
 * \return MPI_SUCCESS or MPI_ERR_RMA_SYNC.
 *
 * \todo In the NOCHECK case, do we still need to Barrier?
 *
 * \ref post_design
 */
int MPID_Win_start(MPID_Group *group_ptr, int assert, MPID_Win *win_ptr)
{
        int mpi_errno = MPI_SUCCESS;
        MPIU_THREADPRIV_DECL;
        MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_START);

        MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_START);
        MPIU_THREADPRIV_GET;
        MPIR_Nest_incr();
        if (win_ptr->_dev.as_origin.epoch_type == MPID_EPOTYPE_NONE ||
			win_ptr->_dev.as_origin.epoch_type == MPID_EPOTYPE_REFENCE) {
                MPIDU_Spin_lock_free(win_ptr);
                win_ptr->_dev.as_origin.epoch_type = MPID_EPOTYPE_START;
                win_ptr->_dev.as_origin.epoch_size = group_ptr->size;
        } else {
                /* --BEGIN ERROR HANDLING-- */
                MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                                        goto fn_fail, "**rmasync");
                /* --END ERROR HANDLING-- */
        }
        win_ptr->start_assert = assert;
        MPIU_Object_add_ref(group_ptr);
        win_ptr->start_group_ptr = group_ptr;
        /**
         * \todo MPI_MODE_NOCHECK might still include POST messages,
         * so the my_sync_begin counter could be incremented. Need to
         * ensure it gets zeroed (appropriately) later...  This is an
         * erroneous condition and needs to be detected and result in
         * reasonable failure.
         */
        if (!(assert & MPI_MODE_NOCHECK)) {
                MPIDU_Progress_spin(!mpid_total_posts_recv(win_ptr));
        }

fn_exit:
        MPIR_Nest_decr();
        MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_START);
        return mpi_errno;

        /* --BEGIN ERROR HANDLING-- */
fn_fail:
        goto fn_exit;
        /* --END ERROR HANDLING-- */
}

/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_post
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_POST function
 *
 * Begin an exposure epoch on nodes in group. Sends MPID_MSGTYPE_POST
 * message to all nodes in group.
 *
 * \param[in] group_ptr	Group
 * \param[in] assert	Synchronization hints
 * \param[in] win_ptr	Window
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC, or error returned from
 *	mpidu_proto_send.
 *
 * \ref post_design
 */
int MPID_Win_post(MPID_Group *group_ptr, int assert, MPID_Win *win_ptr)
{
        int mpi_errno = MPI_SUCCESS;
        volatile unsigned pending = 0;
        MPIU_THREADPRIV_DECL;
        MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_POST);

        MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_POST);
        MPIU_THREADPRIV_GET;
        MPIR_Nest_incr();
        if (win_ptr->_dev.as_target.epoch_type != MPID_EPOTYPE_NONE &&
			win_ptr->_dev.as_target.epoch_type != MPID_EPOTYPE_REFENCE) {
                /* --BEGIN ERROR HANDLING-- */
                MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                                        goto fn_fail, "**rmasync");
                /* --END ERROR HANDLING-- */
        }
        MPIDU_Spin_lock_free(win_ptr);
        MPID_assert_debug(win_ptr->_dev.my_rma_pends == 0 &&
			win_ptr->_dev.my_get_pends == 0);
        win_ptr->_dev.as_target.epoch_size = group_ptr->size;
        win_ptr->_dev.as_target.epoch_type = MPID_EPOTYPE_POST;
        win_ptr->_dev.as_target.epoch_assert = assert;
        win_ptr->_dev.epoch_rma_ok = 1;
        if (assert & MPI_MODE_NOSTORE) {
                /* TBD: anything to optimize? */
        }
        if (assert & MPI_MODE_NOPUT) {
                /* handled later */
        }
        /**
         * \todo In the NOCHECK case, do we still need to Barrier?
         * How do we detect a mismatch of MPI_MODE_NOCHECK in
         * Win_post/Win_start? If the _post has NOCHECK but the _start
         * did not, the _start will wait forever for the POST messages.
         * One option is to still send POST messages in the NOCHECK
         * case, and just not wait in the _start. The POST message
         * could then send the assert value and allow verification
         * when _start nodes call RMA ops.
         */
        if (!(assert & MPI_MODE_NOCHECK)) {
                mpi_errno = mpidu_proto_send(win_ptr, group_ptr,
                                MPID_MSGTYPE_POST);
                if (mpi_errno) { MPIU_ERR_POP(mpi_errno); }
                /**
                 * \todo In theory, we could just return now without
                 * advance/wait.
                 * MPICH2 says this call "does not block", but is
                 * waiting for messages to send considered blocking in
                 * that context? The receiving nodes (in Win_start)
                 * will not procede with RMA ops until they get this
                 * message, so we need to ensure reasonable progress
                 * between the time we call Win_post and Win_wait.
                 * Also, Win_test is not supposed to block in any
                 * fashion, so it should not wait for the sends to
                 * complete either. It seems that the idea is to have
                 * RMA ops going on while this node is executing code
                 * between Win_post and Win_wait, but that won't
                 * happen unless enough calls are made to advance
                 * during that time...
                 * "need input"...
                 */
                MPIDU_Progress_spin(pending > 0);
        }

fn_exit:
        MPIR_Nest_decr();
        MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_POST);
        return mpi_errno;
        /* --BEGIN ERROR HANDLING-- */
fn_fail:
        goto fn_exit;
        /* --END ERROR HANDLING-- */
}

/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_test
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_TEST function
 *
 * Test whether the exposure epoch started by MPID_Win_post has ended.
 * If it has ended, clean up and reset window. This routine must call
 * advance at least once, for any code path.
 *
 * \param[in] win_ptr		Window
 * \param[out] flag		Status of synchronization (TRUE = complete)
 * \return MPI_SUCCESS or MPI_ERR_RMA_SYNC.
 *
 * \see mpid_check_post_done
 *
 * \ref post_design
 */
int MPID_Win_test (MPID_Win *win_ptr, int *flag)
{
        int mpi_errno = MPI_SUCCESS;
        MPIU_THREADPRIV_DECL;
        MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_TEST);

        MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_TEST);
        MPIU_THREADPRIV_GET;
        MPIR_Nest_incr();

        if (win_ptr->_dev.as_target.epoch_type != MPID_EPOTYPE_POST) {
                /* --BEGIN ERROR HANDLING-- */
                MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                                        goto fn_fail, "**rmasync");
                /* --END ERROR HANDLING-- */
        }
        if ((win_ptr->_dev.as_target.epoch_assert & MPI_MODE_NOPUT) &&
                                win_ptr->_dev.my_rma_recvs > 0) {
                /* TBD: handled earlier? */
        }
        *flag = (mpid_check_post_done(win_ptr) != 0);

fn_exit:
        MPIR_Nest_decr();
        MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_TEST);
        return mpi_errno;
        /* --BEGIN ERROR HANDLING-- */
fn_fail:
        goto fn_exit;
        /* --END ERROR HANDLING-- */
}

/// \cond NOT_REAL_CODE
#undef FUNCNAME
#define FUNCNAME MPID_Win_wait
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
/// \endcond
/**
 * \brief MPI-DCMF glue for MPI_WIN_WAIT function
 *
 * Wait for exposure epoch started by MPID_Win_post to end.
 *
 * \param[in] win_ptr		Window
 * \return MPI_SUCCESS or MPI_ERR_RMA_SYNC.
 *
 * \see mpid_check_post_done
 *
 * \ref post_design
 */
int MPID_Win_wait(MPID_Win *win_ptr)
{
        int mpi_errno = MPI_SUCCESS;
        MPIU_THREADPRIV_DECL;
        MPID_MPI_STATE_DECL(MPID_STATE_MPID_WIN_WAIT);

        MPID_MPI_FUNC_ENTER(MPID_STATE_MPID_WIN_WAIT);
        MPIU_THREADPRIV_GET;
        MPIR_Nest_incr();

        if (win_ptr->_dev.as_target.epoch_type != MPID_EPOTYPE_POST) {
                /* --BEGIN ERROR HANDLING-- */
                MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                                        goto fn_fail, "**rmasync");
                /* --END ERROR HANDLING-- */
        }
        if ((win_ptr->_dev.as_target.epoch_assert & MPI_MODE_NOPUT) &&
                                win_ptr->_dev.my_rma_recvs > 0) {
                /* TBD: handled earlier? */
        }
        while (!mpid_check_post_done(win_ptr)) {
                DCMF_CriticalSection_cycle(0);
        }

fn_exit:
        MPIR_Nest_decr();
        MPID_MPI_FUNC_EXIT(MPID_STATE_MPID_WIN_WAIT);
        return mpi_errno;
        /* --BEGIN ERROR HANDLING-- */
fn_fail:
        goto fn_exit;
        /* --END ERROR HANDLING-- */
}
