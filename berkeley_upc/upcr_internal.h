/*
 * Internal UPC Runtime functions.
 *
 * Jason Duell  <jcduell@lbl.gov>
 *
 * $Id: upcr_internal.h,v 1.33 2006/08/23 13:05:09 bonachea Exp $
 */

#ifndef UPCR_INTERNAL_H
#define UPCR_INTERNAL_H

#if !_IN_UPCR_GLOBFILES_C
  #error Include/build error - upcr_internal.h should only be included by the upcr implementation .c files, which should only be compiled via upcr_globfiles.c
#endif

#include <upcr.h>
#include <gasnet_tools.h>

GASNETT_BEGIN_EXTERNC

/************************************************************************
 * Internal library global data
 ************************************************************************/



/************************************************************************
 * Internal library non-inlined functions
 ************************************************************************/

void upcri_alloc_initheaps();

/* Broadcasts data from one thread to all the rest  */
#define upcri_broadcast(fromthread, addr, len) \
       _upcri_broadcast(fromthread, addr, len UPCRI_PT_PASS)

void _upcri_broadcast(upcr_thread_t fromthread, void *addr, size_t len UPCRI_PT_ARG);


#if UPCRI_SUPPORT_TOTALVIEW
  /* To support TotalView debugging of application code */
  #define upcri_startup_totalview(argv) \
       _upcri_startup_totalview(argv UPCRI_PT_PASS)
  int _upcri_startup_totalview(char **argv UPCRI_PT_ARG);
#else
  #define upcri_startup_totalview(argv) (0)
#endif

/************************************************************************
 * Inline functions and macros that don't need to be visible to user.
 ************************************************************************/
    
#include <assert.h>
#undef assert
#define assert(x) !!! Error - use upcri_assert() !!!

/* (En/Dis)able use of out-of-segment temporaries with GASNet collectives.
 * This presently affects the UPC reduce and prefix-reduce implementations
 * and totalview initialization.
 * This is a "shutoff" switch for use if GASNet is suspected to be unreliable.
 * If not set, then upcr_local_alloc() is used to obtain in-segment temporaries.
 */
#define UPCRI_COLL_USE_LOCAL_TEMPS 1

/* "Single" barriers that are named in a DEBUG build (to catch errors)
 * and anonymous in an NDEBUG build (for speed).
 *
 * When debugging, we hope to catch any case that a barrier internal to
 * the runtime would accidentally match any other barrier.  For this
 * reason we use __LINE__ to name barriers rather than a sequence number.
 * However, we also add a large offset to reduce to possibility of
 * collision with small integers that are a likely choice for use in
 * application code.  There is no "perfect" naming, but this is good
 * enough for our purposes.
 *
 * _NOTHR variants call gasnet directly for code prior to pthread setup.
 */
#ifndef UPCRI_SINGLE_BARRIER_NAME
  /* Can redefine if this happens to colide with an app to be debugged */
  #define UPCRI_SINGLE_BARRIER_NAME()	(__LINE__+88888888)
#endif
#define UPCRI_SINGLE_BARRIER_NOTHR() \
      UPCRI_SINGLE_BARRIER_WITHCODE_NOTHR(do {} while(0))
#define UPCRI_SINGLE_BARRIER() \
      UPCRI_SINGLE_BARRIER_WITHCODE(do {} while(0))
#if UPCR_DEBUG
  #define UPCRI_SINGLE_BARRIER_WITHCODE_NOTHR(code) do { \
      int _name = UPCRI_SINGLE_BARRIER_NAME();           \
      gasnet_barrier_notify(_name, 0);                   \
      code;                                              \
      gasnet_barrier_wait(_name, 0);                     \
    } while(0)
  #define UPCRI_SINGLE_BARRIER_WITHCODE(code) do { \
      int _name = UPCRI_SINGLE_BARRIER_NAME();     \
      upcr_notify(_name, 0);                       \
      code;                                        \
      upcr_wait(_name, 0);                         \
    } while(0)
#else
  #define UPCRI_SINGLE_BARRIER_WITHCODE_NOTHR(code) do {      \
      gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS); \
      code;                                                   \
      gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);   \
    } while(0)
  #define UPCRI_SINGLE_BARRIER_WITHCODE(code) do { \
      upcr_notify(0, UPCR_BARRIERFLAG_ANONYMOUS);  \
      code;                                        \
      upcr_wait(0, UPCR_BARRIERFLAG_ANONYMOUS);    \
    } while(0)
#endif

GASNETT_END_EXTERNC

#endif /* UPCR_INTERNAL_H */
