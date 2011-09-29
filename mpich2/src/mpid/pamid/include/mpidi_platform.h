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
/* Default features */
#define USE_PAMI_RDMA 1
#define USE_PAMI_COMM_THREADS 0
#define USE_PAMI_CONSISTENCY PAMI_HINT_ENABLE
#undef  OUT_OF_ORDER_HANDLING


/* Platform overrides */

#ifdef __BGP__
#undef  MPIDI_EAGER_LIMIT  2049
#define MPIDI_EAGER_LIMIT  UINT_MAX
#endif

#ifdef __BGQ__
#define MPIDI_MAX_THREADS     64
#define MPIDI_MUTEX_L2_ATOMIC 1
#endif
#if defined(__BGQ__) && (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
#undef  USE_PAMI_COMM_THREADS
#define USE_PAMI_COMM_THREADS 1
#endif

#ifdef __PE__
#undef USE_PAMI_CONSISTENCY
#define USE_PAMI_CONSISTENCY PAMI_HINT_DISABLE
#define OUT_OF_ORDER_HANDLING 1
#endif


#endif
