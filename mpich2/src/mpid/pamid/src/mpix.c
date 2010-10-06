/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpix.c
 * \brief This file is for MPI extensions that MPI apps are likely to use.
 */

#include <mpidimpl.h>
MPIX_Hardware_t MPIDI_HW;

/* Determine the number of torus dimensions. Implemented to keep this code
 * architecture-neutral. do we already have this cached somewhere?
 * this call is non-destructive (no asserts), however future calls are
 * destructive so the user needs to verify numdimensions != -1
 */

/**
 * \brief Determine the number of physical hardware dimensions
 * \param[out] numdimensions The number of torus dimensions
 * Note: This does NOT include the core+thread ID, so if you plan
 * on allocating an array based on this information, you'll need to
 * add 1 to the value returned here
 */


static void
MPIX_Init_hw(MPIX_Hardware_t *hw)
{
  hw->clockMHz = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CLOCK_MHZ).value.intval;
  hw->memSize  = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_MEM_SIZE).value.intval;

#if defined(__BGQ__) || defined(__BGP__)
  int i=0;
  const pamix_torus_info_t *info = PAMIX_Torus_info();
  /* The extension returns a "T" dimension */
  hw->torus_dimension = info->dims-1;

  for(i=0;i<info->dims-1;i++)
    {
      hw->Size[i]    = info->size[i];
      hw->Coords[i]  = info->coord[i];
      hw->isTorus[i] = info->torus[i];
    }
  /* The torus extension returns "T" as the last element */
  hw->coreID = info->coord[info->dims-1];
  hw->ppn    = info->size[info->dims-1];
#endif
}


void
MPIX_Init()
{
  MPIX_Init_hw(&MPIDI_HW);
}


int
MPIX_Torus_ndims(int *numdimensions)
{
  const pamix_torus_info_t *info = PAMIX_Torus_info();
  *numdimensions = info->dims;
  return MPI_SUCCESS;
}


/**
 * \brief Convert an MPI rank into physical coordinates plus core ID
 * \param[in] rank The MPI Rank
 * \param[out] coords An array of size hw.torus_dimensions+1. The last
 *                   element of the returned array is the core+thread ID
 */
int
MPIX_Rank2torus(int rank, int *coords)
{
  size_t coord_array[MPIDI_HW.torus_dimension+1];

  PAMIX_Task2torus(rank, coord_array);

  unsigned i;
  for(i=0; i<MPIDI_HW.torus_dimension+1; ++i)
    coords[i] = coord_array[i];

  return MPI_SUCCESS;
}


/**
 * \brief Convert a set of coordinates (physical+core/thread) to an MPI rank
 * \param[in] coords An array of size hw.torus_dimensions+1. The last element
 *                   should be the core+thread ID (0..63).
 * \param[out] rank The MPI rank cooresponding to the coords array passed in
 */
int
MPIX_Torus2rank(int *coords, int *rank)
{
  size_t coord_array[MPIDI_HW.torus_dimension+1];
  pami_task_t task;

  unsigned i;
  for(i=0; i<MPIDI_HW.torus_dimension+1; ++i)
    coord_array[i] = coords[i];

  PAMIX_Torus2task(coord_array, &task);
  *rank = task;

  return MPI_SUCCESS;
}


/**
 * \brief Fill in an MPIX_Hardware_t structure
 * \param[in] hw A pointer to an MPIX_Hardware_t structure to be filled in
 */
int
MPIX_Hardware(MPIX_Hardware_t *hw)
{
  MPID_assert(hw != NULL);
  /*
   * We've already initialized the hw structure in MPID_Init,
   * so just copy it to the users buffer
   */
  memcpy(hw, &MPIDI_HW, sizeof(MPIX_Hardware_t));
  return MPI_SUCCESS;
}
