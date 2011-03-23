/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpix/mpix.c
 * \brief This file is for MPI extensions that MPI apps are likely to use.
 */

#include <mpidimpl.h>
MPIX_Hardware_t MPIDI_HW;

/* Determine the number of torus dimensions. Implemented to keep this code
 * architecture-neutral. do we already have this cached somewhere?
 * this call is non-destructive (no asserts), however future calls are
 * destructive so the user needs to verify numdimensions != -1
 */


static void
MPIX_Init_hw(MPIX_Hardware_t *hw)
{
  memset(hw, 0, sizeof(MPIX_Hardware_t));

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


extern int backtrace(void **buffer, int size);                  /**< GlibC backtrace support */
extern char **backtrace_symbols(void *const *buffer, int size); /**< GlibC backtrace support */
void MPIX_Dump_stacks()
{
  const size_t SIZE=32;
  void *array[SIZE];
  size_t i;
  size_t size    = backtrace(array, SIZE);
  char **strings = backtrace_symbols(array, size);
  fprintf(stderr, "Dumping %zd frames:\n", size - 1);
  for (i = 1; i < size; i++)
    {
      if (strings != NULL)
        fprintf(stderr, "\tFrame %zd: %p: %s\n", i, array[i], strings[i]);
      else
        fprintf(stderr, "\tFrame %zd: %p\n", i, array[i]);
    }

  free(strings); /* Since this is not allocated by MPIU_Malloc, do not use MPIU_Free */
}


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


#if defined(__BGQ__) || defined(__BGP__)

int
MPIX_Torus_ndims(int *numdimensions)
{
  const pamix_torus_info_t *info = PAMIX_Torus_info();
  *numdimensions = info->dims;
  return MPI_SUCCESS;
}


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

typedef struct
{
  pami_geometry_t geometry;
  pami_work_t state;
  pami_configuration_t *configs;
  size_t num_configs;
  pami_event_function fn;
  void* cookie;
} MPIX_Data_t;

void _cb_done(void *ctxt, void *data, pami_result_t err)
{
   int *active = (int *)data;
   (*active)--;
}

pami_result_t
update_comm_geometry(pami_context_t context, void *cookie)
{
  MPIX_Data_t *data = (MPIX_Data_t *)cookie;
  
  return PAMI_Geometry_update(data->geometry,
                              data->configs,
                              data->num_configs,
                              context,
                              data->fn,
                              data->cookie);
}

int
MPIX_Comm_Update(MPI_Comm comm, int optimize)
{
  MPID_Comm * comm_ptr;
  pami_result_t res;
  pami_configuration_t configuration;
  size_t numconfigs = 0;
  int geom_update = 1;
  MPIX_Data_t data;
  
  MPID_Comm_get_ptr(comm, comm_ptr);
  if (!comm_ptr || comm == MPI_COMM_NULL)
    return MPI_ERR_COMM;
  
  configuration.name = PAMI_GEOMETRY_OPTIMIZE;
  configuration.value.intval = 0;
  if (optimize)
    configuration.value.intval = 1;
  data.configs = &configuration;
  data.num_configs = numconfigs;
  data.fn = _cb_done;
  data.cookie = &geom_update;
  data.geometry = comm_ptr->mpid.geometry;
  
  res = PAMI_Context_post(MPIDI_Context[0], &data.state, 
                          update_comm_geometry, (void *)&data);
  
  if (res != PAMI_SUCCESS)
    return MPI_ERR_COMM;
  
  /* Determine what protocols are available for this comm/geom */
  MPIDI_Comm_coll_query(comm_ptr);
  MPIDI_Comm_coll_envvars(comm_ptr);
  
  return MPI_SUCCESS;
}

#endif
