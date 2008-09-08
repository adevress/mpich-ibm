/*
 * UPC Collectives implementation on GASNet
 * $Id: upcr_collective.c,v 1.34 2006/07/23 21:34:06 bonachea Exp $
 */

#include <upcr.h>
#include <upcr_internal.h>

/*----------------------------------------------------------------------------*/
/* Initialization, global variables */


#if UPCRI_SINGLE_ALIGNED_REGIONS
    void upcri_coll_init(void) {
        gasnet_coll_init(NULL, 0 /* ignored */, NULL, 0 , 0);
    }
    void _upcri_coll_init_thread(UPCRI_PT_ARG_ALONE) {
        /* No per-thread work */
    }
#elif UPCRI_UPC_PTHREADS
    void upcri_coll_init(void) {
        /* Nothing to do */
    }
    void _upcri_coll_init_thread(UPCRI_PT_ARG_ALONE) {
        UPCRI_PASS_GAS();
        gasnet_node_t nodes = gasnet_nodes();
        gasnet_image_t *tmp_node2pthreads = upcri_malloc(sizeof(gasnet_image_t) * nodes);
        int i;
    
        for (i = 0; i < nodes; ++i) {
	    tmp_node2pthreads[i] = upcri_pthreads(i);
        }
        gasnet_coll_init(tmp_node2pthreads, upcr_mythread(), NULL, 0, 0);
        upcri_free(tmp_node2pthreads);
        (upcri_auxdata()->coll_dstlist) = upcri_malloc(upcri_threads * sizeof(void *));
	(upcri_auxdata()->coll_srclist) = upcri_malloc(upcri_threads * sizeof(void *));
    }
#else /* single unaligned regions */
    void upcri_coll_init(void) {
        (upcri_auxdata()->coll_dstlist) = upcri_malloc(upcri_threads * sizeof(void *));
        (upcri_auxdata()->coll_srclist) = upcri_malloc(upcri_threads * sizeof(void *));
        gasnet_coll_init(NULL, 0/* ignored */, NULL, 0, 0);
    }
    void _upcri_coll_init_thread(UPCRI_PT_ARG_ALONE) {
        /* No per-thread work */
    }
#endif

/*----------------------------------------------------------------------------*/
/* Magical helpers */

#define UPCRI_COLL_INSYNC_MASK	(UPC_IN_NOSYNC  | UPC_IN_MYSYNC  | UPC_IN_ALLSYNC)
#define UPCRI_COLL_OUTSYNC_MASK	(UPC_OUT_NOSYNC | UPC_OUT_MYSYNC | UPC_OUT_ALLSYNC)

#if (UPC_IN_ALLSYNC == 0) || (UPC_OUT_ALLSYNC == 0)
  #error "No longer support a upc_flag_t representation with zero-valued ALLSYNCs"
#endif

/* Make 0 an alias for the ALLSYNC flags and check flag sanity if debugging */
/* XXX: add callsite information to error messages? */
GASNETT_INLINE(upcri_coll_fixsync)
upc_flag_t upcri_coll_fixsync(upc_flag_t sync_mode) {
  if ((sync_mode & UPCRI_COLL_INSYNC_MASK) == 0) {
    sync_mode |= UPC_IN_ALLSYNC;
  }
  if ((sync_mode & UPCRI_COLL_OUTSYNC_MASK) == 0) {
    sync_mode |= UPC_OUT_ALLSYNC;
  }

#if UPCR_DEBUG
  switch (sync_mode & UPCRI_COLL_INSYNC_MASK) {
    case UPC_IN_NOSYNC:
    case UPC_IN_MYSYNC:
    case UPC_IN_ALLSYNC:
      break;
    default:
      upcri_err("Invalid combination of multiple UPC_IN_*SYNC flags");
  }
  switch (sync_mode & UPCRI_COLL_OUTSYNC_MASK) {
    case UPC_OUT_NOSYNC:
    case UPC_OUT_MYSYNC:
    case UPC_OUT_ALLSYNC:
      break;
    default:
      upcri_err("Invalid combination of multiple UPC_OUT_*SYNC flags");
  }
  if (sync_mode & ~(UPCRI_COLL_INSYNC_MASK | UPCRI_COLL_OUTSYNC_MASK)) {
    upcri_warn("Invalid/unknown flags set in sync_mode");
  }
#endif

  return sync_mode;
}

GASNETT_INLINE(upcri_coll_syncmode_only)
unsigned int upcri_coll_syncmode_only(upc_flag_t sync_mode) {
  unsigned int result;

  if (sync_mode & UPC_IN_NOSYNC) {
    result = GASNET_COLL_IN_NOSYNC;
  } else if (sync_mode & UPC_IN_MYSYNC) {
    result = GASNET_COLL_IN_MYSYNC;
  } else if (sync_mode & UPC_IN_ALLSYNC) {
    result = GASNET_COLL_IN_ALLSYNC;
  } else result = 0;

  if (sync_mode & UPC_OUT_NOSYNC) {
    result |= GASNET_COLL_OUT_NOSYNC;
  } else if (sync_mode & UPC_OUT_MYSYNC) {
    result |= GASNET_COLL_OUT_MYSYNC;
  } else if (sync_mode & UPC_OUT_ALLSYNC) {
    result |= GASNET_COLL_OUT_ALLSYNC;
  }

  return result;
}

GASNETT_INLINE(upcri_coll_syncmode)
unsigned int upcri_coll_syncmode(upc_flag_t sync_mode) {
  return GASNET_COLL_SINGLE |
	 GASNET_COLL_SRC_IN_SEGMENT | GASNET_COLL_DST_IN_SEGMENT |
	 upcri_coll_syncmode_only(sync_mode);
}

#define UPCRI_COLL_ONE_DST(D)	upcr_threadof_shared(D), upcri_shared_to_remote(D)
#define UPCRI_COLL_ONE_SRC(S)	upcr_threadof_shared(S), upcri_shared_to_remote(S)

#if UPCRI_SINGLE_ALIGNED_REGIONS
    #define UPCRI_COLL_MULTI_DST(D)	upcri_shared_to_remote(D)
    #define UPCRI_COLL_MULTI_SRC(S)	upcri_shared_to_remote(S)
    #define UPCRI_COLL_GASNETIFY(NAME,ARGS)\
	_CONCAT(gasnet_coll_,NAME)ARGS;
    #define UPCRI_COLL_GASNETIFY_NB(NAME,ARGS)\
	_CONCAT(_CONCAT(gasnet_coll_,NAME),_nb)ARGS;
#else /* Multiple regions or single unaligned regions */
    GASNETT_INLINE(upcri_coll_shared_to_dstlist)
    void **upcri_coll_shared_to_dstlist(upcr_shared_ptr_t ptr UPCRI_PT_ARG) {
        void **result = upcri_auxdata()->coll_dstlist;
        upcr_thread_t i;
        for (i = 0; i < upcri_threads; ++i) {
	    result[i] = upcri_shared_to_remote_withthread(ptr, i);
        }
        return result;
    }
    #define UPCRI_COLL_MULTI_DST(D)		upcri_coll_shared_to_dstlist(D UPCRI_PT_PASS)
    GASNETT_INLINE(upcri_coll_shared_to_srclist)
    void **upcri_coll_shared_to_srclist(upcr_shared_ptr_t ptr UPCRI_PT_ARG) {
        void **result = upcri_auxdata()->coll_srclist;
        upcr_thread_t i;
        for (i = 0; i < upcri_threads; ++i) {
	    result[i] = upcri_shared_to_remote_withthread(ptr, i);
        }
        return result;
    }
    #define UPCRI_COLL_MULTI_SRC(S)		upcri_coll_shared_to_srclist(S UPCRI_PT_PASS)
    #define UPCRI_COLL_GASNETIFY(NAME,ARGS)\
	_CONCAT(_CONCAT(gasnet_coll_,NAME),M)ARGS;
    #define UPCRI_COLL_GASNETIFY_NB(NAME,ARGS)\
	_CONCAT(_CONCAT(_CONCAT(gasnet_coll_,NAME),M),_nb)ARGS;
#endif

#define UPCRI_TRACE_COLL(name, nbytes) \
	UPCRI_TRACE_PRINTF(("COLLECTIVE UPC_ALL_" _STRINGIFY(name) ": sz = %6llu", (long long unsigned)(nbytes)))

/*----------------------------------------------------------------------------*/
/* Data movement collectives
 *
 * With the aid of the helpers above, these functions compile correctly
 * with and without aligned segments and with and without pthreads.
 */

void _upcr_all_broadcast(upcr_shared_ptr_t dst,
                         upcr_shared_ptr_t src,
                         size_t nbytes,
                         upc_flag_t sync_mode
                         UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(BROADCAST, nbytes);
    #define UPCRI_PEVT_ARGS , &dst, &src, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_BROADCAST);
    UPCRI_COLL_GASNETIFY(broadcast,
				(GASNET_TEAM_ALL,
				 UPCRI_COLL_MULTI_DST(dst),
				 UPCRI_COLL_ONE_SRC(src),
				 nbytes, upcri_coll_syncmode(sync_mode)));
    upcri_pevt_end(GASP_UPC_ALL_BROADCAST);
    #undef UPCRI_PEVT_ARGS
}

void _upcr_all_scatter(upcr_shared_ptr_t dst,
                       upcr_shared_ptr_t src,
                       size_t nbytes,
                       upc_flag_t sync_mode
                       UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(SCATTER, nbytes);
    #define UPCRI_PEVT_ARGS , &dst, &src, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_SCATTER);
    UPCRI_COLL_GASNETIFY(scatter,
				(GASNET_TEAM_ALL,
				 UPCRI_COLL_MULTI_DST(dst),
				 UPCRI_COLL_ONE_SRC(src),
				 nbytes, upcri_coll_syncmode(sync_mode)));
    upcri_pevt_end(GASP_UPC_ALL_SCATTER);
    #undef UPCRI_PEVT_ARGS
}

void _upcr_all_gather(upcr_shared_ptr_t dst,
                      upcr_shared_ptr_t src,
                      size_t nbytes,
                      upc_flag_t sync_mode
                      UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(GATHER, nbytes);
    #define UPCRI_PEVT_ARGS , &dst, &src, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_GATHER);
    UPCRI_COLL_GASNETIFY(gather,
				(GASNET_TEAM_ALL,
				 UPCRI_COLL_ONE_DST(dst),
				 UPCRI_COLL_MULTI_SRC(src),
				 nbytes, upcri_coll_syncmode(sync_mode)));
    upcri_pevt_end(GASP_UPC_ALL_GATHER);
    #undef UPCRI_PEVT_ARGS
}

void _upcr_all_gather_all(upcr_shared_ptr_t dst,
                          upcr_shared_ptr_t src,
                          size_t nbytes,
                          upc_flag_t sync_mode
                          UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(GATHER_ALL, nbytes);
    #define UPCRI_PEVT_ARGS , &dst, &src, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_GATHER_ALL);
    UPCRI_COLL_GASNETIFY(gather_all,
				(GASNET_TEAM_ALL,
				 UPCRI_COLL_MULTI_DST(dst),
				 UPCRI_COLL_MULTI_SRC(src),
				 nbytes, upcri_coll_syncmode(sync_mode)));
    upcri_pevt_end(GASP_UPC_ALL_GATHER_ALL);
    #undef UPCRI_PEVT_ARGS
}

void _upcr_all_exchange(upcr_shared_ptr_t dst,
                        upcr_shared_ptr_t src,
                        size_t nbytes,
                        upc_flag_t sync_mode
                        UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(EXCHANGE, nbytes);
    #define UPCRI_PEVT_ARGS , &dst, &src, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_EXCHANGE);
    UPCRI_COLL_GASNETIFY(exchange,
				(GASNET_TEAM_ALL,
				 UPCRI_COLL_MULTI_DST(dst),
				 UPCRI_COLL_MULTI_SRC(src),
				 nbytes, upcri_coll_syncmode(sync_mode)));
    upcri_pevt_end(GASP_UPC_ALL_EXCHANGE);
    #undef UPCRI_PEVT_ARGS
}

/* GASNet interface not yet (ever?) specified */
/* This one is an odd ball regardless, and may never follow the template */
void _upcr_all_permute(upcr_shared_ptr_t dst,
                       upcr_shared_ptr_t src,
                       upcr_pshared_ptr_t perm,
                       size_t nbytes,
                       upc_flag_t sync_mode
                       UPCRI_PT_ARG)
{
#if UPCRI_UPC_PTHREADS
    const int thread_count = upcri_mypthreads();
    const int first_thread = upcri_1stthread(gasnet_mynode());
    const int i_am_master = (upcri_mypthread() == 0);
#else
    const int thread_count = 1;
    const int first_thread = upcr_mythread();
    const int i_am_master = 1;
#endif
    #if UPCRI_GASP
      upcr_shared_ptr_t permtmp = upcr_pshared_to_shared(perm);
    #endif
    int i;
    sync_mode = upcri_coll_fixsync(sync_mode);
    UPCRI_TRACE_COLL(PERMUTE, nbytes);

    #define UPCRI_PEVT_ARGS , &dst, &src, &permtmp, nbytes, (int)sync_mode
    upcri_pevt_start(GASP_UPC_ALL_PERMUTE);

    if (!(sync_mode & UPC_IN_NOSYNC)) {
	UPCRI_SINGLE_BARRIER();
    }
    if (i_am_master) { /* One distinguished thread per node does all the work */
	for (i = 0; i < thread_count; ++i) {
	    /* XXX: use puti? */
	    upcr_thread_t dst_th = *((int *)upcri_pshared_to_remote_withthread(perm, first_thread+i));
	    void *dst_addr = upcri_shared_to_remote_withthread(dst, dst_th);
	    void *src_addr = upcri_shared_to_remote_withthread(src, first_thread+i);
    
	    if (upcri_thread_is_local(dst_th)) {
		UPCRI_UNALIGNED_MEMCPY(dst_addr, src_addr, nbytes);
	    } else {
		gasnet_put_nbi_bulk(upcri_thread_to_node(dst_th), dst_addr, src_addr, nbytes);
	    }
	}
        gasnet_wait_syncnbi_puts();
    }
    if (!(sync_mode & UPC_OUT_NOSYNC)) {
	UPCRI_SINGLE_BARRIER();
    }

    upcri_pevt_end(GASP_UPC_ALL_PERMUTE);
    #undef UPCRI_PEVT_ARGS
}

/*----------------------------------------------------------------------------*/

#define UPCRI_COLL_RED_TYPE	signed char
#define UPCRI_COLL_RED_SUFF	C
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	unsigned char
#define UPCRI_COLL_RED_SUFF	UC
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	signed short
#define UPCRI_COLL_RED_SUFF	S
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	unsigned short
#define UPCRI_COLL_RED_SUFF	US
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	signed int
#define UPCRI_COLL_RED_SUFF	I
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	unsigned int
#define UPCRI_COLL_RED_SUFF	UI
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	signed long
#define UPCRI_COLL_RED_SUFF	L
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	unsigned long
#define UPCRI_COLL_RED_SUFF	UL
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_INT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	float
#define UPCRI_COLL_RED_SUFF	F
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_NONINT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	double
#define UPCRI_COLL_RED_SUFF	D
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_NONINT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

#define UPCRI_COLL_RED_TYPE	UPCRI_COLL_LONG_DOUBLE
#define UPCRI_COLL_RED_SUFF	LD
#define UPCRI_COLL_RED_OPS	UPCRI_COLL_NONINT
#include "upcr_coll_templates.c"
#undef UPCRI_COLL_RED_TYPE
#undef UPCRI_COLL_RED_SUFF
#undef UPCRI_COLL_RED_OPS

/*----------------------------------------------------------------------------*/
