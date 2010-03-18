/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_mem.c
 * \brief ???
 */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpidimpl.h"

void *MPID_Alloc_mem( size_t size, MPID_Info *info_ptr )
{
    void *ap;
    ap = MPIU_Malloc(size);
    return ap;
}

int MPID_Free_mem( void *ptr )
{
    int mpi_errno = MPI_SUCCESS;
    MPIU_Free(ptr);
    return mpi_errno;
}
