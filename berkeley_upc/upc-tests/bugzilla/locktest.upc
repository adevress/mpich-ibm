// UPC lock correctness tester
// Copyright 2004, Dan Bonachea

// all data accesses are properly synchronized, so everything should work in both
// strict *and* relaxed mode - use relaxed because UPC compilers are more likely
// to mess that up
#include <upc_relaxed.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

upc_lock_t * shared threadlock[THREADS];
shared int threadflag[THREADS];
shared int allflag;
shared int errs[THREADS];

void doWork() { /* stall for awhile */
  static double x = 1.1;
  for (int i = 0; i < 1000; i++) {
    sleep(0); /* yield */
  #if defined(__BERKELEY_UPC__) || defined(__GNUC__)
    upc_poll();
  #endif
    x *= 1.001;
  }
}

#define CHECK(location, expectedval) do { \
  int actualval = (location); \
  if (actualval != (expectedval))  { \
    printf("%i: ERROR: %s == %i, should be %i, at %s:%i\n", \
        MYTHREAD, #location, actualval, expectedval, __FILE__, __LINE__); \
    errs[MYTHREAD]++; \
  } \
 } while (0)

#define MSG0(s) do { \
 if (MYTHREAD == 0) { \
  printf(s); fflush(stdout); \
 } } while (0)

int main(int argc, char **argv) {
  int iters = 0;

  if (argc > 1) { iters = atoi(argv[1]); } 
  if (iters < 1) iters = 100;
  if (MYTHREAD == 0) { printf("Running %i iterations of upc_lock test...\n", iters); fflush(stdout); }

  upc_lock_t *alllock = upc_all_lock_alloc();
  threadlock[MYTHREAD] = upc_global_lock_alloc();

  allflag = -1;
  threadflag[MYTHREAD] = -1;
  upc_barrier;
  MSG0("*** testing mutual exclusion...\n");
  errs[MYTHREAD] = 0;
  upc_barrier;
  for (int i = 0; i < iters; i++) { 
    int partner = (i + MYTHREAD)%THREADS;
    if (i%THREADS == MYTHREAD) {
      upc_lock(alllock);
        CHECK(allflag, -1);
        allflag = MYTHREAD;
        upc_fence;
        doWork();
        upc_fence;
        CHECK(allflag, MYTHREAD);
        allflag = -1;
      upc_unlock(alllock);
    }
    upc_lock(threadlock[partner]);
        CHECK(threadflag[partner], -1);
        threadflag[partner] = MYTHREAD;
        upc_fence;
        doWork();
        upc_fence;
        CHECK(threadflag[partner], MYTHREAD);
        threadflag[partner] = -1;
    upc_unlock(threadlock[partner]);
  }
  upc_barrier;
  if (MYTHREAD == 0) {
    int numerrs = 0;
    for (int i=0; i < THREADS; i++) {
      numerrs += errs[i];
    }
    if (numerrs == 0) printf(" passed\n");
    else printf(" FAILED: %i errors\n", numerrs);
  }
  upc_barrier;

  iters *= 10; /* use more iters for remaining tests */

  upc_barrier;
  MSG0("*** testing all_lock_alloc pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* heavy alloc/free traffic */
    upc_lock_t *alock = upc_all_lock_alloc();
    if (MYTHREAD == (i%THREADS)) upc_lock_free(alock);
  }

  upc_barrier;
  MSG0("*** testing global_lock_alloc pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* heavy alloc/free traffic */
    upc_lock_t *alock = upc_global_lock_alloc();
    upc_lock_free(alock);
  }

  upc_barrier;
  MSG0("*** testing all_alloc/lock/unlock/free pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* heavy lock/unlock traffic on fresh locks */
    upc_lock_t *alock = upc_all_lock_alloc();
    if (MYTHREAD == (i%THREADS)) {
      upc_lock(alock);
      upc_unlock(alock);
      upc_lock_free(alock);
    }
  }

  upc_barrier;
  MSG0("*** testing global_alloc/lock/unlock/free pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* heavy lock/unlock traffic on fresh locks */
    upc_lock_t *alock = upc_global_lock_alloc();
    upc_lock(alock);
    upc_unlock(alock);
    upc_lock_free(alock);
  }

  upc_barrier;
  MSG0("*** testing global_alloc/lock/free pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* free without release (legal by spec) */
    upc_lock_t *alock = upc_global_lock_alloc();
    upc_lock(alock);
    upc_lock_free(alock);
  }

  upc_barrier;
  MSG0("*** testing all_alloc/lock/free pounding test...\n");
  upc_barrier;
  for (int i = 0; i < iters; i++) { /* one thread acquires, different one frees */
    upc_lock_t *alock = upc_all_lock_alloc();
    if (MYTHREAD == (i%THREADS)) upc_lock(alock);
    upc_barrier;
    if (MYTHREAD == ((i+1)%THREADS)) upc_lock_free(alock);
  }
  upc_barrier;
  MSG0("done.\n");
  return 0; 
}
