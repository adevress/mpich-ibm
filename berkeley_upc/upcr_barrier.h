/*
 * UPC barrier functions
 */

#ifndef UPCR_BARRIER_H
#define UPCR_BARRIER_H

#define UPCR_BARRIERFLAG_ANONYMOUS 1
#define UPCR_FINAL_BARRIER_VALUE (0xFFC1A43B) /* something a user is unlikely to choose */

#if UPCRI_UPC_PTHREADS

extern void upcri_barrier_init(); /* called by exactly 1 thread at startup */

/* A simple barrier among the local pthreads, returning non-zero to the last arrival */
#define upcri_pthread_barrier()\
        _upcri_pthread_barrier()
extern int _upcri_pthread_barrier();

#else

#define upcri_barrier_init()
#define upcri_pthread_barrier() ((void)0)

#endif  /* UPCRI_UPC_PTHREADS */



/* define calls to add threadinfo struct if using pthreads */
#define upcr_notify(barrierval, flags)		\
	(upcri_srcpos(), _upcr_notify(barrierval, flags UPCRI_PT_PASS))
#define upcr_wait(barrierval, flags)		\
	(upcri_srcpos(), _upcr_wait(barrierval, flags UPCRI_PT_PASS))
#define upcr_try_wait(barrierval, flags)	\
	(upcri_srcpos(), _upcr_try_wait(barrierval, flags UPCRI_PT_PASS))

void _upcr_notify(int barrierval, int flags UPCRI_PT_ARG);
void _upcr_wait(int barrierval, int flags UPCRI_PT_ARG);
int  _upcr_try_wait(int barrierval, int flags UPCRI_PT_ARG);

#endif /* UPCR_BARRIER_H */
