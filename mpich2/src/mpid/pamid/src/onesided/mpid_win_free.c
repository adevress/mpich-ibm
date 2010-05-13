/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_free.c
 * \brief ???
 */
#include "mpidi_onesided.h"


/**
 * \brief MPI-PAMI glue for MPI_Win_free function
 *
 * Release all references and free memory associated with window.
 *
 * \param[in,out] win_ptr	Window
 * \return MPI_SUCCESS or error returned from PMPI_Barrier.
 */
int
MPID_Win_free(MPID_Win **win_ptr)
{
  int mpi_errno = MPI_SUCCESS;

  MPID_Win *win = *win_ptr;
  size_t rank = win->mpid.comm_ptr->rank;

  struct MPIDI_Win_info *winfo = &win->mpid.info[rank];
  pami_result_t rc;
  rc = PAMI_Memregion_destroy(MPIDI_Context[0], &winfo->memregion);
  MPID_assert(rc == PAMI_SUCCESS);

  MPIU_Free(win->mpid.info);

  mpi_errno = PMPI_Comm_free(&win->comm);
  if (unlikely(mpi_errno != MPI_SUCCESS))
    return mpi_errno;

  MPIU_Handle_obj_free(&MPID_Win_mem, win_ptr);

  return mpi_errno;
}
