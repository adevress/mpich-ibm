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
  sync->total    = 0;
  sync->started  = 0;
  sync->complete = 0;

  mpi_errno = MPIR_Barrier_impl(win->comm_ptr, &mpi_errno);
  return mpi_errno;
}
