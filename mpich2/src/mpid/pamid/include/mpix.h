/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpix.h
 * \brief Blue Gene extensions to the MPI Spec
 *
 * These functions generally use MPI functions and internal APIs to
 * expose extra information relating to the specific system on which
 * the job is running.  This may allow certain hardware specific
 * optimizations to be made.
 */

#ifndef __include_mpix_h__
#define __include_mpix_h__

#if defined(__cplusplus)
extern "C" {
#endif


int PAMIX_Get_torus_dims(size_t *numdim);
int PAMIX_Rank2torus(int rank, size_t *coords);
int PAMIX_Torus2rank(size_t *coords, int *rank);


#if defined(__cplusplus)
}
#endif

#endif
