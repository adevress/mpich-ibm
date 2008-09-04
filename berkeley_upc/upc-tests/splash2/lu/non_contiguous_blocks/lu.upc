/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/*  Parallel dense blocked LU factorization (no pivoting)                */
/*                                                                       */
/*  This version contains one dimensional arrays in which the matrix     */
/*  to be factored is stored.                                            */
/*                                                                       */
/*  Command line options:                                                */
/*                                                                       */
/*  -nN : Decompose NxN matrix.                                          */
/*  -pP : P = number of processors.                                      */
/*  -bB : Use a block size of B. BxB elements should fit in cache for    */
/*        good performance. Small block sizes (B=8, B=16) work well.     */
/*  -s  : Print individual processor timing statistics.                  */
/*  -t  : Test output.                                                   */
/*  -o  : Print out matrix values.                                       */
/*  -h  : Print out command line options.                                */
/*                                                                       */
/*  Note: This version works under both the FORK and SPROC models        */
/*                                                                       */
/*************************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include "upc.h"
#include "lu.h"
//MAIN_ENV

#define MAXRAND                         32767.0
#define DEFAULT_N                         128
#define DEFAULT_P                           1
#define DEFAULT_B                          16
#define min(a,b) ((a) < (b) ? (a) : (b))


//Everthing in globalmemory corrspond to shared types
shared double t_in_solve[THREADS];
shared double t_in_mod[THREADS];
shared double t_in_bar[THREADS];
shared double t_in_fac[THREADS];
shared double completion[THREADS];
shared struct timeval rf;
shared struct timeval  rs;
shared struct timeval done;
shared int id;
upc_lock_t *idlock;

/*
struct GlobalMemory {
  //double *t_in_fac;
  
  shared double *t_in_solve;
  shared double *t_in_mod;
  double *t_in_bar;
  double *completion;
  struct timeval starttime;
  struct timeval rf;
  struct timeval rs;
  struct timeval done;
  int id;
  //BARDEC(start)
  //LOCKDEC(idlock)
  upc_lock_t *idlock;
};
*/

//shared struct GlobalMemory *Global;

struct LocalCopies {
  double t_in_fac;
  double t_in_solve;
  double t_in_mod;
  double t_in_bar;
};

shared int n = DEFAULT_N;          /* The size of the matrix */
shared int block_size = DEFAULT_B; /* Block dimension */
int nblocks;                /* Number of blocks in each dimension */
int num_rows;               /* Number of processors per row of processor grid */
int num_cols;               /* Number of processors per col of processor grid */
//double *a;                  /* a = lu; l and u both placed back in a */
shared double *a;
//double *rhs;
shared double *rhs;
int *proc_bytes;            /* Bytes to malloc per processor to hold blocks
                               of A*/
int test_result = 0;        /* Test result of factorization? */
int doprint = 0;            /* Print out matrix values? */
int dostats = 0;            /* Print out individual processor statistics? */

void InitA();
void SlaveStart();
void OneSolve(int, int, shared double *, int, int);
void lu0(shared double *,int, int);
void bdiv(shared double *, shared double *, int, int, int, int);
void bmodd(shared double *, shared double*, int, int, int, int);
void bmod(shared double *, shared double *, shared double *, int, int, int, int);
void daxpy(shared double *, shared double *, int, double);
int BlockOwner(int, int);
void lu(int, int, int, struct LocalCopies *, int);
double TouchA(int, int);
void PrintA();
void CheckResult();
void printerr(const char *);

#define CLOCK(x) gettimeofday(&(x), NULL)

float calc_time(struct timeval tp_1st, struct timeval tp_2nd) {
  
  float diff = (tp_2nd.tv_sec-tp_1st.tv_sec) * 1000000.0 +
    (tp_2nd.tv_usec-tp_1st.tv_usec) ;
  return diff / 1000000.0;
}

int main(int argc, char* argv[])

{
  int i, j;
  int ch;
  double mint, maxt, avgt;
  double min_fac, min_solve, min_mod, min_bar;
  double max_fac, max_solve, max_mod, max_bar;
  double avg_fac, avg_solve, avg_mod, avg_bar;
  int proc_num;
  struct timeval start;

  CLOCK(start);

  if (!MYTHREAD) {
    
    while ((ch = getopt(argc, argv, "n:p:b:cstoh")) != -1) {
      switch(ch) {
      case 'n': n = atoi(optarg); break;
      case 'b': block_size = atoi(optarg); break;
      case 's': dostats = 1; break;
      case 't': test_result = !test_result; break;
      case 'o': doprint = !doprint; break;
      case 'h': 
	printf("Usage: LU <options>\n\n");
	printf("options:\n");
	printf("  -nN : Decompose NxN matrix.\n");
	printf("  -bB : Use a block size of B. BxB elements should fit in cache for \n");
	printf("        good performance. Small block sizes (B=8, B=16) work well.\n");
	printf("  -c  : Copy non-locally allocated blocks to local memory before use.\n");
	printf("  -s  : Print individual processor timing statistics.\n");
	printf("  -t  : Test output.\n");
	printf("  -o  : Print out matrix values.\n");
	printf("  -h  : Print out command line options.\n\n");
	printf("Default: LU -n%1d -p%1d -b%1d\n",
	       DEFAULT_N,DEFAULT_P,DEFAULT_B);
	exit(0);
	break;
      }
    }

    printf("\n");	
    printf("Blocked Dense LU Factorization\n");
    printf("     %d by %d Matrix\n",n,n);
    printf("     %d Processors\n", THREADS);
    printf("     %d by %d Element Blocks\n",block_size,block_size);
    printf("\n");
    printf("\n");
  }

  upc_barrier;
    
  num_rows = (int) sqrt((double) THREADS);
  for (;;) {
    num_cols = THREADS/num_rows;
    if (num_rows*num_cols == THREADS)
      break;
    num_rows--;
  }
  nblocks = n/block_size;
  if (block_size * nblocks != n) {
    nblocks++;
  }
  
  //a = (double *) G_MALLOC(n*n*sizeof(double));
  a = (shared double *) upc_all_alloc(n*n, sizeof(double));

  //rhs = (double *) G_MALLOC(n*sizeof(double));
  rhs = (shared double*) upc_all_alloc(n, sizeof(double));

  //Global = (struct GlobalMemory *) G_MALLOC(sizeof(struct GlobalMemory));
  /*
    Global->t_in_fac = (double *) G_MALLOC(P*sizeof(double));
    Global->t_in_mod = (double *) G_MALLOC(P*sizeof(double));
    Global->t_in_solve = (double *) G_MALLOC(P*sizeof(double));
    Global->t_in_bar = (double *) G_MALLOC(P*sizeof(double));
    Global->completion = (double *) G_MALLOC(P*sizeof(double));
  */

/* POSSIBLE ENHANCEMENT:  Here is where one might distribute the a
   matrix data across physically distributed memories in a 
   round-robin fashion as desired. */

  //BARINIT(Global->start);
  //LOCKINIT(Global->idlock);
  idlock = upc_all_lock_alloc();
  //Global->id = 0;
  if (MYTHREAD == 0) 
    id = 0;

  //Fork off code is unnecessary due to spmd model
  /*
  for (i=1; i<P; i++) {
    CREATE(SlaveStart)
  }
  */

  InitA();
  if (MYTHREAD == 0 && doprint) {
    printf("Matrix before decomposition:\n");
    PrintA();
  }


  //SlaveStart(MyNum);
  SlaveStart();

  upc_barrier;
  //WAIT_FOR_END(P-1)

  if (MYTHREAD == 0) {
    if (doprint) {
      printf("\nMatrix after decomposition:\n");
      PrintA();
    }
    
    if (dostats) {
      maxt = avgt = mint = completion[0];
      for (i=1; i<THREADS; i++) {
	if (completion[i] > maxt) {
	  maxt = completion[i];
	}
	if (completion[i] < mint) {
	  mint = completion[i];
	}
	avgt += completion[i];
      }
      avgt = avgt / THREADS;
      
      min_fac = max_fac = avg_fac = t_in_fac[0];
      min_solve = max_solve = avg_solve = t_in_solve[0];
      min_mod = max_mod = avg_mod = t_in_mod[0];
      min_bar = max_bar = avg_bar = t_in_bar[0];
      
      for (i=1; i<THREADS; i++) {
	if (t_in_fac[i] > max_fac) {
	  max_fac = t_in_fac[i];
	}
	if (t_in_fac[i] < min_fac) {
	  min_fac = t_in_fac[i];
	}
	if (t_in_solve[i] > max_solve) {
	  max_solve = t_in_solve[i];
	}
	if (t_in_solve[i] < min_solve) {
	  min_solve = t_in_solve[i];
	}
	if (t_in_mod[i] > max_mod) {
	  max_mod = t_in_mod[i];
	}
	if (t_in_mod[i] < min_mod) {
	  min_mod = t_in_mod[i];
	}
	if (t_in_bar[i] > max_bar) {
	  max_bar = t_in_bar[i];
	}
	if (t_in_bar[i] < min_bar) {
	  min_bar = t_in_bar[i];
	}
	avg_fac += t_in_fac[i];
	avg_solve += t_in_solve[i];
	avg_mod += t_in_mod[i];
	avg_bar += t_in_bar[i];
      }
      avg_fac = avg_fac/THREADS;
      avg_solve = avg_solve/THREADS;
      avg_mod = avg_mod/THREADS;
      avg_bar = avg_bar/THREADS;
    }
    printf("                            PROCESS STATISTICS\n");
    printf("              Total      Diagonal     Perimeter      Interior       Barrier\n");
    printf(" Proc         Time         Time         Time           Time          Time\n");
    printf("    0    %10.6f    %10.6f    %10.6f    %10.6f    %10.6f\n",
	   completion[0],t_in_fac[0],
	   t_in_solve[0],t_in_mod[0],
	   t_in_bar[0]);
    if (dostats) {
      for (i=1; i<THREADS; i++) {
	printf("  %3d    %4.6f    %4.6f    %4.6f    %4.6f    %4.6f\n",
	       i,completion[i],t_in_fac[i],
	       t_in_solve[i],t_in_mod[i],
	       t_in_bar[i]);
      }
      printf("  Avg    %10.6f    %10.6f    %10.6f    %10.6f    %10.6f\n",
	     avgt,avg_fac,avg_solve,avg_mod,avg_bar);
      printf("  Min    %10.6f    %10.6f    %10.6f    %10.6f    %10.6f\n",
	     mint,min_fac,min_solve,min_mod,min_bar);
      printf("  Max    %10.6f    %10.6f    %10.6f    %10.6f    %10.6f\n",
	     maxt,max_fac,max_solve,max_mod,max_bar);
    }
    printf("\n");
    printf("                            TIMING INFORMATION\n");
    //printf("Start time                        : %16d\n",
    //       starttime);
    //printf("Initialization finish time        : %16d\n",
    //       rs);
    //printf("Overall finish time               : %16d\n",
    //       rf);
    printf("Total time with initialization    : %4.6f\n",
	   calc_time(start, rf));
    printf("Total time without initialization : %4.6f\n",
	   calc_time(rs, rf));
    printf("\n");
    
    if (test_result) {
      printf("                             TESTING RESULTS\n");
      CheckResult();
    }
    
  }

  return 0;
}
	   
void SlaveStart()

{

/* POSSIBLE ENHANCEMENT:  Here is where one might pin processes to
   processors to avoid migration */

  OneSolve(n, block_size, a, MYTHREAD, dostats);
}


void OneSolve(n, block_size, a, MyNum, dostats)

shared double *a;
int n;
int block_size;
int MyNum;
int dostats;

{
  unsigned int i;
  struct timeval myrs, myrf, mydone;
  struct LocalCopies *lc;

  lc = (struct LocalCopies *) malloc(sizeof(struct LocalCopies));
  if (lc == NULL) {
    fprintf(stderr,"Proc %d could not malloc memory for lc\n",MyNum);
    exit(-1);
  }
  lc->t_in_fac = 0.0;
  lc->t_in_solve = 0.0;
  lc->t_in_mod = 0.0;
  lc->t_in_bar = 0.0;

  /* barrier to ensure all initialization is done */
  //BARRIER(Global->start, P);
  upc_barrier;

  /* to remove cold-start misses, all processors begin by touching a[] */
  TouchA(block_size, MyNum);

  //BARRIER(Global->start, P);
  upc_barrier;

/* POSSIBLE ENHANCEMENT:  Here is where one might reset the
   statistics that one is measuring about the parallel execution */

  if ((MyNum == 0) || (dostats)) {
    CLOCK(myrs);
  }

  lu(n, block_size, MyNum, lc, dostats);

  if ((MyNum == 0) || (dostats)) {
    CLOCK(mydone);
  }

  //BARRIER(Global->start, P);
  upc_barrier;

  if ((MyNum == 0) || (dostats)) {
    CLOCK(myrf);
    t_in_fac[MyNum] = lc->t_in_fac;
    t_in_solve[MyNum] = lc->t_in_solve;
    t_in_mod[MyNum] = lc->t_in_mod;
    t_in_bar[MyNum] = lc->t_in_bar;
    completion[MyNum] = calc_time(myrs, mydone);
  }
  if (MyNum == 0) {
    rs = myrs;
    done = mydone;
    rf = myrf;
  }
}


void lu0(a, n, stride)

shared double *a;
int n;
int stride;

{
  int j; 
  int k;
  int length;
  double alpha;

  for (k=0; k<n; k++) {
    /* modify subsequent columns */
    for (j=k+1; j<n; j++) {
      a[k+j*stride] /= a[k+k*stride];
      alpha = -a[k+j*stride];
      length = n-k-1;
      daxpy(&a[k+1+j*stride], &a[k+1+k*stride], n-k-1, alpha);
    }
  }
}


void bdiv(a, diag, stride_a, stride_diag, dimi, dimk)

shared double *a;
shared double *diag;
int stride_a;
int stride_diag;
int dimi;
int dimk;

{
  int j;
  int k;
  double alpha;

  for (k=0; k<dimk; k++) {
    for (j=k+1; j<dimk; j++) {
      alpha = -diag[k+j*stride_diag];
      daxpy(&a[j*stride_a], &a[k*stride_a], dimi, alpha);
    }
  }
}


void bmodd(a, c, dimi, dimj, stride_a, stride_c)

shared double *a;
shared double *c;
int dimi;
int dimj;
int stride_a;
int stride_c;

{
  int i;
  int j;
  int k;
  int length;
  double alpha;

  for (k=0; k<dimi; k++)
    for (j=0; j<dimj; j++) {
      c[k+j*stride_c] /= a[k+k*stride_a];
      alpha = -c[k+j*stride_c];
      length = dimi - k - 1;
      daxpy(&c[k+1+j*stride_c], &a[k+1+k*stride_a], dimi-k-1, alpha);
    }
}


void bmod(a, b, c, dimi, dimj, dimk, stride)

shared double *a;
shared double *b;
shared double *c;
int dimi;
int dimj;
int dimk;
int stride;

{
  int i;
  int j;
  int k;
  double alpha;

  for (k=0; k<dimk; k++) {
    for (j=0; j<dimj; j++) {
      alpha = -b[k+j*stride];
      daxpy(&c[j*stride], &a[k*stride], dimi, alpha);
    }
  }
}


void daxpy(a, b, n, alpha)

shared double *a;
shared double *b;
double alpha;
int n;

{
  int i;

  for (i=0; i<n; i++) {
    a[i] += alpha*b[i];
  }
}


int BlockOwner(I, J)

int I;
int J;

{
  return((I%num_cols) + (J%num_rows)*num_cols);
}


void lu(n, bs, MyNum, lc, dostats)

int n;
int bs;
int MyNum;
struct LocalCopies *lc;
int dostats;

{
  int i, il, j, jl, k, kl;
  int I, J, K;
  //double *A, *B, *C, *D;
  shared double *A, *B, *C, *D;
  int dimI, dimJ, dimK;
  int strI;
  //unsigned int t1, t2, t3, t4, t11, t22;
  struct timeval t1, t2, t3, t4, t11, t22;
  int diagowner;
  int colowner;

  strI = n;
  for (k=0, K=0; k<n; k+=bs, K++) {
    kl = k+bs; 
    if (kl>n) {
      kl = n;
    }

    if ((MyNum == 0) || (dostats)) {
      CLOCK(t1);
    }

    /* factor diagonal block */
    diagowner = BlockOwner(K, K);
    if (diagowner == MyNum) {
      A = &(a[k+k*n]); 
      lu0(A, kl-k, strI);
    }

    if ((MyNum == 0) || (dostats)) {
      CLOCK(t11);
    }

    //BARRIER(Global->start, P);
    upc_barrier;

    if ((MyNum == 0) || (dostats)) {
      CLOCK(t2);
    }

    /* divide column k by diagonal block */
    D = &(a[k+k*n]);
    for (i=kl, I=K+1; i<n; i+=bs, I++) {
      if (BlockOwner(I, K) == MyNum) {  /* parcel out blocks */
        il = i + bs;
        if (il > n) {
          il = n;
        }
        A = &(a[i+k*n]);
        bdiv(A, D, strI, n, il-i, kl-k);
      }
    }
    /* modify row k by diagonal block */
    for (j=kl, J=K+1; j<n; j+=bs, J++) {
      if (BlockOwner(K, J) == MyNum) {  /* parcel out blocks */
        jl = j+bs;
        if (jl > n) {
          jl = n;
        }
        A = &(a[k+j*n]);
        bmodd(D, A, kl-k, jl-j, n, strI);
      }
    }

    if ((MyNum == 0) || (dostats)) {
      CLOCK(t22);
    }

    //BARRIER(Global->start, P);
    upc_barrier;

    if ((MyNum == 0) || (dostats)) {
      CLOCK(t3);
    }

    /* modify subsequent block columns */
    for (i=kl, I=K+1; i<n; i+=bs, I++) {
      il = i+bs;
      if (il > n) {
        il = n;
      }
      colowner = BlockOwner(I,K);
      A = &(a[i+k*n]);
      for (j=kl, J=K+1; j<n; j+=bs, J++) {
        jl = j + bs;
        if (jl > n) {
          jl = n;
        }
        if (BlockOwner(I, J) == MyNum) {  /* parcel out blocks */
          B = &(a[k+j*n]);
          C = &(a[i+j*n]);
          bmod(A, B, C, il-i, jl-j, kl-k, n);
        }
      }
    }
    if ((MyNum == 0) || (dostats)) {
      CLOCK(t4);
      lc->t_in_fac += calc_time(t1, t11);
      lc->t_in_solve += calc_time(t2, t22);
      lc->t_in_mod += calc_time(t3, t4);
      lc->t_in_bar += calc_time(t11,t2) + calc_time(t22, t3);
    }
  }
}


//void InitA(double *rhs)
void InitA()

{
  int i, j;

  srand48((long) 1);
  for (j=0; j<n; j++) {
    for (i=0; i<n; i++) {
      a[i+j*n] = (double) lrand48()/MAXRAND;
      if (i == j) {
	a[i+j*n] *= 10;
      }
    }
  }

  upc_forall (j=0; j<n; j++; j) {
    rhs[j] = 0.0;
  }
  for (j=0; j<n; j++) {
    upc_forall (i=0; i<n; i++; i) {
      rhs[i] += a[i+j*n];
    }
  }
}


double TouchA(bs, MyNum)

int bs;
int MyNum;

{
  int i, j, I, J;
  double tot = 0.0;

  for (J=0; J*bs<n; J++) {
    for (I=0; I*bs<n; I++) {
      if (BlockOwner(I, J) == MyNum) {
        for (j=J*bs; j<(J+1)*bs && j<n; j++) {
          for (i=I*bs; i<(I+1)*bs && i<n; i++) {
            tot += a[i+j*n];
          }
        }
      }
    }
  }
  return(tot);
}


void PrintA()
{
  int i, j;

  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) {
      printf("%8.1f ", a[i+j*n]);
    }
    printf("\n");
  }
}


void CheckResult()

{
  int i, j, bogus = 0;
  double *y, diff, max_diff;

  y = (double *) malloc(n*sizeof(double));
  if (y == NULL) {
    printerr("Could not malloc memory for y\n");
    exit(-1);
  }
  for (j=0; j<n; j++) {
    y[j] = rhs[j];
  }
  for (j=0; j<n; j++) {
    y[j] = y[j]/a[j+j*n];
    for (i=j+1; i<n; i++) {
      y[i] -= a[i+j*n]*y[j];
    }
  }

  for (j=n-1; j>=0; j--) {
    for (i=0; i<j; i++) {
      y[i] -= a[i+j*n]*y[j];
    }
  }

  max_diff = 0.0;
  for (j=0; j<n; j++) {
    diff = y[j] - 1.0;
    if (fabs(diff) > 0.00001) {
      bogus = 1;
      max_diff = diff;
    }
  }
  if (bogus) {
    printf("TEST FAILED: (%.5f diff)\n", max_diff);
  } else {
    printf("TEST PASSED\n");
  }
  free(y);
}


void printerr(const char *s)

{
  fprintf(stderr,"ERROR: %s\n",s);
}
