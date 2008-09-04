/* UPC allocation performance tester, v0.1
   Copyright 2003, Dan Bonachea <bonachea@cs.berkeley.edu>
   This program is public domain - permission is granted for any and all uses.
 */
#include <upc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>
#include <bupc_collectivev.h>

#ifndef MIN
#define MIN(x,y)  ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y)  ((x)>(y)?(x):(y))
#endif


int giters=0;
int threshwarning = 0;

static int64_t mygetMicrosecondTimeStamp(void) {
    int64_t retval;
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
	perror("gettimeofday");
	abort();
    }
    retval = ((int64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
    return retval;
}

#define TIME() mygetMicrosecondTimeStamp()
#define TIMER_THRESHOLD_US 10000
#define ZEROMSG(s) do {  \
    if (MYTHREAD == 0) { \
      printf(s);         \
      fflush(stdout);    \
    }                    \
  } while(0)
static void report(int64_t timeus, const char *msg, int zeroonly) {
  const char *threshmark = "";
  double myus = ((double)timeus)/giters;
  double minus = bupc_allv_reduce(double, myus, 0, UPC_MIN);
  double maxus = bupc_allv_reduce(double, myus, 0, UPC_MAX);
  double avgus = bupc_allv_reduce(double, myus, 0, UPC_ADD);
  avgus /= THREADS;
  if (!MYTHREAD) {
    double threshval = (zeroonly?maxus:minus);
    if (threshval*giters < TIMER_THRESHOLD_US) {
      threshmark = "(*)";
      threshwarning = 1;
    }
    printf(" %-40s: Total=%6.3f sec  Avg/Min/Max=%6.3f %6.3f %6.3f us %s\n",
      msg, ((double)threshval)*giters/1000000, avgus, minus, maxus, threshmark);
    fflush(stdout);
  }
}

/* Usage: lockperf <iters>
 * Runs a performance test of various UPC lock-related functions
 *  barrier performance is also shown for comparison purposes
 * Average performance is reported 
 */

upc_lock_t * shared alllocks[THREADS];

int main(int argc, char **argv) {
  if (THREADS > 1 && THREADS % 2 != 0) { 
    if (!MYTHREAD) printf("ERROR: this test requires a unary or even thread count\n"); 
    exit(1);
  }
  if (THREADS < 4 && !MYTHREAD) printf("WARNING: this test provides the most complete results with 4+ threads\n"); 

  int64_t start,total;
  int i = 0;
  int iters = 0;

  if (argc > 1) iters = atoi(argv[1]);
  if (iters <= 0) iters = 10000;
  giters = iters;

  if (MYTHREAD == 0) {
      printf("running lockperf test with %i iterations, %i threads\n", iters, THREADS);
      printf("-------------------------------------------------------------------------------\n");
      fflush(stdout);
  }

  /* create locks and exchange information */
  upc_lock_t *mylock = upc_global_lock_alloc(); 
  alllocks[MYTHREAD] = mylock;
  upc_barrier;
  int partnerid = (THREADS == 1 ? 0 : MYTHREAD^1);

  /* select affinity for the contended lock test
   * each thread pair selects a unique remote affinity 
   * which is not local to either thread in the pair
   */
  int centralized_partnerid = (THREADS <= 2 ? 0 :
         ((MYTHREAD | 0x1) + (THREADS/2)) % THREADS );

  /* fetch lock pointers from remote and cache them in local vars */
  upc_lock_t *partnerlock = alllocks[partnerid]; 
  upc_lock_t *zerolock = alllocks[0]; 
  upc_lock_t *centrallock = alllocks[centralized_partnerid]; 

  /* ----------------------------------------------------- 
   * Warm up the locks
   * pay any first-call costs so that we 
   * measure steady-state behavior
   * ----------------------------------------------------- */
  
    for (i=0; i < MIN(10,iters/10); i++) {
      upc_lock(mylock); 
      upc_unlock(mylock); 
      upc_lock(partnerlock); 
      upc_unlock(partnerlock); 
    }
    upc_barrier;

  /* ----------------------------------------------------- */
    ZEROMSG("\n    --- Timing upc_lock/upc_unlock latency ---\n\n");
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_lock(mylock); 
      upc_unlock(mylock); 
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "local lock, uncontended", 0);
  /* ----------------------------------------------------- */
  if (THREADS > 1) {
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_lock(partnerlock); 
      upc_unlock(partnerlock); 
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "remote lock, uncontended", 0);
  }
  /* ----------------------------------------------------- */
  if (THREADS > 1) {
    char msg[255];
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_lock(centrallock); 
      upc_unlock(centrallock); 
    }
    total = TIME() - start;
    upc_barrier;
    sprintf(msg, "lock contended by 2%s", 
            (THREADS >= 4 ? " remote threads" : " threads, one local"));
    report(total, msg, 0);
  }
  /* ----------------------------------------------------- */
  if (THREADS >= 4) {
    char msg[255];
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_lock(zerolock); 
      upc_unlock(zerolock); 
    }
    total = TIME() - start;
    upc_barrier;
    sprintf(msg, "lock contended by %i threads, one local", THREADS);
    report(total, msg, 0);
  }
  /* ----------------------------------------------------- */
    ZEROMSG("\n    --- Timing lock allocation/deallocation ---\n\n");
  /* ----------------------------------------------------- */
    upc_lock_t **lockarr = malloc(iters*sizeof(upc_lock_t *));

    upc_barrier; /* warm up */
    for (i=0; i < iters; i++) lockarr[i] = upc_global_lock_alloc();
    for (i=0; i < iters; i++) upc_free(lockarr[i]);
    for (i=0; i < MIN(10,iters/10); i++) lockarr[i] = upc_all_lock_alloc();
    if (!MYTHREAD) {
      for (i=0; i < MIN(10,iters/10); i++) upc_free(lockarr[i]);
    }
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      lockarr[i] = upc_global_lock_alloc();
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "upc_global_lock_alloc", 0);
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_lock_free(lockarr[i]);
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "upc_lock_free local", 0);
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      lockarr[i] = upc_all_lock_alloc();
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "upc_all_lock_alloc", 0);
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    if (!MYTHREAD) {
      for (i=0; i < iters; i++) {
        upc_lock_free(lockarr[i]);
      }
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "upc_lock_free from T0", 1);
  /* ----------------------------------------------------- */
    ZEROMSG("\n    --- Timing upc_barrier ---\n\n");
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_barrier;
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "Anon. Barrier latency", 0);
  /* ----------------------------------------------------- */
    upc_barrier;
    start = TIME();
    for (i=0; i < iters; i++) {
      upc_barrier i;
    }
    total = TIME() - start;
    upc_barrier;
    report(total, "Named Barrier latency", 0);
  /* ----------------------------------------------------- */

  upc_barrier;
  /* cleanup */
  free(lockarr);
  upc_lock_free(mylock);
  ZEROMSG("-------------------------------------------------------------------------------\n");
  if (threshwarning) {
    ZEROMSG(" (*) = Total time too short relative to timer granularity/overhead.\n"
            "       Number should not be trusted - run more iterations.\n");
  }
  ZEROMSG("done.\n");

  upc_barrier;

  return 0;
}
