/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/comm/mpid_dims_create.c
 * \brief ???
 */
#include <mpidimpl.h>

extern int MPIR_Dims_create( int nnodes, int ndims, int *dims );
/** \brief Hook function for a torus-geometry optimized version of MPI_Dims_Create */
int MPID_Dims_create( int nnodes, int ndims, int *dims )
{
  return MPIR_Dims_create( nnodes, ndims, dims );
}
