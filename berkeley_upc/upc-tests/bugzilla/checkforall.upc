#include <upc.h>
#include <stdio.h>

#define N 10
#define M 11

#define check(expr) do { \
  if (!(expr)) printf("ERROR: thread %i failed (%s) at %s:%i\n", \
      MYTHREAD, #expr, __FILE__, __LINE__); } while (0)

shared int A[N*N*THREADS];

shared [1] int *psi1 = (shared [1] int *)&A;
shared [N] int *psiN = (shared [N] int *)&A;
shared [] int *psiI0 = (shared [] int *)&A;
shared [] int *psiITm1;

int main() {
  int i,j,k;
  int cnt1, cnt2;

  psiITm1 = (shared [] int *)&(A[THREADS-1]);

  upc_forall( i=-N; i<N; i++; i) {
    if (i >= 0)
      check(i % THREADS == MYTHREAD);
    else
      check(((i % THREADS)+THREADS)%THREADS == MYTHREAD);
  }
  upc_barrier;
  upc_forall(i=0;i<N*THREADS;i++; &(psi1[i])) {
    check(upc_threadof(&(psi1[i])) == MYTHREAD);
    check(i % THREADS == MYTHREAD);
  }
  upc_barrier;
  upc_forall(i=0;i<N*N*THREADS;i++; &(psiN[i])) {
    check(upc_threadof(&(psiN[i])) == MYTHREAD);
    check((i/N) % THREADS == MYTHREAD);
  }
  upc_barrier;
  upc_forall(i=0;i<N*N;i++; &(psiI0[i])) {
    check(upc_threadof(&(psiI0[i])) == MYTHREAD);
    check(MYTHREAD == 0);
  }
  upc_barrier;
  upc_forall(i=0;i<N*N;i++; &(psiITm1[i])) {
    check(upc_threadof(&(psiITm1[i])) == MYTHREAD);
    check(MYTHREAD == THREADS-1);
  }
  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  for (i=0;i<N;i++) {
    cnt1++;
    upc_forall( j=0; j<M*THREADS; j++; j) {
      cnt2++;
      check(j % THREADS == MYTHREAD);
    }
  }
  check(cnt1 == N);
  check(cnt2 == N*M);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (i=0;i<M*THREADS;i++;i) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    for( j=0; j<N; j++) {
      cnt2++;
    }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (i=0;i<M*THREADS;i++;i) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    upc_forall( j=0; j<N; j++; j) {
      cnt2++;
    }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (k=0;k<1;k++;continue) {
  upc_forall (i=0;i<M*THREADS;i++;i) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    upc_forall( j=0; j<N; j++; j) {
      cnt2++;
    }
  }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (i=0;i<M*THREADS;i++;i) {
  upc_forall (k=0;k<1;k++;continue) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    upc_forall( j=0; j<N; j++; j) {
      cnt2++;
    }
  }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (i=0;i<M*THREADS;i++;i) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    upc_forall (k=0;k<1;k++;continue) {
    upc_forall( j=0; j<N; j++; j) {
      cnt2++;
    }
    }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  cnt1 = 0;
  cnt2 = 0;
  upc_forall (i=0;i<M*THREADS;i++;i) {
    cnt1++;
    check(i % THREADS == MYTHREAD);
    upc_forall( j=0; j<N; j++; j) {
    upc_forall (k=0;k<1;k++;continue) {
      cnt2++;
    }
    }
  }
  check(cnt1 == M);
  check(cnt2 == M*N);

  upc_barrier;

  printf("done.\n");
  return 0;
}
