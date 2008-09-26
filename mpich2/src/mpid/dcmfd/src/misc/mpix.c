/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/misc/mpix.c
 * \brief Blue Gene extensions to the MPI Spec
 */

#include "mpidimpl.h"
#include "mpix.h"

#pragma weak PMI_torus2rank = MPIX_torus2rank
unsigned MPIX_torus2rank (unsigned        x,
                         unsigned        y,
                         unsigned        z,
                         unsigned        t)
{
  unsigned rank;
  int rc = DCMF_Messager_torus2rank(x, y, z, t, &rank);
  if(rc == DCMF_SUCCESS) return rank;
  else return (unsigned)-1;
}

#pragma weak PMI_Comm_torus2rank = MPIX_Comm_torus2rank
unsigned MPIX_Comm_torus2rank (MPI_Comm comm,
                              unsigned x,
                              unsigned y,
                              unsigned z,
                              unsigned t)
{
  int rank, worldrank = MPIX_torus2rank(x, y, z, t);
  if (comm == MPI_COMM_WORLD) rank = worldrank;
  else
    {
      MPI_Group group_a, worldgroup;
      PMPI_Comm_group (comm, &group_a);
      PMPI_Comm_group (MPI_COMM_WORLD, &worldgroup);
      PMPI_Group_translate_ranks (worldgroup, 1, &worldrank, group_a, &rank);
    }
  return rank;
}

#pragma weak PMI_rank2torus = MPIX_rank2torus
void MPIX_rank2torus (unsigned        rank,
                     unsigned       *x,
                     unsigned       *y,
                     unsigned       *z,
                     unsigned       *t)
{
  DCMF_Messager_rank2torus (rank, x, y, z, t);
}

#pragma weak PMI_Comm_rank2torus = MPIX_Comm_rank2torus
void MPIX_Comm_rank2torus(MPI_Comm comm,
                         unsigned rank,
                         unsigned *x,
                         unsigned *y,
                         unsigned *z,
                         unsigned *t)
{
  int worldrank;
  if (comm == MPI_COMM_WORLD) worldrank = rank;
  else
    {
      MPI_Group group_a, worldgroup;
      PMPI_Comm_group (comm, &group_a);
      PMPI_Comm_group (MPI_COMM_WORLD, &worldgroup);
      PMPI_Group_translate_ranks (group_a, 1, (int*)&rank, worldgroup, &worldrank);
    }
  MPIX_rank2torus (worldrank, x, y, z, t);
}


/**
 * \brief Compare each elemt of two four-element arrays
 * \param[in] A The first array
 * \param[in] B The first array
 * \returns MPI_SUCCESS (does not return on failure)
 */
#define CMP_4(A,B)                              \
({                                              \
  assert(A[0] == B[0]);                         \
  assert(A[1] == B[1]);                         \
  assert(A[2] == B[2]);                         \
  assert(A[3] == B[3]);                         \
  MPI_SUCCESS;                                  \
})
#pragma weak PMI_Cart_comm_create = MPIX_Cart_comm_create
int MPIX_Cart_comm_create(MPI_Comm *cart_comm)
{
  int result;
  int rank, numprocs,
      dims[4],
      wrap[4],
      coords[4];
  int new_rank,
      new_dims[4],
      new_wrap[4],
      new_coords[4];
  DCMF_Hardware_t pers;


  *cart_comm = MPI_COMM_NULL;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
  PMPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  DCMF_Hardware(&pers);


  dims[3]   = pers.xSize;
  dims[2]   = pers.ySize;
  dims[1]   = pers.zSize;
  dims[0]   = pers.tSize;

/* This only works if MPI_COMM_WORLD is the full partition */
  if (dims[3] * dims[2] * dims[1] * dims[0] != numprocs)
    return MPI_ERR_TOPOLOGY;

  wrap[3]   = pers.xTorus;
  wrap[2]   = pers.yTorus;
  wrap[1]   = pers.zTorus;
  wrap[0]   = pers.tTorus;

  coords[3] = pers.xCoord;
  coords[2] = pers.yCoord;
  coords[1] = pers.zCoord;
  coords[0] = pers.tCoord;


  result = PMPI_Cart_create(
    MPI_COMM_WORLD,
    4,
    dims,
    wrap,
    0,
    cart_comm
  );
  if (result != MPI_SUCCESS) return result;


  PMPI_Comm_rank(*cart_comm, &new_rank);
  PMPI_Cart_get (*cart_comm, 4, new_dims, new_wrap, new_coords);

  CMP_4(dims,   new_dims);
  CMP_4(wrap,   new_wrap);
  CMP_4(coords, new_coords);

  return MPI_SUCCESS;
}

#pragma weak PMI_Pset_same_comm_create = MPIX_Pset_same_comm_create
int MPIX_Pset_same_comm_create(MPI_Comm *pset_comm)
{
  int key, color;
  DCMF_Hardware_t pers;

  DCMF_Hardware(&pers);
  /*  All items of the same color are grouped in the same communicator  */
  color = pers.idOfPset;
  /*  The key implies the new rank  */
  key   = pers.rankInPset*pers.tSize + pers.tCoord;

  return PMPI_Comm_split(MPI_COMM_WORLD, color, key, pset_comm);
}

#pragma weak PMI_Pset_diff_comm_create = MPIX_Pset_diff_comm_create
int MPIX_Pset_diff_comm_create(MPI_Comm *pset_comm)
{
  int key, color;
  DCMF_Hardware_t pers;

  DCMF_Hardware(&pers);
  /*  The key implies the new rank  */
  key   = pers.idOfPset;
  /*  All items of the same color are grouped in the same communicator  */
  color = pers.rankInPset*pers.tSize + pers.tCoord;

  return PMPI_Comm_split(MPI_COMM_WORLD, color, key, pset_comm);
}


#pragma weak MPID_Dump_stacks = MPIX_Dump_stacks
extern int backtrace(void **buffer, int size);                  /**< GlibC backtrace support */
extern char **backtrace_symbols(void *const *buffer, int size); /**< GlibC backtrace support */
void MPIX_Dump_stacks()
{
  void *array[32];
  size_t i;
  size_t size    = backtrace(array, 32);
  char **strings = backtrace_symbols(array, size);
  fprintf(stderr, "Dumping %zd frames:\n", size - 1);
  for (i = 1; i < size; i++)
    {
      if (strings != NULL)
        fprintf(stderr, "\tFrame %d: %p: %s\n", i, array[i], strings[i]);
      else
        fprintf(stderr, "\tFrame %d: %p\n", i, array[i]);
    }

  free(strings); /* Since this is not allocated by MPIU_Malloc, do not use MPIU_Free */
}

#pragma weak PMI_Get_property = MPIX_Get_property
int MPIX_Get_property(MPI_Comm comm, int prop, int * result)
{
  MPID_Comm * comm_ptr;
  MPID_Comm_get_ptr(comm, comm_ptr);
  if (!comm_ptr || (prop < 0 && prop > DCMF_MAX_NUM_BITS))
  {
    if (comm_ptr -> rank == 0)
      fprintf(stderr, "invalid property to check or bad communicator\n");
    return DCMF_INVAL;
  }

  *result = DCMF_INFO_ISSET(&(comm_ptr->dcmf.properties), prop);
  return DCMF_SUCCESS;
}

#pragma weak PMI_Set_property = MPIX_Set_property
int MPIX_Set_property(MPI_Comm comm, int prop, int value)
{
  MPID_Comm * comm_ptr;
  MPID_Comm_get_ptr(comm, comm_ptr);
  
  if (!comm_ptr || (prop < 0 && prop > DCMF_MAX_NUM_BITS))
  {
    if (comm_ptr -> rank == 0)
      fprintf(stderr, "invalid property to set or bad communicator\n");
    return DCMF_INVAL;
  }
  if (value != 0 && value != 1)
  {
    if (comm_ptr -> rank == 0)
      fprintf(stderr, "%d is invalid value to set (use 0 or 1)\n", value);
    return DCMF_INVAL;
  }

  if (!value)
    DCMF_INFO_UNSET(&(comm_ptr->dcmf.properties), prop);
  else
    DCMF_INFO_SET(&(comm_ptr->dcmf.properties), prop);

  if (prop == DCMF_TREE_COMM || prop == DCMF_RECT_COMM)
    MPIDI_Comm_setup_properties(comm_ptr, 0);

  return DCMF_SUCCESS;
}
