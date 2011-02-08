/*  (C)Copyright IBM Corp.  2007, 2011  */
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

  /**
   * \brief Fill in an MPIX_Hardware_t structure
   * \param[in] hw A pointer to an MPIX_Hardware_t structure to be filled in
   */
  int MPIX_Hardware(MPIX_Hardware_t *hw);


  /* These functions only exist on torus platforms (i.e. Blue Gene) */

  /**
   * \brief Determine the number of physical hardware dimensions
   * \param[out] numdimensions The number of torus dimensions
   * \note This does NOT include the core+thread ID, so if you plan on
   *       allocating an array based on this information, you'll need to
   *       add 1 to the value returned here
   */
  int MPIX_Torus_ndims(int *numdim);
  /**
   * \brief Convert an MPI rank into physical coordinates plus core ID
   * \param[in] rank The MPI Rank
   * \param[out] coords An array of size hw.torus_dimensions+1. The last
   *                   element of the returned array is the core+thread ID
   */
  int MPIX_Rank2torus(int rank, int *coords);
  /**
   * \brief Convert a set of coordinates (physical+core/thread) to an MPI rank
   * \param[in] coords An array of size hw.torus_dimensions+1. The last element
   *                   should be the core+thread ID (0..63).
   * \param[out] rank The MPI rank cooresponding to the coords array passed in
   */
  int MPIX_Torus2rank(int *coords, int *rank);


  /**
   * \brief Print the current system stack
   *
   * The first frame (this function) is discarded to make the trace look nicer.
   */
  void MPIX_Dump_stacks();


#if defined(__cplusplus)
}
#endif

#endif
