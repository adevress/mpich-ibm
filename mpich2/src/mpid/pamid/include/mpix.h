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


#define MPIX_TORUS_MAX_DIMS 5 /* This is the maximum physical size of the torus */
  typedef struct
  {
/* These fields will be used on all platforms. */
    unsigned prank;    /**< Physical rank of the node (irrespective of mapping) */
    unsigned psize;    /**< Size of the partition (irrespective of mapping) */
    unsigned ppn;      /**< Processes per node ("T+P" size) */
    unsigned coreID;   /**< Core+Thread info. Value ranges from 0..63 */

    unsigned clockMHz; /**< Frequency in MegaHertz */
    unsigned memSize;  /**< Size of the core memory in MB */

/* These fields are only set on torus platforms (i.e. Blue Gene) */
    unsigned torus_dimension;              /**< Actual dimension for the torus */
    unsigned Size[MPIX_TORUS_MAX_DIMS];    /**< Max coordinates on the torus */
    unsigned Coords[MPIX_TORUS_MAX_DIMS];  /**< This node's coordinates */
    unsigned isTorus[MPIX_TORUS_MAX_DIMS]; /**< Do we have wraparound links? */

/* These fields are only set on systems using Blue Gene IO psets. */
    unsigned rankInPset;
    unsigned sizeOfPset;
    unsigned idOfPset;
  } MPIX_Hardware_t;

  int MPIX_Hardware(MPIX_Hardware_t *hw);

/* These functions only exist on torus platforms (i.e. Blue Gene) */
  int MPIX_Torus_ndims(int *numdim);
  int MPIX_Rank2torus(int rank, int *coords);
  int MPIX_Torus2rank(int *coords, int *rank);


#if defined(__cplusplus)
}
#endif

#endif
