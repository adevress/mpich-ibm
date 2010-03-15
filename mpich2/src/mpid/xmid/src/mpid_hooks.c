/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_hooks.c
 * \brief ???
 */
#include "mpidimpl.h"

void MPIDU_dtc_free(MPID_Datatype *dt)
{
}
void MPIDI_Comm_create(MPID_Comm *comm)
{
}
void MPIDI_Comm_destroy(MPID_Comm *comm)
{
}

extern int MPIR_Dims_create( int nnodes, int ndims, int *dims );
/** \brief Hook function for a torus-geometry optimized version of MPI_Dims_Create */
int MPID_Dims_create( int nnodes, int ndims, int *dims )
{
  return MPIR_Dims_create( nnodes, ndims, dims );
}



int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPID_Win_create()  __attribute__((alias("MPID_Dummy")));
int MPID_Win_free()    __attribute__((alias("MPID_Dummy")));
