/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_create.c
 * \brief ???
 */
#include "mpidi_onesided.h"


/**
 * \brief MPI-PAMI glue for MPI_Win_create function
 *
 * Create a window object. Allocates a MPID_Win object and initializes it,
 * then allocates the collective info array, initalizes our entry, and
 * performs an Allgather to distribute/collect the rest of the array entries.
 *
 * ON first call, initializes (registers) protocol objects for locking,
 * get, and send operations to message layer. Also creates datatype to
 * represent the rma_sends element of the collective info array,
 * used later to synchronize epoch end events.
 *
 * \param[in] base	Local window buffer
 * \param[in] size	Local window size
 * \param[in] disp_unit	Displacement unit size
 * \param[in] info	Window hints (not used)
 * \param[in] comm_ptr	Communicator
 * \param[out] win_ptr	Window
 * \return MPI_SUCCESS, MPI_ERR_OTHER, or error returned from
 *	PMPI_Comm_dup or PMPI_Allgather.
 */
int
MPID_Win_create(void       * base,
                MPI_Aint     length,
                int          disp_unit,
                MPID_Info  * info,
                MPID_Comm  * comm_ptr,
                MPID_Win  ** win_ptr)
{
  int mpi_errno  = MPI_SUCCESS;


  /* ----------------------------------------- */
  /*  Setup the common sections of the window  */
  /* ----------------------------------------- */
  MPID_Win *win = (MPID_Win*)MPIU_Handle_obj_alloc(&MPID_Win_mem);
  if (win == NULL)
    return mpi_errno;
  *win_ptr = win;

  win->base = base;
  win->size = length;
  win->disp_unit = disp_unit;

  /* --------------------------------------- */
  /*  Setup the PAMI sections of the window  */
  /* --------------------------------------- */
  memset(&win->mpid, 0, sizeof(struct MPIDI_Win));

  win->comm_ptr = comm_ptr; MPIR_Comm_add_ref(comm_ptr);

  size_t size = comm_ptr->local_size;
  size_t rank = comm_ptr->rank;

  win->mpid.info = MPIU_Calloc0(size, struct MPIDI_Win_info);

  struct MPIDI_Win_info *winfo = &win->mpid.info[rank];

  MPID_assert((base != NULL) || (length == 0));
  if (length != 0)
    {
      size_t length_out = 0;
      pami_result_t rc;
      rc = PAMI_Memregion_create(MPIDI_Context[0], base, length, &length_out, &winfo->memregion);
      MPID_assert(rc == PAMI_SUCCESS);
      MPID_assert(length == length_out);
    }

  winfo->base_addr  = base;
  /* winfo->win_handle = win->handle; */
  winfo->win        = win;
  winfo->disp_unit  = disp_unit;

  mpi_errno = PMPI_Allgather(MPI_IN_PLACE,
                             0,
                             MPI_DATATYPE_NULL,
                             win->mpid.info,
                             sizeof(struct MPIDI_Win_info),
                             MPI_BYTE,
                             comm_ptr->handle);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;

  mpi_errno = PMPI_Barrier(comm_ptr->handle);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;

  return mpi_errno;
}
