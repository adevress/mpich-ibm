/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_platform.h
 * \brief ???
 */


#ifndef __include_mpidi_platform_h__
#define __include_mpidi_platform_h__

/* Default values */

#define MPIDI_MAX_CONTEXTS 64
/** This is not the real value, but should default to something larger than PAMI_DISPATCH_SEND_IMMEDIATE_MAX */
#define MPIDI_SHORT_LIMIT  555
/** This is set to 4 BGQ torus packets (+1, because of the way it is compared) */
#define MPIDI_EAGER_LIMIT  2049
/** This is set to 0 which effectively disables the eager protocol for local transfers */
#define MPIDI_EAGER_LIMIT_LOCAL  0
/* Default features */
#define USE_PAMI_RDMA 1
#define USE_PAMI_CONSISTENCY PAMI_HINT_ENABLE
#undef  OUT_OF_ORDER_HANDLING


/* Platform overrides */
/* These should be disable-able shortly */
//#define PAMI_BYTES_REQUIRED 
//#define PAMI_DISPS_ARE_BYTES 


#ifdef __BGP__
#undef  MPIDI_EAGER_LIMIT
#define MPIDI_EAGER_LIMIT  UINT_MAX
#endif

#ifdef __BGQ__
#define MPIDI_MAX_THREADS     64
#define MPIDI_MUTEX_L2_ATOMIC 1
#define MPIDI_BASIC_COLLECTIVE_SELECTION 1
#endif

#ifdef __PE__
#undef USE_PAMI_CONSISTENCY
#define USE_PAMI_CONSISTENCY PAMI_HINT_DISABLE
#undef  MPIDI_EAGER_LIMIT
#define MPIDI_EAGER_LIMIT  4096
#define OUT_OF_ORDER_HANDLING 1
#endif


#endif
