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


typedef struct
{
#if defined(__BGQ__) || defined(__BGP__)
#define MPID_TORUS_MAX_DIMS 5 /* This is the maximum physical size of the torus */
   unsigned Size[MPID_TORUS_MAX_DIMS];    /**< Max coordinates on the torus */
   unsigned Coords[MPID_TORUS_MAX_DIMS];  /**< This node's coordinates */
   unsigned isTorus[MPID_TORUS_MAX_DIMS]; /**< Do we have wraparound links? */
   unsigned torus_dimension;              /**< Actual dimension for the torus */

   unsigned rankInPset;
   unsigned sizeOfPset;
   unsigned idOfPset;
#endif
   unsigned coreID;   /**< Core+Thread info. Value ranges from 0..63 */
   unsigned prank;    /**< Physical rank of the node (irrespective of mapping) */
   unsigned psize;    /**< Size of the partition (irrespective of mapping) */
   unsigned ppn;      /**< Processes per node ("T+P" size) */

   unsigned clockMHz; /**< Frequency in MegaHertz */
   unsigned memSize;  /**< Size of the core memory in MB */

} MPIX_Hardware_t;

int MPIX_Hardware(MPIX_Hardware_t *hw);

#if defined(__BGQ__) || defined(__BGP__)
int MPIX_Torus_ndims(int *numdim);
int MPIX_Rank2torus(int rank, int *coords);
int MPIX_Torus2rank(int *coords, int *rank);
#endif


#if defined(__cplusplus)
}
#endif

#endif
