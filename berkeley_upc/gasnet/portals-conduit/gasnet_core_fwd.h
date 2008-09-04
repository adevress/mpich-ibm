/*   $Source: /var/local/cvs/gasnet/portals-conduit/gasnet_core_fwd.h,v $
 *     $Date: 2007/10/31 05:13:59 $
 * $Revision: 1.9 $
 * Description: GASNet header for PORTALS conduit core (forward definitions)
 * Copyright 2002, Dan Bonachea <bonachea@cs.berkeley.edu>
 * Terms of use are as specified in license.txt
 */

#ifndef _IN_GASNET_H
  #error This file is not meant to be included directly- clients should include gasnet.h
#endif

#ifndef _GASNET_CORE_FWD_H
#define _GASNET_CORE_FWD_H

#define GASNET_CORE_VERSION      1.1
#define GASNET_CORE_VERSION_STR  _STRINGIFY(GASNET_CORE_VERSION)
#define GASNET_CORE_NAME         PORTALS
#define GASNET_CORE_NAME_STR     _STRINGIFY(GASNET_CORE_NAME)
#define GASNET_CONDUIT_NAME      GASNET_CORE_NAME
#define GASNET_CONDUIT_NAME_STR  _STRINGIFY(GASNET_CONDUIT_NAME)
#define GASNET_CONDUIT_PORTALS 1

  /*  defined to be 1 if gasnet_init guarantees that the remote-access memory segment will be aligned  */
  /*  at the same virtual address on all nodes. defined to 0 otherwise */
#if GASNETI_DISABLE_ALIGNED_SEGMENTS
  #define GASNET_ALIGNED_SEGMENTS   0 /* user disabled segment alignment */
#else
  #define GASNET_ALIGNED_SEGMENTS   0
#endif

  /* conduits should define GASNETI_CONDUIT_THREADS to 1 if they have one or more 
     "private" threads which may be used to run AM handlers, even under GASNET_SEQ
     this ensures locking is still done correctly, etc
   */
#if 0
#define GASNETI_CONDUIT_THREADS 1
#endif

  /* define to 1 if your conduit may interrupt an application thread 
     (e.g. with a signal) to run AM handlers (interrupt-based handler dispatch)
   */
#if 0
#define GASNETC_USE_INTERRUPTS 1
#endif

  /* this can be used to add conduit-specific 
     statistical collection values (see gasnet_trace.h) */
#define GASNETC_CONDUIT_STATS(CNT,VAL,TIME)     \
        CNT(C, CHUNK_ALLOC, count)              \
        CNT(C, CHUNK_FREE, count)               \
        CNT(C, TMPMD_ALLOC, count)              \
        CNT(C, TMPMD_FREE, count)               \
        CNT(C, MSG_THROTTLE, count)             \
        CNT(C, CREDIT_THROTTLE, count)		\
        CNT(C, TMPMD_THROTTLE, count)		\
        CNT(C, CREDIT_STALL, count)		\
        CNT(C, SYSQ_DROPPED, count)		\
        CNT(C, END_EPOCH, count)		\
	CNT(C, GET_RAR, count)                  \
	CNT(C, GET_BB, count)                   \
	CNT(C, GET_TMPMD, count)                \
	CNT(C, PUT_RAR, count)                  \
	CNT(C, PUT_BB, count)                   \
	CNT(C, PUT_TMPMD, count)                \
        VAL(C, EVENT_CNT, numreaped)

#endif
