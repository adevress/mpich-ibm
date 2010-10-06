/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_fence.c
 * \brief ???
 */
#include "mpidi_onesided.h"


int
MPID_Win_fence(int       assert,
               MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_PROGRESS_WAIT_WHILE(sync->started != sync->complete);
  sync->started  = 0;
  sync->complete = 0;

  mpi_errno = PMPI_Barrier(win->comm_ptr->handle);
  return mpi_errno;
}
