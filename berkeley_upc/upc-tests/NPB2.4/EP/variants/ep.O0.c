/*--------------------------------------------------------------------
  
  NAS Parallel Benchmarks 2.4 UPC versions - EP

  This benchmark is an UPC version of the NPB EP code.
  
  The UPC versions are developed by HPCL-GWU and are derived from 
  the OpenMP version (developed by RWCP). 

  Permission to use, copy, distribute and modify this software for any
  purpose with or without fee is hereby granted.
  This software is provided "as is" without express or implied warranty.
  
  Information on the UPC project at GWU is available at:

           http://upc.gwu.edu/

  Information on NAS Parallel Benchmarks 2.4 is available at:
  
           http://www.nas.nasa.gov/Software/NPB/

---------------------------------------------------------------------*/
/*--------------------------------------------------------------------
  UPC version: F. Cantonnet  - GWU - HPCL (fcantonn@gwu.edu)
               T. El-Ghazawi - GWU - HPCL (tarek@gwu.edu)
               S. Chauvin

  Author(NAS): P. O. Frederickson 
               D. H. Bailey
               A. C. Woo
	       R. F. Van der Wijngaart
 --------------------------------------------------------------------*/
/*--------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--------------------------------------------------------------------*/

#include <stdio.h>
#include <assert.h>
#include <upc_relaxed.h>
#include "npb-C.h"
#include "npbparams.h"

/* parameters */
#define	MK		16
#define	MM		(M - MK)
#define	NN		(1 << MM)
#define	NK		(1 << MK)
#define	NQ		10
#define EPSILON		1.0e-8
#define	A		1220703125.0
#define	S		271828183.0
#define	TIMERS_ENABLED	TRUE

#define TIMER_TOTALTIME 1
#define TIMER_GAUSSIAN  2
#define TIMER_RANDOM    3
#define TIMER_ALLREDUCE 0
#define MAX_TIMERS      4

/* global variables */
/* common /storage/ */
static double x[2*NK];

shared double p_sx;
shared double p_sy;

static shared double q[NQ];  // NQ is small so no major loss in memory

shared double timer[MAX_TIMERS][THREADS];

upc_lock_t *critical_lock;

void timers_reduce( double *max_t ) // TO BE RUN only by THREAD 0
{
  int i, j;

  for( j=0; j<MAX_TIMERS; j++ )
    {
      max_t[j] = timer[j][0];
      for( i=1; i<THREADS; i++ )
	{
	  if( timer[j][i] > max_t[j] )
	    max_t[j] = timer[j][i];
	}
    }
}

/*--------------------------------------------------------------------
      program EMBAR
c-------------------------------------------------------------------*/
/*
c   This is the serial version of the APP Benchmark 1,
c   the "embarassingly parallel" benchmark.
c
c   M is the Log_2 of the number of complex pairs of uniform (0, 1) random
c   numbers.  MK is the Log_2 of the size of each batch of uniform random
c   numbers.  MK can be set for convenience on a given system, since it does
c   not affect the results.
*/
int main(int argc, char **argv)
{
  double t3, t4, x1, x2;
  int kk, ik, l;
  double qq[NQ];		/* private copy of q[0:NQ-1] */
  double Mops, t1, t2, sx, sy, tm, an, tt, gc;
  double dum[3] = { 1.0, 1.0, 1.0 };
  int np, i, k, nit, k_offset, j;
  boolean verified;
  char size[13+1];	/* character*13 */
  double max_timer[MAX_TIMERS];

  verified = 0;

  /*
    c   Because the size of the problem is too large to store in a 32-bit
    c   integer for some classes, we put it into a string (for printing).
    c   Have to strip off the decimal point put in there by the floating
    c   point print statement (internal file)
  */

  critical_lock = upc_all_lock_alloc();
  assert( critical_lock != NULL );

  if(MYTHREAD == 0)
    {
      printf("\n\n NAS Parallel Benchmarks 2.4 UPC version"
	     " - EP Benchmark - GWU/HPCL\n");

      sprintf(size, "%13.0f", pow(2.0, M+1));
      for (j = 14; j >= 1; j--)
	{
	  if (size[j] == '.') 
	    size[j] = ' ';
	}
     printf(" Number of random numbers generated: %15s\n", size);

     verified = FALSE;
    }

  /*
    c   Compute the number of "batches" of random number pairs generated 
    c   per processor. Adjust if the number of processors does not evenly 
    c   divide the total number
    */
  np = NN;
  
  /*
    c   Call the random number generator functions and initialize
    c   the x-array to reduce the effects of paging on the timings.
    c   Also, call all mathematical functions that are used. Make
    c   sure these initializations cannot be eliminated as dead code.
    */
  vranlc(0, &(dum[0]), dum[1], &(dum[2]));
  dum[0] = randlc(&(dum[1]), dum[2]);
  for (i = 0; i < 2*NK; i++)
    x[i] = -1.0e99;
  Mops = log(sqrt(fabs(max(1.0, 1.0))));

  timer_clear(TIMER_TOTALTIME);
  timer_clear(TIMER_GAUSSIAN);
  timer_clear(TIMER_RANDOM);
  timer_clear(TIMER_ALLREDUCE);
  upc_barrier;

  timer_start(TIMER_TOTALTIME);
  
  vranlc(0, &t1, A, x);
  
  /*   Compute AN = A ^ (2 * NK) (mod 2^46). */
  t1 = A;
  
  for ( i = 1; i <= MK+1; i++)
    {
      t2 = randlc(&t1, t1);
    }
  
  an = t1;
  tt = S;
  gc = 0.0;
  
  /*
    c   Each instance of this loop may be performed independently. We compute
    c   the k offsets separately to take into account the fact that some nodes
    c   have more numbers to generate than others
    */
  for (i = 0; i < NQ; i++) 
    qq[i] = 0.0;
  sx = 0.0;
  sy = 0.0;

  k_offset=-1;

  upc_forall (k = 1; k <= np; k++; k%THREADS) /* %THREADS is present for
						 the SGI GCC-UPC Compiler */
    {
      kk = k_offset + k;
      t1 = S;
      t2 = an;
      
      /*      Find starting seed t1 for this kk. */
      for (i = 1; i <= 100; i++)
	{
	  ik = kk / 2;
	  if (2 * ik != kk)
	    t3 = randlc(&t1, t2);
	  if (ik == 0)
	    break;
	  t3 = randlc(&t2, t2);
	  kk = ik;
	}

      /*      Compute uniform pseudorandom numbers. */
#if (TIMERS_ENABLED == TRUE) 
      timer_start(TIMER_RANDOM);
#endif
      vranlc(2*NK, &t1, A, x-1);
#if (TIMERS_ENABLED == TRUE) 
      timer_stop(TIMER_RANDOM);
#endif
      /*
	c       Compute Gaussian deviates by acceptance-rejection method and 
	c       tally counts in concentric square annuli.  This loop is not 
	c       vectorizable.
	*/
#if (TIMERS_ENABLED == TRUE)
      timer_start(TIMER_GAUSSIAN);
#endif
      for ( i = 0; i < NK; i++)
	{
	  x1 = 2.0 * x[2*i] - 1.0;
	  x2 = 2.0 * x[2*i+1] - 1.0;
	  t1 = pow2(x1) + pow2(x2);
	  if (t1 <= 1.0)
	    {
	      t2 = sqrt(-2.0 * log(t1) / t1);
	      t3 = (x1 * t2);				/* Xi */
	      t4 = (x2 * t2);				/* Yi */
	      l = max(fabs(t3), fabs(t4));
	      qq[l] += 1.0;				/* counts */
	      sx = sx + t3;				/* sum of Xi */
	      sy = sy + t4;				/* sum of Yi */
	    }
	}
#if (TIMERS_ENABLED == TRUE)
      timer_stop(TIMER_GAUSSIAN);
#endif
    }
  
#if( TIMERS_ENABLED == TRUE )
  timer_start(TIMER_ALLREDUCE);
#endif
  upc_lock(critical_lock);

  p_sx+=sx;
  p_sy+=sy;
  for (i = 0; i <= NQ - 1; i++) 
    q[i] += qq[i];

  upc_unlock(critical_lock);
#if( TIMERS_ENABLED == TRUE )
  timer_stop(TIMER_ALLREDUCE);
#endif

  /* end of parallel region */
  timer_stop(TIMER_TOTALTIME);

  /* prepare for timers_reduce() */
  for( i=0; i<MAX_TIMERS; i++ )
    timer[i][MYTHREAD] = timer_read(i);

  upc_barrier;
  
  if (MYTHREAD == 0)
    {
      for (i = 0; i <= NQ-1; i++)
	{
	  gc = gc + q[i];
	}

      /* reduce the timers */
      timers_reduce( max_timer );
      
      tm = max_timer[TIMER_TOTALTIME];
      
      nit = 0;
      if (M == 24)
	{
	  if((fabs((p_sx- (-3.247834652034740e3))/p_sx) <= EPSILON) &&
	     (fabs((p_sy- (-6.958407078382297e3))/p_sy) <= EPSILON))
	    {
	      verified = TRUE;
	    }
	} 
      else if (M == 25) 
	{
	  if ((fabs((p_sx- (-2.863319731645753e3))/p_sx) <= EPSILON) &&
	      (fabs((p_sy- (-6.320053679109499e3))/p_sy) <= EPSILON)) 
	    {
	      verified = TRUE;
	    }
	}
      else if (M == 28)
	{
	  if ((fabs((p_sx- (-4.295875165629892e3))/p_sx) <= EPSILON) &&
	      (fabs((p_sy- (-1.580732573678431e4))/p_sy) <= EPSILON))
	    {
	      verified = TRUE;
	    }
	}
      else if (M == 30)
	{
	  if ((fabs((p_sx- (4.033815542441498e4))/p_sx) <= EPSILON) &&
	      (fabs((p_sy- (-2.660669192809235e4))/p_sy) <= EPSILON))
	    {
	      verified = TRUE;
	    }
	}
      else if (M == 32)
	{
	  if ((fabs((p_sx- (4.764367927995374e4))/p_sx) <= EPSILON) &&
	      (fabs((p_sy- (-8.084072988043731e4))/p_sy) <= EPSILON))
	    {
	      verified = TRUE;
	    }
	}
      else if (M == 36)
	{
	  if ((fabs((p_sx- (1.982481200946593e5))/p_sx) <= EPSILON) &&
	      (fabs((p_sy- (-1.020596636361769e5))/p_sy) <= EPSILON))
	    {
	      verified = TRUE;
	    }
	}
      
      Mops = pow(2.0, M+1)/tm/1000000.0;
      
      printf("EP Benchmark Results: \n"
	     "CPU Time = %10.4f\n"
	     "N = 2^%5d\n"
	     "No. Gaussian Pairs = %15.0f\n"
	     "Sums = %25.15e %25.15e\n"
	     "Counts:\n",
	     tm, M, gc, p_sx, p_sy);
      
      for (i = 0; i  <= NQ-1; i++)
	{
	  printf("%3d %15.0f\n", i, q[i]);
	}
      
      c_print_results("EP", CLASS, M+1, 0, 0, np, THREADS,
		      tm, Mops,
		      "Random numbers generated",
		      verified, NPBVERSION, COMPILETIME,
		      NPB_CS1, NPB_CS2, NPB_CS3, NPB_CS4, NPB_CS5, NPB_CS6, NPB_CS7);

#if (TIMERS_ENABLED == TRUE)
      printf("Total time:     %.6lf sec\n", max_timer[TIMER_TOTALTIME]);
      printf("Gaussian pairs: %.6lf sec\n", max_timer[TIMER_GAUSSIAN]);
      printf("Random numbers: %.6lf sec\n", max_timer[TIMER_RANDOM]);
      printf("All_Reduce cost:%.6lf sec\n", max_timer[TIMER_ALLREDUCE]);
#endif
    }

  return 0;
}
