/*
 * compiles all files at once.
 *
 * Faster, no repeating error messages from header files, and may allow some
 * compiler optimizations
 */

#define _IN_UPCR_GLOBFILES_C 1
#define UPCR_NO_SRCPOS 1

/* enable 64-bit system header support */
#define _LARGEFILE64_SOURCE 1
#define _LARGEFILE_SOURCE 1
#include <unistd.h>

#include <upcr.h>

#include <gasnet_coll.h>
#include <gasnet_vis.h>

#include "upcr_alloc.c"
#include "upcr_barrier.c"
#include "upcr_broadcast.c"
#include "upcr_collective.c"
#include "upcr_io.c"
#include "upcr_err.c"
#include "upcr_handlers.c"
#include "upcr_init.c"
#include "upcr_locks.c"
#include "upcr_sem.c"
#include "upcr_memcpy.c"
#include "upcr_sptr.c"
#include "upcr_threads.c"
#include "upcr_util.c"
#include "upcr_extern.c"
#include "upcr_totalview.c"
#include "upcr_profile.c"
#include "upcr_atomic.c"

#ifdef UPCRI_SIZES_H
#include _STRINGIFY(UPCRI_SIZES_H)
#else
#include "upcr_sizes.c"
#endif

/* Intrepid GCC-UPC doesn't use pthreads */
#if UPCRI_USING_GCCUPC
# include "upcr_gccupc.c"
#endif
