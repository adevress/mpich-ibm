/*--------------------------------------------------------------------

  NAS Parallel Benchmarks 2.4 UPC versions - MG

  This benchmark is an UPC version of the NPB MG code.

  The UPC versions are developed by HPCL-GWU and are derived from
  the OpenMP version (developed by RWCP) and from the MPI version
  (developed by NAS).

  Permission to use, copy, distribute and modify this software for any
  purpose with or without fee is hereby granted.
  This software is provided "as is" without express or implied warranty.

  Information on the UPC project at GWU is available at:

           http://upc.gwu.edu

  Information on NAS Parallel Benchmarks 2.4 is available at:

           http://www.nas.nasa.gov/Software/NPB/

--------------------------------------------------------------------*/
/*--- PLAIN UPC VERSION - NOV 2004                              ----*/
/*--------------------------------------------------------------------
  UPC version: F. Cantonnet  - GWU - HPCL (fcantonn@gwu.edu)
               V. Baydogan   - GWU - HPCL (vbdogan@gwu.edu)
	       T. El-Ghazawi - GWU - HPCL (tarek@gwu.edu)
	       S. Chauvin

  Authors(NAS): R. F. Van der Wijngaart
                E. Barszcz
                P. Frederickson
		A. Woo
		M. Yarrow
                
--------------------------------------------------------------------*/
/*--------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. This program
 can be freely redistributed provided that you conspicuously and
 appropriately publish on each copy an appropriate referral notice to
 the authors and disclaimer of warranty; keep intact all the notices
 that refer to this License and to the absence of any warranty; and
 give any other recipients of the Program a copy of this License along
 with the Program.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 --------------------------------------------------------------------*/

#include <stdio.h>
#include <upc.h>
#include <upc_relaxed.h>

#include "npb-C.h"
#include "globals.h"

/* parameters */
#define MAX_TIMERS      2
#define TIMER_BENCH	0
#define	TIMER_INIT	1

/* global variables */
shared double timer[MAX_TIMERS][THREADS];

shared double sh_a[4], sh_c[4];
shared int sh_nit, sh_nx, sh_ny, sh_nz, sh_debug_vec[8], sh_lt, sh_lb;
shared double s, max;
int is1, is2, is3, ie1, ie2, ie3;

upc_lock_t *critical_lock;

shared int jg[4][MM][2];
shared double red_best, red_winner;

/* functions prototypes */
void setup(int *n1, int *n2, int *n3, int lt);

double power( double a, int n );
void bubble( double ten[M][2], int j1[M][2], int j2[M][2],
		    int j3[M][2], int m, int ind );

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

int nx[MAXLEVEL+1], ny[MAXLEVEL+1], nz[MAXLEVEL+1];

char Class;

int debug_vec[8];

int lt, lb;

int ir[MAXLEVEL];

int m1[MAXLEVEL+1], m2[MAXLEVEL+1], m3[MAXLEVEL+1];
int nbr[4][2][MAXLEVEL+1];
int dead[MAXLEVEL+1], give_ex[4][MAXLEVEL+1], take_ex[4][MAXLEVEL+1];

shared sh_arr_t sh_u[THREADS*(LT_DEFAULT_I)], sh_r[THREADS*(LT_DEFAULT_I)];
shared sh_arr_t sh_v[THREADS];

void zero3( shared sh_arr_t *z, int n1, int n2, int n3);
void zran3( shared sh_arr_t *z, int n1, int n2, int n3, 
	    int nx, int ny, int k);
void comm3( shared sh_arr_t *u, int n1, int n2, int n3, int kk);
void norm2u3(shared sh_arr_t *r, int n1, int n2, int n3,
	     double *rnm2, double *rnmu, int nx, int ny, int nz);
void resid( shared sh_arr_t *u, shared sh_arr_t *v, shared sh_arr_t *r,
	    int n1, int n2, int n3, double a[4], int k );
void rep_nrm( shared sh_arr_t *u, int n1, int n2, int n3,
		    char *title, int kk);
void showall( shared sh_arr_t *z, int n1, int n2, int n3);
void mg3P(shared sh_arr_t *u, shared sh_arr_t *v, shared sh_arr_t *r, 
	  double a[4], double c[4], int n1, int n2, int n3, int k);
void rprj3( shared sh_arr_t *r, int m1k, int m2k, int m3k,
	    shared sh_arr_t *s, int m1j, int m2j, int m3j, int k );
void psinv( shared sh_arr_t *r, shared sh_arr_t *u, 
	    int n1, int n2, int n3, double c[4], int k);
void interp( shared sh_arr_t *z, int mm1, int mm2, int mm3,
	     shared sh_arr_t *u, int n1, int n2, int n3, int k );

#include "file_output.h"
#define FILE_DEBUG FALSE

/*--------------------------------------------------------------------
  program mg
  c-------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  /*-------------------------------------------------------------------------
    c k is the current level. It is passed down through subroutine args
    c and is NOT global. it is the current iteration
    c------------------------------------------------------------------------*/
  int it;
  double t, tinit, mflops;
  double a[4], c[4];
  double rnm2;         
  double rnmu;

  /*-------------------------------------------------------------------------
    c These arrays are in common because they are quite large
    c and probably shouldn't be allocated on the stack. They
    c are always passed as subroutine args. 
    c------------------------------------------------------------------------*/

  double epsilon = 1.0e-8;
  int n1, n2, n3, nit;
  double verify_value;
  boolean verified;
  int i, l;
  int nn;
  FILE *fp;
  double max_timer[MAX_TIMERS];

  // UPC Specific initializations
  critical_lock = upc_all_lock_alloc();
  MEM_OK( critical_lock );

  // Parameters
  timer_clear(TIMER_BENCH);
  timer_clear(TIMER_INIT);
  timer_start(TIMER_INIT);

  if( MYTHREAD == 0 )
    {
      /*----------------------------------------------------------------------
	c Read in and broadcast input data
	c---------------------------------------------------------------------*/
      printf("\n\n NAS Parallel Benchmarks 2.4 UPC version"
	     " - MG Benchmark - GWU/HPCL\n\n");

      fp = fopen("mg.input", "r");
      if (fp != NULL) 
	{
	  printf(" Reading from input file mg.input\n");
	  fscanf(fp, "%d", &lt);
	  while(fgetc(fp) != '\n');
	  fscanf(fp, "%d%d%d", &nx[lt], &ny[lt], &nz[lt]);
	  while(fgetc(fp) != '\n');
	  fscanf(fp, "%d", &nit);
	  while(fgetc(fp) != '\n');
	  for (i = 0; i <= 7; i++) 
	    {
	      fscanf(fp, "%d", &debug_vec[i]);
	    }
	  fclose(fp);
	}
      else 
	{
	  printf(" No input file. Using compiled defaults\n");
    
	  lt = LT_DEFAULT;
	  nit = NIT_DEFAULT;
	  nx[lt] = NX_DEFAULT;
	  ny[lt] = NY_DEFAULT;
	  nz[lt] = NZ_DEFAULT;

	  for (i = 0; i <= 7; i++) 
	    {
	      debug_vec[i] = DEBUG_DEFAULT;
	    }
	}

      sh_nx=nx[lt];    sh_ny=ny[lt];    sh_nz=nz[lt];
      sh_lt=lt; sh_nit=nit;

      for (i = 0; i <= 7; i++) 
	sh_debug_vec[i]=debug_vec[i];

      if ( (nx[lt] != ny[lt]) || (nx[lt] != nz[lt]) ) 
	{
	  Class = 'U';
	}
      else if( nx[lt] == 32 && nit == 4 ) 
	{
	  Class = 'S';
	}
      else if( nx[lt] == 64 && nit == 40 ) 
	{
	  Class = 'W';
	}
      else if( nx[lt] == 256 && nit == 4 ) 
	{
	  Class = 'A';
	}
      else if( nx[lt] == 256 && nit == 20 ) 
	{
	  Class = 'B';
	}
      else if( nx[lt] == 512 && nit == 20 ) 
	{
	  Class = 'C';
	}
      else if( nx[lt] == 1024 && nit == 40 )
        {
          Class = 'D';
        }
      else 
	{
	  Class = 'U';
	}

      /*--------------------------------------------------------------------
	c  Use these for debug info:
	c---------------------------------------------------------------------
	c     debug_vec(0) = 1 !=> report all norms
	c     debug_vec(1) = 1 !=> some setup information
	c     debug_vec(1) = 2 !=> more setup information
	c     debug_vec(2) = k => at level k or below, show result of resid
	c     debug_vec(3) = k => at level k or below, show result of psinv
	c     debug_vec(4) = k => at level k or below, show result of rprj
	c     debug_vec(5) = k => at level k or below, show result of interp
	c     debug_vec(6) = 1 => (unused)
	c     debug_vec(7) = 1 => (unused)
	c-------------------------------------------------------------------*/

      sh_a[0] = -8.0/3.0;
      sh_a[1] =  0.0;
      sh_a[2] =  1.0/6.0;
      sh_a[3] =  1.0/12.0;

      if (Class == 'A' || Class == 'S' || Class =='W') 
	{
	  /*--------------------------------------------------------------------
	    c     Coefficients for the S(a) smoother
	    c-------------------------------------------------------------------*/
	  sh_c[0] =  -3.0/8.0;
	  sh_c[1] =  1.0/32.0;
	  sh_c[2] =  -1.0/64.0;
	  sh_c[3] =   0.0;
	}
      else 
	{
	  /*--------------------------------------------------------------------
	    c     Coefficients for the S(b) smoother
	    c-------------------------------------------------------------------*/
	  sh_c[0] =  -3.0/17.0;
	  sh_c[1] =  1.0/33.0;
	  sh_c[2] =  -1.0/61.0;
	  sh_c[3] =   0.0;
	}
    
      sh_lb = 1;
    }
  upc_barrier 1;
    
  lt=sh_lt;
  nit=sh_nit; 
  lb=sh_lb;
  nx[lt]=sh_nx;ny[lt]=sh_ny;nz[lt]=sh_nz; 

  for (i = 0; i <= 7; i++) 
    debug_vec[i]=sh_debug_vec[i];

  a[0]=sh_a[0]; a[1]=sh_a[1]; a[2]=sh_a[2]; a[3]=sh_a[3];
  c[0]=sh_c[0]; c[1]=sh_c[1]; c[2]=sh_c[2]; c[3]=sh_c[3];
 
  setup(&n1,&n2,&n3,lt);
      
  for( l=lt; l>=1; l-- )
    {
      sh_u[MYTHREAD+(l*THREADS)].arr = (shared [] double *) 
	upc_local_alloc( (m3[l]*m2[l]*m1[l]), sizeof( double ));
      MEM_OK( sh_u[MYTHREAD+(l*THREADS)].arr );
      sh_r[MYTHREAD+(l*THREADS)].arr = (shared [] double *) 
	upc_local_alloc( (m3[l]*m2[l]*m1[l]), sizeof( double ));
      MEM_OK( sh_r[MYTHREAD+(l*THREADS)].arr );
    }

  sh_v[MYTHREAD].arr = (shared [] double *) 
    upc_local_alloc( (m3[lt]*m2[lt]*m1[lt]), sizeof( double ));
  MEM_OK( sh_v[MYTHREAD].arr );

  zero3( &sh_u[lt*THREADS], n1, n2, n3);
  zran3( sh_v, n1, n2, n3, nx[lt], ny[lt], lt);

  norm2u3( sh_v, n1, n2, n3, 
	   &rnm2, &rnmu, nx[lt], ny[lt], nz[lt]);

  if (MYTHREAD == 0) 
    {
      printf(" Size: %3dx%3dx%3d (class %1c)\n",
	     nx[lt], ny[lt], nz[lt], Class);
      printf(" Iterations: %3d\n", nit);
    }

  resid( &sh_u[lt*THREADS], sh_v, &sh_r[lt*THREADS], n1, n2, n3, a, lt);
  norm2u3( &sh_r[lt*THREADS], n1, n2, n3,
	   &rnm2, &rnmu, nx[lt], ny[lt], nz[lt]);
  
  /*c---------------------------------------------------------------------
    c     One iteration for startup
    c---------------------------------------------------------------------*/
  mg3P( sh_u, sh_v, sh_r, a, c, n1, n2, n3, lt);
	
  resid( &sh_u[lt*THREADS], sh_v, &sh_r[lt*THREADS], n1, n2, n3, a, lt);
  
  setup(&n1,&n2,&n3,lt);
  
  zero3( &sh_u[lt*THREADS], n1, n2, n3 );

  zran3( sh_v, n1, n2, n3, nx[lt], ny[lt], lt );

  upc_barrier 17;
    
  timer_stop(TIMER_INIT);
  timer_start(TIMER_BENCH);
    
  resid( &sh_u[lt*THREADS], sh_v, &sh_r[lt*THREADS], n1, n2, n3, a, lt);
  norm2u3( &sh_r[lt*THREADS], n1, n2, n3,
	   &rnm2, &rnmu, nx[lt], ny[lt], nz[lt] );
  
  for ( it = 1; it <= nit; it++) 
    {
      if( MYTHREAD == 0 )
	printf(" %4d\n", it );
      mg3P( sh_u, sh_v, sh_r, a, c, n1, n2, n3, lt );
      resid( &sh_u[lt*THREADS], sh_v, 
	     &sh_r[lt*THREADS], n1, n2, n3, a, lt );
    }
  
  norm2u3( &sh_r[lt*THREADS], n1, n2, n3,
	   &rnm2, &rnmu, nx[lt], ny[lt], nz[lt] );

  timer_stop(TIMER_BENCH);

  /* prepare for timers_reduce() */
  for( i=0; i<MAX_TIMERS; i++ )
    timer[i][MYTHREAD] = timer_read(i);

  upc_barrier 18;

#if( FILE_DEBUG == TRUE )
  file_output();
#endif
    
  if (MYTHREAD == 0) 
    {
      timers_reduce( max_timer );

      t = max_timer[TIMER_BENCH];
      tinit = max_timer[TIMER_INIT];

      verified = FALSE;
      verify_value = 0.0;

      printf(" Initialization time: %15.3f seconds\n", tinit);
      printf(" Benchmark completed\n");

      if (Class != 'U') 
	{
	  if (Class == 'S') 
	    {
	      verify_value = 0.530770700573e-04;
	    } 
	  else if (Class == 'W') 
	    {
	      verify_value = 0.250391406439e-17;  /* 40 iterations*/
	      /*				0.183103168997d-044 iterations*/
	    } 
	  else if (Class == 'A') 
	    {
	      verify_value = 0.2433365309e-5;
	    } 
	  else if (Class == 'B') 
	    {
	      verify_value = 0.180056440132e-5;
	    } 
	  else if (Class == 'C') 
	    {
	      verify_value = 0.570674826298e-06;
	    }
          else if (Class == 'D')
            {
              verify_value = 0.158327506042e-9;
            }

	  if ( fabs( rnm2 - verify_value ) <= epsilon ) 
	    {
	      verified = TRUE;
	      printf(" VERIFICATION SUCCESSFUL\n");
	      printf(" L2 Norm is %20.12e\n", rnm2);
	      printf(" Error is   %20.12e\n", rnm2 - verify_value);
	    } 
	  else 
	    {
	      verified = FALSE;
	      printf(" VERIFICATION FAILED\n");
	      printf(" L2 Norm is             %20.12e\n", rnm2);
	      printf(" The correct L2 Norm is %20.12e\n", verify_value);
	    }
	} 
      else 
	{
	  verified = FALSE;
	  printf(" Problem size unknown\n");
	  printf(" NO VERIFICATION PERFORMED\n");
	}

      if ( t != 0.0 ) 
	{
	  nn = nx[lt]*ny[lt]*nz[lt];
	  mflops = 58.*nit*nn*1.0e-6 / t;
	}
      else
	{
	  mflops = 0.0;
	}

      c_print_results("MG", Class, nx[lt], ny[lt], nz[lt], 
		      nit, THREADS, t, mflops, "          floating point", 
		      verified, NPBVERSION, COMPILETIME,
		      NPB_CS1, NPB_CS2, NPB_CS3, NPB_CS4, NPB_CS5, NPB_CS6, NPB_CS7);
    }
  return 0;
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void setup(int *n1, int *n2, int *n3, int lt) 
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  int k;
  int dx, dy, log_p, i, j;
  int ax, next[4],mi[4][MAXLEVEL+1],mip[4][MAXLEVEL+1];
  int ng[4][MAXLEVEL+1];
  int idi[4], pi[4], idin[4][2];
  int dir;
    
  ng[1][lt] = nx[lt];
  ng[2][lt] = ny[lt];
  ng[3][lt] = nz[lt];
    
  for(ax=1; ax<4; ax++) 
    {
      next[ax] = 1;

      for(k=lt-1; k>0; k--)
	ng[ax][k] = ng[ax][k+1]/2;
    }
    
  for(k=lt-1; k>0; k--) 
    {
      nx[k] = ng[1][k];
      ny[k] = ng[2][k];
      nz[k] = ng[3][k];
    }
      
  log_p  = log(((float)THREADS)+0.0001)/log(2.0);

  dx     = log_p/3;
  pi[1]  = power(2,dx);
  idi[1] = MYTHREAD % pi[1];
      
  dy     = (log_p-dx)/2;
  pi[2]  = power(2,dy);
  idi[2] = (MYTHREAD/pi[1]) % pi[2];
      
  pi[3]  = THREADS/(pi[1]*pi[2]);
  idi[3] = MYTHREAD / (pi[1]*pi[2]);
      
  for(k=lt; k>0; k--) 
    {
      dead[k] = 0;
	  
      for(ax=1; ax<4; ax++) 
	{
	  take_ex[ax][k] = 0;
	  give_ex[ax][k] = 0;
	      
	  mi[ax][k] = 2 + 
	    ((idi[ax]+1)*ng[ax][k])/pi[ax] -
	    ((idi[ax]+0)*ng[ax][k])/pi[ax];
	    
	  mip[ax][k] = 2 + 
	    ((next[ax]+idi[ax]+1)*ng[ax][k])/pi[ax] -
	    ((next[ax]+idi[ax]+0)*ng[ax][k])/pi[ax];
	    
	  if (mip[ax][k]==2 || mi[ax][k]==2)
	    next[ax] = 2*next[ax];

	  if( k+1 < lt ) 
	    {
	      if ((mip[ax][k]==2) && (mi[ax][k]==3)) 
		{
		  give_ex[ax][k+1] = 1;
		}
		
	      if ((mip[ax][k]==3)&&(mi[ax][k]==2)) 
		{
		  take_ex[ax][k+1] = 1;
		}
	    }
	}
	  
      if( mi[1][k]==2 || mi[2][k]==2 || mi[3][k]==2 )
	dead[k] = 1;
	  
      m1[k] = mi[1][k];
      m2[k] = mi[2][k];
      m3[k] = mi[3][k];

      for (ax=1; ax<4; ax++) 
	{
	  idin[ax][DIRm1] = (idi[ax]+next[ax]+pi[ax]) % pi[ax];
	  idin[ax][DIRp1] = (idi[ax]-next[ax]+pi[ax]) % pi[ax];
	}
	
      for (dir=0; dir<2; dir++) 
	{
	  nbr[1][dir][k] = idin[1][dir] + pi[1] * (idi[2] + pi[2] * idi[3]);
	  nbr[2][dir][k] = idi[1] + pi[1] * (idin[2][dir] + pi[2] * idi[3]);
	  nbr[3][dir][k] = idi[1] + pi[1] * (idi[2] + pi[2] * idin[3][dir]);
	} 
    }

  k=lt;
    
  is1 = 1 + ng[1][k] - ((pi[1]  -idi[1])*ng[1][lt])/pi[1];
  ie1 = 0 + ng[1][k] - ((pi[1]-1-idi[1])*ng[1][lt])/pi[1];
  *n1 = 3 + ie1 - is1;

  is2 = 1 + ng[2][k] - ((pi[2]  -idi[2])*ng[2][lt])/pi[2];
  ie2 = 0 + ng[2][k] - ((pi[2]-1-idi[2])*ng[2][lt])/pi[2];
  *n2 = 3 + ie2 - is2;

  is3 = 1 + ng[3][k] - ((pi[3]  -idi[3])*ng[3][lt])/pi[3];
  ie3 = 0 + ng[3][k] - ((pi[3]-1-idi[3])*ng[3][lt])/pi[3];
  *n3 = 3 + ie3 - is3;

  ir[lt]=1;

  for(j = lt-1 ; j>0; j--)
    {
      ir[j]=ir[j+1] + m1[j+1]*m2[j+1]*m3[j+1];
    }

  if (debug_vec[1] >=  1 ) 
    {
      if (MYTHREAD == 0) 
	{
	  printf(" in setup, \n");
	  printf("  P#  lt  nx  ny  nz  n1  n2  n3 is1 is2 is3 ie1 ie2 ie3\n");
	}
      upc_barrier 67;
	
      printf("%4d%4d%4d%4d%4d%4d%4d%4d%4d%4d%4d%4d%4d%4d\n",
	     MYTHREAD,lt,nx[lt],ny[lt],nz[lt],*n1,*n2,*n3,is1,is2,is3,ie1,ie2,ie3);
    }

  if (debug_vec[1] >=  2 ) 
    {	
      for(i=0; i<THREADS; i++) 
	{	    
	  if( MYTHREAD == i ) 
	    {
	      printf("\nprocesssor = %d\n", MYTHREAD);
	      for (k=lt; k>0; k--) 
		{
		  printf("%didi=%4d%4d%4d", k,idi[1], idi[2], idi[3]);
		  printf("nbr=%4d%4d   %4d%4d   %4d%4d ", 
			 nbr[1][DIRm1][k], nbr[1][DIRp1][k],
			 nbr[2][DIRm1][k], nbr[2][DIRp1][k],
			 nbr[3][DIRm1][k], nbr[3][DIRp1][k]);
		  printf("mi=%4d%4d%4d\n", mi[1][k],mi[2][k],mi[3][k]);
		}
	      printf("idi(s) = %10d%10d%10d\n", idi[1],idi[2],idi[3]);
	      printf("dead(2), dead(1) = %2d%2d\n", dead[2], dead[1]);
	      for(ax=1; ax<4; ax++) 
		{
		  printf("give_ex[%d][2]= %d", ax, give_ex[ax][2]);
		  printf("  take_ex[%d][2]= %d\n", ax, take_ex[ax][2]);
		}
	    }
	  upc_barrier 68;
	}
	
	
    }
  k = lt;
}
      

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void mg3P(shared sh_arr_t *u, shared sh_arr_t *v, shared sh_arr_t *r, 
	  double a[4], double c[4], int n1, int n2, int n3, int k)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     multigrid V-cycle routine
    c-------------------------------------------------------------------*/

  int j;

  /*--------------------------------------------------------------------
    c     down cycle.
    c     restrict the residual from the find grid to the coarse
    c-------------------------------------------------------------------*/

  for (k = lt; k >= lb+1; k--) 
    {
      j = k-1;
      rprj3( &r[k*THREADS], m1[k], m2[k], m3[k],
	     &r[j*THREADS], m1[j], m2[j], m3[j], k);
    }

  k = lb;
  /*--------------------------------------------------------------------
    c     compute an approximate solution on the coarsest grid
    c-------------------------------------------------------------------*/
  zero3( &u[k*THREADS], m1[k], m2[k], m3[k]);
  psinv( &r[k*THREADS], &u[k*THREADS], m1[k], m2[k], m3[k], c, k);

  for (k = lb+1; k <= lt-1; k++) 
    {
      j = k-1;
      /*--------------------------------------------------------------------
	c        prolongate from level k-1  to k
	c-------------------------------------------------------------------*/
      zero3( &u[k*THREADS], m1[k], m2[k], m3[k]);
      interp( &u[j*THREADS], m1[j], m2[j], m3[j],
	      &u[k*THREADS], m1[k], m2[k], m3[k], k);
      /*--------------------------------------------------------------------
	c        compute residual for level k
	c-------------------------------------------------------------------*/
      resid( &u[k*THREADS], &r[k*THREADS], &r[k*THREADS], 
	     m1[k], m2[k], m3[k], a, k);
      /*--------------------------------------------------------------------
	c        apply smoother
	c-------------------------------------------------------------------*/
      psinv( &r[k*THREADS], &u[k*THREADS], m1[k], m2[k], m3[k], c, k);
    }

  j = lt - 1;
  k = lt;
  interp( &u[j*THREADS], m1[j], m2[j], m3[j], 
	  &u[lt*THREADS], n1, n2, n3, k);
  resid( &u[lt*THREADS], v, &r[lt*THREADS], n1, n2, n3, a, k);
  psinv( &r[lt*THREADS], &u[lt*THREADS], n1, n2, n3, c, k);
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void psinv( shared sh_arr_t *r, shared sh_arr_t *u, int n1, int n2, 
	    int n3, double c[4], int k)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     psinv applies an approximate inverse as smoother:  u = u + Cr
    c
    c     This  implementation costs  15A + 4M per result, where
    c     A and M denote the costs of Addition and Multiplication.  
    c     Presuming coefficient c(3) is zero (the NPB assumes this,
    c     but it is thus not a general case), 2A + 1M may be eliminated,
    c     resulting in 13A + 3M.
    c     Note that this vectorizes, and is also fine for cache 
    c     based machines.  
    c-------------------------------------------------------------------*/
#define u(iz,iy,ix) u[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
#define r(iz,iy,ix) r[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  int i3, i2, i1;
  double r1[M], r2[M];

  for (i3 = 1; i3 < n3-1; i3++) 
    {
      for (i2 = 1; i2 < n2-1; i2++) 
	{
	  for (i1 = 0; i1 < n1; i1++) 
	    {
	      r1[i1] = r(i3, (i2-1), i1) + r(i3, (i2+1), i1)
		+ r((i3-1), i2, i1) + r((i3+1), i2, i1);
	      r2[i1] = r((i3-1), (i2-1), i1) + r((i3-1), (i2+1), i1)
		+ r((i3+1), (i2-1), i1) + r((i3+1), (i2+1), i1);
	    }
	  for (i1 = 1; i1 < n1-1; i1++) 
	    {
	      u(i3, i2, i1) = u(i3, i2, i1)
		+ c[0] * r(i3, i2, i1)
		+ c[1] * ( r(i3, i2, (i1-1)) + r(i3, i2, (i1+1))
			   + r1[i1] )
		+ c[2] * ( r2[i1] + r1[i1-1] + r1[i1+1] );
	      /*--------------------------------------------------------------------
		c  Assume c(3) = 0    (Enable line below if c(3) not= 0)
		c---------------------------------------------------------------------
		c    >                     + c(3) * ( r2(i1-1) + r2(i1+1) )
		c-------------------------------------------------------------------*/
	    }
	}
    }

  /*--------------------------------------------------------------------
    c     exchange boundary points
    c-------------------------------------------------------------------*/
  comm3( u, n1, n2, n3, k );

  if (MYTHREAD == 0) 
    {
      if (debug_vec[0] >= 1 ) 
	{
	  rep_nrm( u, n1, n2, n3, "   psinv", k );
	}

      if ( debug_vec[3] >= k ) 
	{
	  showall( u, n1, n2, n3 );
	}
    }
#undef u
#undef r    
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void resid( shared sh_arr_t *u, shared sh_arr_t *v, shared sh_arr_t *r,
	    int n1, int n2, int n3, double a[4], int k )
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     resid computes the residual:  r = v - Au
    c
    c     This  implementation costs  15A + 4M per result, where
    c     A and M denote the costs of Addition (or Subtraction) and 
    c     Multiplication, respectively. 
    c     Presuming coefficient a(1) is zero (the NPB assumes this,
    c     but it is thus not a general case), 3A + 1M may be eliminated,
    c     resulting in 12A + 3M.
    c     Note that this vectorizes, and is also fine for cache 
    c     based machines.  
    c-------------------------------------------------------------------*/
#define u(iz,iy,ix) u[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
#define v(iz,iy,ix) v[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
#define r(iz,iy,ix) r[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  int i3, i2, i1;
  double u1[M], u2[M];

  for (i3 = 1; i3 < n3-1; i3++) 
    {
      for (i2 = 1; i2 < n2-1; i2++) 
	{
	  for (i1 = 0; i1 < n1; i1++) 
	    {
	      u1[i1] = u( i3, (i2-1), i1) + u( i3, (i2+1), i1)
		+ u((i3-1), i2, i1) + u((i3+1), i2, i1);
	      u2[i1] = u((i3-1), (i2-1), i1) + u((i3-1), (i2+1), i1)
		+ u((i3+1), (i2-1), i1) + u((i3+1), (i2+1), i1);
	    }

	  for (i1 = 1; i1 < n1-1; i1++) 
	    {
	      r(i3, i2, i1) = v(i3, i2, i1)
		- a[0] * u(i3, i2, i1)
		/*--------------------------------------------------------------------
		  c  Assume a(1) = 0      (Enable 2 lines below if a(1) not= 0)
		  c---------------------------------------------------------------------
		  c    >                     - a(1) * ( u(i1-1,i2,i3) + u(i1+1,i2,i3)
		  c    >                              + u1(i1) )
		  c-------------------------------------------------------------------*/
		- a[2] * ( u2[i1] + u1[i1-1] + u1[i1+1] )
		- a[3] * ( u2[i1-1] + u2[i1+1] );
	    }
	}
    }

  /*--------------------------------------------------------------------
    c     exchange boundary data
    c--------------------------------------------------------------------*/
  comm3( r, n1, n2, n3, k );

  if (MYTHREAD == 0) 
    {
      if (debug_vec[0] >= 1 ) 
	{
	  rep_nrm( r, n1, n2, n3, "   resid", k );
	}

      if ( debug_vec[2] >= k ) 
	{
	  showall( r, n1, n2, n3 );
	}
    }
#undef u
#undef v
#undef r    
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void rprj3( shared sh_arr_t *r, int m1k, int m2k, int m3k,
	    shared sh_arr_t *s, int m1j, int m2j, int m3j, int k )
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     rprj3 projects onto the next coarser grid, 
    c     using a trilinear Finite Element projection:  s = r' = P r
    c     
    c     This  implementation costs  20A + 4M per result, where
    c     A and M denote the costs of Addition and Multiplication.  
    c     Note that this vectorizes, and is also fine for cache 
    c     based machines.  
    c-------------------------------------------------------------------*/
#define r(iz,iy,ix) r[MYTHREAD].arr[iz*m2k*m1k + iy*m1k + ix]
#define s(iz,iy,ix) s[MYTHREAD].arr[iz*m2j*m1j + iy*m1j + ix]
  int j3, j2, j1, i3, i2, i1, d1, d2, d3;
  double x1[M], y1[M], x2, y2;

  if (m1k == 3) 
    {
      d1 = 2;
    }
  else 
    {
      d1 = 1;
    }

  if (m2k == 3) 
    {
      d2 = 2;
    }
  else 
    {
      d2 = 1;
    }

  if (m3k == 3) 
    {
      d3 = 2;
    }
  else 
    {
      d3 = 1;
    }
  for (j3 = 1; j3 < m3j-1; j3++) 
    {
      i3 = 2*j3-d3;

      for (j2 = 1; j2 < m2j-1; j2++) 
	{
	  i2 = 2*j2-d2;

	  for (j1 = 1; j1 < m1j; j1++) 
	    {
	      i1 = 2*j1-d1;

	      x1[i1] = r((i3+1), i2, i1) + r((i3+1), (i2+2), i1)
		+ r(i3, (i2+1), i1) + r((i3+2), (i2+1), i1);
	      y1[i1] = r(i3, i2, i1) + r((i3+2), i2, i1)
		+ r(i3, (i2+2), i1) + r((i3+2), (i2+2), i1);
	    }

	  for (j1 = 1; j1 < m1j-1; j1++) 
	    {
	      i1 = 2*j1-d1;

	      y2 = r(i3, i2, (i1+1)) + r((i3+2), i2, (i1+1))
		+ r(i3, (i2+2), (i1+1)) + r((i3+2), (i2+2), (i1+1));
	      x2 = r((i3+1), i2, (i1+1)) + r((i3+1), (i2+2), (i1+1))
		+ r(i3, (i2+1), (i1+1)) + r((i3+2), (i2+1), (i1+1));
	      s(j3, j2, j1) =
		0.5 * r((i3+1), (i2+1), (i1+1))
		+ 0.25 * ( r((i3+1), (i2+1), i1) + r((i3+1), (i2+1), (i1+2)) + x2)
		+ 0.125 * ( x1[i1] + x1[i1+2] + y2)
		+ 0.0625 * ( y1[i1] + y1[i1+2] );
	    }
	}
    }

  comm3( s, m1j, m2j, m3j, k-1 );

  if (MYTHREAD == 0) 
    {	
      if (debug_vec[0] >= 1 ) 
	{
	  rep_nrm( s, m1j, m2j, m3j, "   rprj3", k-1 );
	}

      if (debug_vec[4] >= k ) 
	{
	  showall( s, m1j, m2j, m3j );
	}
    }
#undef r
#undef s
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void interp( shared sh_arr_t *z, int mm1, int mm2, int mm3,
	     shared sh_arr_t *u, int n1, int n2, int n3, int k )
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     interp adds the trilinear interpolation of the correction
    c     from the coarser grid to the current approximation:  u = u + Qu'
    c     
    c     Observe that this  implementation costs  16A + 4M, where
    c     A and M denote the costs of Addition and Multiplication.  
    c     Note that this vectorizes, and is also fine for cache 
    c     based machines.  Vector machines may get slightly better 
    c     performance however, with 8 separate "do i1" loops, rather than 4.
    c-------------------------------------------------------------------*/
#define z(iz,iy,ix) z[MYTHREAD].arr[iz*mm2*mm1 + iy*mm1 + ix]
#define u(iz,iy,ix) u[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  int i3, i2, i1, d1, d2, d3, t1, t2, t3;
  double z1[M], z2[M], z3[M];

  /*
    c note that m = 1037 in globals.h but for this only need to be
    c 535 to handle up to 1024^3
    c      integer m
    c      parameter( m=535 )
  */

  if( (n1 != 3) && (n2 != 3) && (n3 != 3) ) 
    {
      for (i3 = 0; i3 < mm3-1; i3++) 
	{
	  for (i2 = 0; i2 < mm2-1; i2++) 
	    {
	      for (i1 = 0; i1 < mm1; i1++) 
		{
		  z1[i1] = z(i3, (i2+1), i1) + z(i3, i2, i1);
		  z2[i1] = z((i3+1), i2, i1) + z(i3, i2, i1);
		  z3[i1] = z((i3+1), (i2+1), i1) + z((i3+1), i2, i1) + z1[i1];
		}

	      for (i1 = 0; i1 < mm1-1; i1++) 
		{
		  u((2*i3), (2*i2), (2*i1)) = u((2*i3), (2*i2), (2*i1))
		    + z(i3, i2, i1);
		  u((2*i3), (2*i2), (2*i1+1)) = u((2*i3), (2*i2), (2*i1+1))
		    +0.5*(z(i3, i2, (i1+1)) + z(i3, i2, i1));
		}

	      for (i1 = 0; i1 < mm1-1; i1++) 
		{
		  u((2*i3), (2*i2+1), (2*i1)) = u((2*i3), (2*i2+1), (2*i1))
		    +0.5 * z1[i1];
		  u((2*i3), (2*i2+1), (2*i1+1)) = u((2*i3), (2*i2+1), (2*i1+1))
		    +0.25*( z1[i1] + z1[i1+1] );
		}

	      for (i1 = 0; i1 < mm1-1; i1++) 
		{
		  u((2*i3+1), (2*i2), (2*i1)) = u((2*i3+1), (2*i2), (2*i1))
		    +0.5 * z2[i1];
		  u((2*i3+1), (2*i2), (2*i1+1)) = u((2*i3+1), (2*i2), (2*i1+1))
		    +0.25*( z2[i1] + z2[i1+1] );
		}

	      for (i1 = 0; i1 < mm1-1; i1++) 
		{
		  u((2*i3+1), (2*i2+1), (2*i1)) = u((2*i3+1), (2*i2+1), (2*i1))
		    +0.25* z3[i1];
		  u((2*i3+1), (2*i2+1), (2*i1+1)) = u((2*i3+1), (2*i2+1), (2*i1+1))
		    +0.125*( z3[i1] + z3[i1+1] );
		}
	    }
	}
    }
  else 
    {
      if (n1 == 3) 
	{
	  d1 = 2;
	  t1 = 1;
	}
      else 
	{
	  d1 = 1;
	  t1 = 0;
	}
         
      if (n2 == 3) 
	{
	  d2 = 2;
	  t2 = 1;
	}
      else 
	{
	  d2 = 1;
	  t2 = 0;
	}
         
      if (n3 == 3) 
	{
	  d3 = 2;
	  t3 = 1;
	} 
      else 
	{
	  d3 = 1;
	  t3 = 0;
	}
         
      for ( i3 = d3; i3 <= mm3-1; i3++) 
	{
	  for ( i2 = d2; i2 <= mm2-1; i2++) 
	    {
	      for ( i1 = d1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-d3-1), (2*i2-d2-1), (2*i1-d1-1)) =
		    u((2*i3-d3-1), (2*i2-d2-1), (2*i1-d1-1))
		    +z((i3-1), (i2-1), (i1-1));
		}

	      for ( i1 = 1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-d3-1), (2*i2-d2-1), (2*i1-t1-1)) =
		    u((2*i3-d3-1), (2*i2-d2-1), (2*i1-t1-1))
		    +0.5*(z((i3-1), (i2-1), i1)+z((i3-1), (i2-1), (i1-1)));
		}
	    }
	  for ( i2 = 1; i2 <= mm2-1; i2++) 
	    {
	      for ( i1 = d1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-d3-1), (2*i2-t2-1), (2*i1-d1-1)) =
		    u((2*i3-d3-1), (2*i2-t2-1), (2*i1-d1-1))
		    +0.5*(z((i3-1), i2, (i1-1))+z((i3-1), (i2-1), (i1-1)));
		}

	      for ( i1 = 1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-d3-1), (2*i2-t2-1), (2*i1-t1-1)) =
		    u((2*i3-d3-1), (2*i2-t2-1), (2*i1-t1-1))
		    +0.25*(z((i3-1), i2, i1)+z((i3-1), (i2-1), i1)
			   +z((i3-1), i2, (i1-1))+z((i3-1), (i2-1), (i1-1)));
		}
	    }
	}

      for ( i3 = 1; i3 <= mm3-1; i3++) 
	{
	  for ( i2 = d2; i2 <= mm2-1; i2++) 
	    {
	      for ( i1 = d1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-t3-1), (2*i2-d2-1), (2*i1-d1-1)) =
		    u((2*i3-t3-1), (2*i2-d2-1), (2*i1-d1-1))
		    +0.5*(z(i3, (i2-1), (i1-1))+z((i3-1), (i2-1), (i1-1)));
		}

	      for ( i1 = 1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-t3-1), (2*i2-d2-1), (2*i1-t1-1)) =
		    u((2*i3-t3-1), (2*i2-d2-1), (2*i1-t1-1))
		    +0.25*(z(i3, (i2-1), i1)+z(i3, (i2-1), (i1-1))
			   +z((i3-1), (i2-1), i1)+z((i3-1), (i2-1), (i1-1)));
		}
	    }
	  for ( i2 = 1; i2 <= mm2-1; i2++) 
	    {
	      for ( i1 = d1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-t3-1), (2*i2-t2-1), (2*i1-d1-1)) =
		    u((2*i3-t3-1), (2*i2-t2-1), (2*i1-d1-1))
		    +0.25*(z(i3, i2, (i1-1))+z(i3, (i2-1), (i1-1))
			   +z((i3-1), i2, (i1-1))+z((i3-1), (i2-1), (i1-1)));
		}

	      for ( i1 = 1; i1 <= mm1-1; i1++) 
		{
		  u((2*i3-t3-1), (2*i2-t2-1), (2*i1-t1-1)) =
		    u((2*i3-t3-1), (2*i2-t2-1), (2*i1-t1-1))
		    +0.125*(z(i3, i2, i1)+z(i3, (i2-1), i1)
			    +z(i3, i2, (i1-1))+z(i3, (i2-1), (i1-1))
			    +z((i3-1), i2, i1)+z((i3-1), (i2-1), i1)
			    +z((i3-1), i2, (i1-1))+z((i3-1), (i2-1), (i1-1)));
		}
	    }
	}
    }
    
  if (MYTHREAD == 0)
    {
      if (debug_vec[0] >= 1 ) 
	{
	  rep_nrm( z, mm1, mm2, mm3, "z: inter", k-1 );
	  rep_nrm( u, n1, n2, n3, "u: inter", k );
	}

      if ( debug_vec[5] >= k ) 
	{
	  showall( z, mm1, mm2, mm3 );
	  showall( u, n1, n2, n3 );
	}
    }
#undef z
#undef u
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void norm2u3(shared sh_arr_t *r, int n1, int n2, int n3,
	     double *rnm2, double *rnmu, int nx, int ny, int nz)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     norm2u3 evaluates approximations to the L2 norm and the
    c     uniform (or L-infinity or Chebyshev) norm, under the
    c     assumption that the boundaries are periodic or zero.  Add the
    c     boundaries in with half weight (quarter weight on the edges
    c     and eighth weight at the corners) for inhomogeneous boundaries.
    c-------------------------------------------------------------------*/
#define r(iz,iy,ix) r[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  double tmp;
  int i3, i2, i1, n;
  double p_s = 0, p_a = 0;

  n = nx*ny*nz;

  for (i3 = 1; i3 < n3-1; i3++) 
    {
      for (i2 = 1; i2 < n2-1; i2++) 
	{
	  for (i1 = 1; i1 < n1-1; i1++) 
	    {
	      p_s = p_s + r(i3, i2, i1) * r(i3, i2, i1);
	      tmp = fabs( r(i3, i2, i1) );
	      if (tmp > p_a)
		p_a = tmp;
	    }
	}
    }
    
  upc_lock(critical_lock);
    
  s += p_s;
  if (p_a > max) 
    max = p_a;
    
  upc_unlock(critical_lock);
    
  upc_barrier 20;
    
  *rnm2 = sqrt(s/(double)n);
  *rnmu = max;    

  if (MYTHREAD == 0) 
    {
      max = 0;
      s = 0.0;
    }     
#undef r
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void rep_nrm( shared sh_arr_t *u, int n1, int n2, int n3,
		    char *title, int kk)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     report on norm
    c-------------------------------------------------------------------*/
  double rnm2, rnmu;

  norm2u3( u, n1, n2, n3, &rnm2, &rnmu, nx[kk], ny[kk], nz[kk]);
  printf(" Level%2d in %8s: norms =%21.14e%21.14e\n",
	 kk, title, rnm2, rnmu);
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/
double lbuff[NM2];

void comm3( shared sh_arr_t *u, int n1, int n2, int n3, int kk)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     comm3 organizes the communication on all borders 
    c-------------------------------------------------------------------*/
#define u_th(th,iz,iy,ix) u[th].arr[iz*n2*n1 + iy*n1 + ix]
#define u(iz,iy,ix) u[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  int fellow;
  int i1, i2, i3;

  if( dead[kk] == 0 ) 
    {
      if( THREADS > 1 )
	{
	  upc_barrier 64000;

	  fellow = nbr[1][DIRm1][kk];    
	  for(i3=1; i3<n3-1; i3++) 
	    {
	      for(i2=0; i2<n2; i2++)
		{
		  u_th(fellow, i3, i2, (n1-1)) = u( i3, i2, 1 );
		}
	    }

	  fellow = nbr[1][DIRp1][kk];    
	  for(i3=1; i3<n3-1; i3++) 
	    {
	      for(i2=0; i2<n2; i2++)
		{
		  u_th(fellow, i3, i2, 0) = u( i3, i2, (n1-2) );
		}
	    }

          fellow = nbr[2][DIRm1][kk];
          for(i3=1; i3<n3-1; i3++)
            {
              upc_memcpy( &u_th( fellow, i3, (n2-1), 0 ),
                          &u( i3, 1, 0 ),
                          n1 * sizeof( double ));
            }

          fellow = nbr[2][DIRp1][kk];
          for(i3=1; i3<n3-1; i3++)
            {
              upc_memcpy( &u_th( fellow, i3, 0, 0 ),
                          &u( i3, (n2-2), 0 ),
                          n1 * sizeof( double ));
            }

          fellow = nbr[3][DIRm1][kk];
          upc_memcpy( &u_th( fellow, (n3-1), 0, 0 ),
                      &u( 1, 0, 0 ),
                      n2 * n1 * sizeof( double ));

          fellow = nbr[3][DIRp1][kk];
          upc_memcpy( &u_th( fellow, 0, 0, 0 ),
                      &u( (n3-2), 0, 0 ),
                      n2 * n1 * sizeof( double ));

          // synchronization check
	  upc_barrier 65000;
	}
      else 
	{
	  /* axis = 1 */
	  for( i3 = 1; i3 < n3-1; i3++)
	    {
	      for( i2 = 1; i2 < n2-1; i2++) 
		{
		  u(i3, i2, (n1-1)) = u(i3, i2, 1);
		  u(i3, i2, 0) = u(i3, i2, (n1-2));
		}
	    }

	  /* axis = 2 */
	  for( i3 = 1; i3 < n3-1; i3++)
	    {
	      for( i1 = 0; i1 < n1; i1++) 
		{
		  u(i3, (n2-1), i1) = u( i3, 1, i1);
		  u(i3, 0, i1) = u(i3, (n2-2), i1);
		}
	    }

	  /* axis = 3 */
	  for( i2 = 0; i2 < n2; i2++)
	    {
	      for( i1 = 0; i1 < n1; i1++) 
		{
		  u((n3-1), i2, i1) = u(1, i2, i1);
		  u(0, i2, i1) = u((n3-2), i2, i1);
		}
	    }
	}
    }
  else 
    {
      zero3(u, n1, n2, n3);

      upc_barrier 64000; // Need to stay in sync
      upc_barrier 65000;
    }
#undef u
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void zran3( shared sh_arr_t *z, int n1, int n2, int n3, 
	    int nx, int ny, int k)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     zran3  loads +1 at ten randomly chosen points,
    c     loads -1 at a different ten random points,
    c     and zero elsewhere.
    c-------------------------------------------------------------------*/
#define z(iz,iy,ix) z[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
#define	A	pow(5.0,13)
#define	X	314159265.e0    
    
  int i0, m0, m1;
  int i1, i2, i3, d1, e1, e2, e3;
  double xx, x0, x1, a1, a2, ai;
  double ten[MM][2], best;
  int i, j1[MM][2], j2[MM][2], j3[MM][2];
  double rdummy;

  a1 = power( A, nx );
  a2 = power( A, nx*ny );

  zero3( z, n1, n2, n3 );

  i = is1-1+nx*(is2-1+ny*(is3-1));

  ai = power( A, i );
  d1 = ie1 - is1 + 1;
  e1 = ie1 - is1 + 2;
  e2 = ie2 - is2 + 2;
  e3 = ie3 - is3 + 2;
  x0 = X;
  rdummy = randlc( &x0, ai );
    
  for (i3 = 1; i3 < e3; i3++) 
    {
      x1 = x0;
      for (i2 = 1; i2 < e2; i2++) 
	{
	  xx = x1;
	  vranlc( d1, &xx, A, (double *) &(z(i3,i2,0)) );
	  rdummy = randlc( &x1, a1 );
	}
      rdummy = randlc( &x0, a2 );
    }

  /*--------------------------------------------------------------------
    c       call comm3(z,n1,n2,n3)
    c       call showall(z,n1,n2,n3)
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     each processor looks for twenty candidates
    c-------------------------------------------------------------------*/
  for (i = 0; i < MM; i++) 
    {
      ten[i][1] = 0.0;
      j1[i][1] = 0;
      j2[i][1] = 0;
      j3[i][1] = 0;
      ten[i][0] = 1.0;
      j1[i][0] = 0;
      j2[i][0] = 0;
      j3[i][0] = 0;
    }

  for (i3 = 1; i3 < n3-1; i3++) 
    {
      for (i2 = 1; i2 < n2-1; i2++) 
	{
	  for (i1 = 1; i1 < n1-1; i1++) 
	    {
	      if( z(i3, i2, i1) > ten[0][1] ) 
		{
		  ten[0][1] = z(i3, i2, i1);
		  j1[0][1] = i1;
		  j2[0][1] = i2;
		  j3[0][1] = i3;
		  bubble( ten, j1, j2, j3, MM, 1 );
		}
	      if( z(i3, i2, i1) < ten[0][0] ) 
		{
		  ten[0][0] = z( i3, i2, i1 );
		  j1[0][0] = i1;
		  j2[0][0] = i2;
		  j3[0][0] = i3;
		  bubble( ten, j1, j2, j3, MM, 0 );
		}
	    }
	}
    }

  for( i1=0; i1<4; i1++ ) 
    {
      for( i=0; i<MM; i++ ) 
	{
	  upc_forall(i0=0; i0<2; i0++; &jg[i1][i][i0]) 
	    {
	      jg[i1][i][i0]=0;
	    }
	}
    }
  
  upc_barrier 30;
  
  /*--------------------------------------------------------------------
    c     Now which of these are globally best?
    c-------------------------------------------------------------------*/
  
  i1 = MM - 1;
  i0 = MM - 1;

  for (i = MM - 1 ; i >= 0; i--) 
    {
      best = z( j3[i1][1], j2[i1][1], j1[i1][1] );

      if( MYTHREAD == 0 )
	red_best=0;
 
      upc_barrier 20;

      upc_lock(critical_lock);
      if (red_best<best)
	{
	  red_best=best;
	  red_winner=MYTHREAD;
	}
      upc_unlock(critical_lock);

      upc_barrier 21;

      best=red_best;
      if (red_winner==MYTHREAD) 
	{
	  jg[0][i][1] = MYTHREAD;
	  jg[1][i][1] = is1 - 1 + j1[i1][1];
	  jg[2][i][1] = is2 - 1 + j2[i1][1];
	  jg[3][i][1] = is3 - 1 + j3[i1][1];
	  i1 = i1-1;
	}
      ten[i][1] = best;

      upc_barrier;

      best = z( j3[i0][0], j2[i0][0], j1[i0][0] );

      if( MYTHREAD == 0 )
	red_best=1;

      upc_barrier 22;

      upc_lock(critical_lock);
      if (red_best>best) 
	{
	  red_best=best;
	  red_winner=MYTHREAD;
	}
      upc_unlock(critical_lock);

      upc_barrier 23;

      best=red_best;
      if (red_winner==MYTHREAD) 
	{
	  jg[0][i][0] = MYTHREAD;
	  jg[1][i][0] = is1 - 1 + j1[i0][0];
	  jg[2][i][0] = is2 - 1 + j2[i0][0];
	  jg[3][i][0] = is3 - 1 + j3[i0][0];
	  i0 = i0-1;
	}
      ten[i][0] = best;

      upc_barrier;
    }

  m1 = i1+1; 
  m0 = i0+1; 

  zero3( z,n1,n2,n3 );

  for (i = MM-1; i >= m0; i--) 
    {
      z( j3[i][0], j2[i][0], j1[i][0]) = -1.0;
    }
  for (i = MM-1; i >= m1; i--) 
    {
      z( j3[i][1], j2[i][1], j1[i][1]) = 1.0;
    }

  comm3( z,n1,n2,n3,k );

  /*--------------------------------------------------------------------
    c          call showall(z,n1,n2,n3)
    c-------------------------------------------------------------------*/
#undef z
}


/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void showall( shared sh_arr_t *z, int n1, int n2, int n3)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/
#define z(iz,iy,ix) z[MYTHREAD].arr[iz*n2*n1 + iy*n1 + ix]
  int i1,i2,i3;
  int m1, m2, m3;

  m1 = min(n1,18);
  m2 = min(n2,14);
  m3 = min(n3,18);

  printf("\n");
  for (i3 = 0; i3 < m3; i3++) 
    {
      for (i1 = 0; i1 < m1; i1++) 
	{
	  for (i2 = 0; i2 < m2; i2++) 
	    {
	      printf("%6.3f", z(i3, i2, i1));
	    }
	  printf("\n");
	}
      printf(" - - - - - - - \n");
    }
  printf("\n");
#undef z
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

double power( double a, int n ) 
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     power  raises an integer, disguised as a double
    c     precision real, to an integer power
    c-------------------------------------------------------------------*/
  double aj;
  int nj;
  double rdummy;
  double power;

  power = 1.0;
  nj = n;
  aj = a;

  while (nj != 0) 
    {
      if( (nj%2) == 1 ) rdummy =  randlc( &power, aj );
      rdummy = randlc( &aj, aj );
      nj = nj/2;
    }
    
  return (power);
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void bubble( double ten[M][2], int j1[M][2], int j2[M][2],
		    int j3[M][2], int m, int ind ) 
{

  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/

  /*--------------------------------------------------------------------
    c     bubble        does a bubble sort in direction dir
    c-------------------------------------------------------------------*/

  double temp;
  int i, j_temp;

  if ( ind == 1 ) 
    {
      for (i = 0; i < m-1; i++) 
	{
	  if ( ten[i][ind] > ten[i+1][ind] ) 
	    {
	      temp = ten[i+1][ind];
	      ten[i+1][ind] = ten[i][ind];
	      ten[i][ind] = temp;

	      j_temp = j1[i+1][ind];
	      j1[i+1][ind] = j1[i][ind];
	      j1[i][ind] = j_temp;

	      j_temp = j2[i+1][ind];
	      j2[i+1][ind] = j2[i][ind];
	      j2[i][ind] = j_temp;

	      j_temp = j3[i+1][ind];
	      j3[i+1][ind] = j3[i][ind];
	      j3[i][ind] = j_temp;
	    }
	  else 
	    {
	      return;
	    }
	}
    }
  else
    {
      for (i = 0; i < m-1; i++) 
	{
	  if ( ten[i][ind] < ten[i+1][ind] ) 
	    {
	      temp = ten[i+1][ind];
	      ten[i+1][ind] = ten[i][ind];
	      ten[i][ind] = temp;

	      j_temp = j1[i+1][ind];
	      j1[i+1][ind] = j1[i][ind];
	      j1[i][ind] = j_temp;

	      j_temp = j2[i+1][ind];
	      j2[i+1][ind] = j2[i][ind];
	      j2[i][ind] = j_temp;

	      j_temp = j3[i+1][ind];
	      j3[i+1][ind] = j3[i][ind];
	      j3[i][ind] = j_temp;
	    }
	  else 
	    {
	      return;
	    }
	}
    }
}

/*--------------------------------------------------------------------
  c-------------------------------------------------------------------*/

void zero3( shared sh_arr_t *z, int n1, int n2, int n3)
{
  /*--------------------------------------------------------------------
    c-------------------------------------------------------------------*/
  upc_memset( &z[MYTHREAD].arr[0], 0, sizeof( double ) * n3 * n2 * n1 );
}

/*---- end of program ------------------------------------------------*/
 
