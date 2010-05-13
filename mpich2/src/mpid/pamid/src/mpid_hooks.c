/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_hooks.c
 * \brief ???
 */
#include "mpidimpl.h"

void MPIDU_dtc_free(MPID_Datatype *dt)
{
}

extern int MPIR_Dims_create( int nnodes, int ndims, int *dims );
/** \brief Hook function for a torus-geometry optimized version of MPI_Dims_Create */
int MPID_Dims_create( int nnodes, int ndims, int *dims )
{
  return MPIR_Dims_create( nnodes, ndims, dims );
}
