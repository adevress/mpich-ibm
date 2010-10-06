/*  (C)Copyright IBM Corp.  2007, 2011  */
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
 * \param[in,out] win  Window
 * \return MPI_SUCCESS or error returned from PMPI_Barrier.
 */
int
MPID_Win_free(MPID_Win **win_ptr)
{
  int mpi_errno = MPI_SUCCESS;

  MPID_Win *win = *win_ptr;
  size_t rank = win->comm_ptr->rank;

  mpi_errno = PMPI_Barrier(win->comm_ptr->handle);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;

  struct MPIDI_Win_info *winfo = &win->mpid.info[rank];
  if (win->size != 0)
    {
      pami_result_t rc;
      rc = PAMI_Memregion_destroy(MPIDI_Context[0], &winfo->memregion);
      MPID_assert(rc == PAMI_SUCCESS);
    }

  MPIU_Free(win->mpid.info);

  int count = 0;
  MPIR_Comm_release_ref(win->comm_ptr, &count);

  MPIU_Handle_obj_free(&MPID_Win_mem, win);

  return mpi_errno;
}
