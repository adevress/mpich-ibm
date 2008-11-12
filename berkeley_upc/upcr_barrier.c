/* $Source: /var/local/cvs/upcr/upcr_barrier.c,v $
 * $Date: 2008/09/22 23:50:06 $
 * $Revision: 1.39 $
 * Description: UPC barrier implementation on GASNet
 * Copyright 2002, Dan Bonachea <bonachea@cs.berkeley.edu>
 */

#include <upcr.h>
#include <upcr_internal.h>

/*----------------------------------------------------------------------------*/

#ifdef UPCRI_UPC_PTHREADS

/* init to { upcr_numthreads, upcr_numthreads } */
static char _cache_pad1[256];
static gasnett_atomic_t barrier_count[2];
static char _cache_pad2[256];

static int volatile notify_done[2] = {0,0};
static int volatile wait_done[2] = {0,0};
static gasnett_mutex_t barrier_lock = GASNETT_MUTEX_INITIALIZER;

static int barrier_consensus_value = 0;
static int barrier_flags = 0;
static int barrier_local_mismatch = 0;

typedef struct {
  gasnett_atomic_t	count;
  volatile int		done;
  gasnett_cond_t	cond;
  gasnett_mutex_t	lock;
} upcri_pthread_barrier_t;

static upcri_pthread_barrier_t upcri_pthread_barrier_var[2];
static int volatile upcri_pthread_barrier_phase = 0;

void 
upcri_barrier_init() 
{
    /* called by exactly 1 thread at startup */
    int i;

    gasnett_atomic_set(&barrier_count[0], upcri_mypthreads(), 0);
    gasnett_atomic_set(&barrier_count[1], upcri_mypthreads(), 0);

    for (i = 0; i < 2; ++i) {
	gasnett_atomic_set(&(upcri_pthread_barrier_var[i].count), upcri_mypthreads(), 0);
	upcri_pthread_barrier_var[i].done = 0;
	gasnett_cond_init(&upcri_pthread_barrier_var[i].cond);
	gasnett_mutex_init(&upcri_pthread_barrier_var[i].lock);
    }
}

int
_upcri_pthread_barrier() {
    upcri_pthread_barrier_t *state = &(upcri_pthread_barrier_var[upcri_pthread_barrier_phase]);
    int last;

    if_pf (upcri_polite_wait) {
	gasnett_mutex_lock(&state->lock);
    }

    /* The REL ensures everything written by this thread prior to entering the barrier
     * will be visible to all threads before any thread can leave the barrier.
     */
    last = gasnett_atomic_decrement_and_test(&(state->count), GASNETT_ATOMIC_REL | GASNETT_ATOMIC_ACQ_IF_TRUE);
    if (last) {
	upcri_pthread_barrier_t *tmp;

	upcri_pthread_barrier_phase = !upcri_pthread_barrier_phase;
	tmp = &(upcri_pthread_barrier_var[upcri_pthread_barrier_phase]);
	tmp->done = 0;
	gasnett_atomic_set(&(tmp->count), upcri_mypthreads(), GASNETT_ATOMIC_WMB_POST);

	state->done = 1;

	if_pf (upcri_polite_wait) {
	    gasnett_cond_broadcast(&state->cond);
	}
    } else {
	do {
	    if_pf (upcri_polite_wait) {
		gasnett_cond_wait(&state->cond, &state->lock);
	    } else {
		gasnett_spinloop_hint();	/* XXX: gasnet_AMPoll() ? */
	    }
	} while (!state->done);
    }

    if_pf (upcri_polite_wait) {
	gasnett_mutex_unlock(&state->lock);
    }
    
    /* Ensure all values written by any thread before entering the barrer will be
     * visible on this thread before we return.  This also includes the values of the
     * barrier structure that will be observed on our next pass through this function.
     */
    gasnett_local_rmb();

    return last;
}

void 
_upcr_notify(int barrierval, int flags UPCRI_PT_ARG)
{
    upcri_barrier_t * const bar = &(upcri_auxdata()->barrier_info);
    const int phase = (bar->barrier_phase = !bar->barrier_phase);

    #define UPCRI_PEVT_ARGS , !(flags & UPCR_BARRIERFLAG_ANONYMOUS), barrierval
    upcri_pevt_start(GASP_UPC_NOTIFY);

    if_pf (bar->barrier_state == upcr_barrierstate_insidebarrier)
	upcri_err("misordered call to barrier notify");

    bar->barrier_value = barrierval;
    bar->barrier_flags = flags;
    bar->barrier_state = upcr_barrierstate_insidebarrier;
    if (upcri_mypthreads() == 1) {
      gasnet_barrier_notify(barrierval, flags);
    } else {
	int lastone = gasnett_atomic_decrement_and_test(&barrier_count[phase], GASNETT_ATOMIC_REL | GASNETT_ATOMIC_ACQ_IF_TRUE);

	if (lastone) {		/* we are the last thread to notify */
	    int i;
	    int valueset = 0;
	    int mismatch = 0;

            barrier_flags = 0;
	    for (i = 0; i < upcri_mypthreads(); i++) {
		upcri_barrier_t * const his_bar = &(upcri_hisauxdata(i)->barrier_info);
		if (his_bar->barrier_flags != UPCR_BARRIERFLAG_ANONYMOUS) {
		    if (!valueset) {
			barrier_consensus_value = his_bar->barrier_value;
			valueset = 1;
		    } else {
			if_pf (his_bar->barrier_value != barrier_consensus_value)
			    mismatch = 1;
		    }
		}
	    }			/*  for */
	    if (!valueset)
		barrier_flags = GASNET_BARRIERFLAG_ANONYMOUS;
	    else if_pf (mismatch) {
                barrier_local_mismatch = 1;
		barrier_flags = GASNET_BARRIERFLAG_MISMATCH;
            }
	    /*  do the global gasnet barrier notify */
	    gasnet_barrier_notify(barrier_consensus_value, barrier_flags);
	    /*  signal that notify is complete */
            notify_done[!phase] = 0;
	    gasnett_atomic_set(&barrier_count[phase], upcri_mypthreads(), GASNETT_ATOMIC_WMB_POST);
            notify_done[phase] = 1;
	}
  }

  upcri_pevt_end(GASP_UPC_NOTIFY);
  #undef UPCRI_PEVT_ARGS
}


static int 
upcr_wait_internal(int barrierval, int flags, int block UPCRI_PT_ARG)
{
    upcri_barrier_t * const bar = &(upcri_auxdata()->barrier_info);
    int phase = bar->barrier_phase;

    if_pf (bar->barrier_state == upcr_barrierstate_outsidebarrier)
	upcri_err("misordered call to barrier wait");
    if_pf (bar->barrier_value != barrierval || bar->barrier_flags != flags)
	upcri_err("notify/wait barrier values don't match");

    if (upcri_mypthreads() == 1) {
        int retval;
        if (block) retval = gasnet_barrier_wait(barrierval, flags);
        else {
          retval = gasnet_barrier_try(barrierval, flags);
          if (retval == GASNET_ERR_NOT_READY) return 0;
        }
	if_pf (retval == GASNET_ERR_BARRIER_MISMATCH)
          upcri_err("barrier mismatch detected");
#if 0
/* PHH 06 AUG 2004
   I'm disabling the single-node optimization because it is slightly incorrect.
   The problem is that threads other than thread 0 can run ahead while
   thread 0 is performing the gasnet_barrier_wait().
   If one of the other threads enters a collective it could make a
   gasnet_barrier_notify() call before thread 0 calls gasnet_barrier_wait().
*/
    } else if (gasnet_nodes() == 1) { /* streamlined code for single-node */
      if (!notify_done[phase]) { /*  not everyone has notified yet */
        if (!block) return 0;
	while (!notify_done[phase]) {
	    if (upcri_polite_wait) gasnett_sched_yield(); /* spin-wait */
            gasnett_spinloop_hint();
        }
      }
      gasnett_local_rmb();
      if_pf (barrier_local_mismatch) 
        upcri_err("barrier mismatch detected");
      if (upcri_mypthread() == 0) { /* just a formality to maintain the tracing and GASNet checks */
        int retval = gasnet_barrier_wait(barrier_consensus_value, barrier_flags);
        upcri_assert(retval == GASNET_OK);
      }
#endif
    } else if (!wait_done[phase]) {	/* redundant, but reduces lock contention */
	if (block) {
            while (gasnett_mutex_trylock(&barrier_lock) == EBUSY) {
              /* required to prevent deadlock, also reduces lock contention */
              if (wait_done[phase]) goto done; 
	      if (upcri_polite_wait) gasnett_sched_yield(); /* spin-wait */
              gasnett_spinloop_hint();
            }
        } else {
	    int retval = gasnett_mutex_trylock(&barrier_lock);

	    if (retval == EBUSY)
		return 0;
	}

	if (!wait_done[phase]) {
	    int retval;

	    if (!notify_done[phase]) { /*  not everyone has notified yet */
                if (!block) {
	          gasnett_mutex_unlock(&barrier_lock);
                  return 0;
                }
		while (!notify_done[phase]) {
	          if (upcri_polite_wait) gasnett_sched_yield(); /* spin-wait */
                  gasnett_spinloop_hint();
                }
	    }
	    gasnett_local_rmb(); /* ensure we will observe the values written by the notify */

	    if (block)		/*  do the global gasnet barrier wait */
		retval = gasnet_barrier_wait(barrier_consensus_value, barrier_flags);
	    else {
		retval = gasnet_barrier_try(barrier_consensus_value, barrier_flags);
                if (retval == GASNET_ERR_NOT_READY) {
	          gasnett_mutex_unlock(&barrier_lock);
		  return 0;
                }
	    }
	    if_pf (retval == GASNET_ERR_BARRIER_MISMATCH)
		upcri_err("barrier mismatch detected");

	    wait_done[!phase] = 0;
            gasnett_local_wmb();
  	    wait_done[phase] = 1;
	}
	gasnett_mutex_unlock(&barrier_lock);
    }
done:
    gasnett_local_rmb();
    #if PLATFORM_COMPILER_PATHSCALE
      /* bug1337: extraneous deref is workaround for pathscale optimizer bug */
      *(volatile int *)&(bar->barrier_state) = (int)upcr_barrierstate_outsidebarrier; 
    #else
      bar->barrier_state = upcr_barrierstate_outsidebarrier; 
    #endif
    return 1;
}

void 
_upcr_wait(int barrierval, int flags UPCRI_PT_ARG)
{
  #define UPCRI_PEVT_ARGS , !(flags & UPCR_BARRIERFLAG_ANONYMOUS), barrierval
  upcri_pevt_start(GASP_UPC_WAIT);

    upcr_wait_internal(barrierval, flags, 1 UPCRI_PT_PASS);

  upcri_pevt_end(GASP_UPC_WAIT);
  #undef UPCRI_PEVT_ARGS
}

int 
_upcr_try_wait(int barrierval, int flags UPCRI_PT_ARG)
{
    return upcr_wait_internal(barrierval, flags, 0 UPCRI_PT_PASS);
}

#else	/* !HAVE_PTHREADS */

void 
_upcr_notify(int barrierval, int flags)
{
  #define UPCRI_PEVT_ARGS , !(flags & UPCR_BARRIERFLAG_ANONYMOUS), barrierval
  upcri_pevt_start(GASP_UPC_NOTIFY);

    upcri_auxdata()->barrier_info.barrier_phase ^= 1;
    gasnet_barrier_notify(barrierval, flags);

  upcri_pevt_end(GASP_UPC_NOTIFY);
  #undef UPCRI_PEVT_ARGS
}

void 
_upcr_wait(int barrierval, int flags)
{
  #define UPCRI_PEVT_ARGS , !(flags & UPCR_BARRIERFLAG_ANONYMOUS), barrierval
  upcri_pevt_start(GASP_UPC_WAIT);

    if (gasnet_barrier_wait(barrierval, flags) == GASNET_ERR_BARRIER_MISMATCH)
        upcri_err("barrier mismatch detected");

  upcri_pevt_end(GASP_UPC_WAIT);
  #undef UPCRI_PEVT_ARGS
}

int 
_upcr_try_wait(int barrierval, int flags)
{
    int retval = gasnet_barrier_try(barrierval, flags);

    if (retval == GASNET_ERR_NOT_READY)
	return 0;
    if (retval == GASNET_ERR_BARRIER_MISMATCH)
	upcri_err("barrier mismatch detected");
    return 1;
}

#endif
