/**
 * This is the version with static allocation
 */
/*--------------------------------------------------------------------

  NAS Parallel Benchmarks 2.3 UPC versions - EP

  This benchmark is an UPC version of the NPB EP code.
 
  The UPC versions are developed by HPCL-GMU and are derived from 
  the OpenMP version (develloped by RWCP), derived from the serial
  Fortran versions in "NPB 2.3-serial" developed by NAS.

  Permission to use, copy, distribute and modify this software for any
  purpose with or without fee is hereby granted.
  This software is provided "as is" without express or implied warranty.
 
  Information on the UPC project at GMU is available at:

           http://hpc.gmu.edu/~upc

  Information on OpenMP activities at RWCP is available at:

           http://pdplab.trc.rwcp.or.jp/pdperf/Omni/
 
  Information on NAS Parallel Benchmarks 2.3 is available at:
 
           http://www.nas.nasa.gov/NAS/NPB/

--------------------------------------------------------------------*/
/*--------------------------------------------------------------------

  Authors: E. Barszcz
           P. Frederickson
           A. Woo
           M. Yarrow

  OpenMP C version: S. Satoh

  UPC version: S. Chauvin
			   F. Vroman
--------------------------------------------------------------------*/
#include <upc_relaxed.h>
#include "npb-C.h"
#include <string.h>
#include "globals.h"

/* parameters */
#define T_BENCH	1
#define	T_INIT	2
#define T_RESID 3
#define T_NORM	4
#define T_MG3P 	5
#define T_COMM3 6
#define T_TAKE3 7

#define DIRm1 0
#define DIRp1 1

shared int rem_buff_len[4][THREADS];
shared double buff[THREADS*4*2*NM2];

shared [] double *shared rem_buff[THREADS][4][2];

static double* pbuff[4][2];


/* global variables */
/* common /grid/ */
static int is1, is2, is3, ie1, ie2, ie3;

/* functions prototypes */
static void setup(int *n1, int *n2, int *n3, int lt);
static double power( double a, int n );
static void zran3(double *z, int n1, int n2, int n3, int nx, int ny, int k);
static void comm3(double *u, int n1, int n2, int n3, int kk);
static void bubble( double ten[M][2], int j1[M][2], int j2[M][2],
		    int j3[M][2], int m, int ind );
static void give3(int axis, int dir, double *u, int n1, int n2, int n3, int kk);
static void take3(int axis, int dir, double *u, int n1, int n2, int n3, int kk);
static void norm2u3(double *r, int n1, int n2, int n3,
		    double *rnm2, double *rnmu, int nx, int ny, int nz);
static void resid( double *u, double *v, double *r,
		   int n1, int n2, int n3, double a[4], int k );
static void mg3P(double *u, double *v, double *r, double a[4],
		 double c[4], int n1, int n2, int n3, int k);
static void psinv( double *r, double *u, int n1, int n2, int n3,
		   double c[4], int k);
static void rprj3( double *r, int m1k, int m2k, int m3k,
		   double *s, int m1j, int m2j, int m3j, int k );
static void interp( double *z, int mm1, int mm2, int mm3,
		    double *u, int n1, int n2, int n3, int k );
static void rep_nrm(double *u, int n1, int n2, int n3,
		    const char *title, int kk);
static void showall(double *z, int n1, int n2, int n3);

static double u[NR], v[NV], r[NR] ;

/**
 * upc_reduce function (equivalent to the mpi_reduce)
 */
typedef double elem_t;
typedef enum
{       MYUPC_ADD, MYUPC_MULT, MYUPC_SUB, MYUPC_AND,
        MYUPC_OR,  MYUPC_XOR,  MYUPC_LOGAND, MYUPC_LOGOR,
		MYUPC_MIN, MYUPC_MAX, MYUPC_FUNC 
} upc_operator;

void upc_reduce(shared elem_t* buffer, size_t size, int type)
{
    shared elem_t * shared *ptrs ;
    int thr_cnt;
    int p ;
    elem_t *local_array, *lp ;
				    
    ptrs = (shared elem_t * shared*)upc_all_alloc(THREADS, sizeof(elem_t)) ;
    ptrs[MYTHREAD] = (shared elem_t *)buffer ;

	local_array = (elem_t *)malloc(size * sizeof(elem_t)) ;
	lp = (elem_t *)buffer ;

	upc_barrier ;
	
	/* reduce */
	upc_forall(thr_cnt=1; thr_cnt<THREADS; thr_cnt <<= 1; continue)
	{   
		if (!(MYTHREAD%(thr_cnt<<1)))
		{   
			upc_memget(local_array, ptrs[MYTHREAD + thr_cnt], size * sizeof(elem_t)) ;
	
			for(p = 0; p < size; p++)
			{
				switch(type)
				{
				case MYUPC_ADD : lp[p] += local_array[p] ;
				break;
				case MYUPC_MAX : lp[p] = max(lp[p], local_array[p]) ;
				break;
				case MYUPC_MIN : lp[p] = min(lp[p], local_array[p]) ;
				break;
				}
			}
		}

		upc_barrier ;
	}
}   

/*--------------------------------------------------------------------
      program mg
c-------------------------------------------------------------------*/

int main(int argc, char *argv[]) 
{
    /*-------------------------------------------------------------------------
     c k is the current level. It is passed down through subroutine args
     c and is NOT global. it is the current iteration
     c------------------------------------------------------------------------*/

    int k, it;
    double t, tinit, mflops;

    static shared double sh_a[4], sh_c[4];
    double a[4], c[4];

    static shared int sh_nit;

    static shared int sh_nx, sh_ny, sh_nz;
    static shared int sh_debug_vec[8];

    static shared int sh_lt, sh_lb;

    static double rnm2;         
    static double rnmu;

   /*-------------------------------------------------------------------------
    c These arrays are in common because they are quite large
    c and probably shouldn't be allocated on the stack. They
    c are always passed as subroutine args. 
    c------------------------------------------------------------------------*/
    double epsilon = 1.0e-8;
    int n1, n2, n3, nit;
    double verify_value;
    boolean verified;

    int i, j, l;
    FILE *fp;
	int axis, dir;

	for (axis=1; axis<=3; axis++)
	    for(dir=0; dir<2; dir++) 
		{
			rem_buff[MYTHREAD][axis][dir]=(shared [] double*)&buff[MYTHREAD+(axis*2+dir)*NM2*THREADS];
			pbuff[axis][dir]=(double*)rem_buff[MYTHREAD][axis][dir];
	    }
    
    // Parameters
    if (!MYTHREAD) 
	{
		timer_clear(T_BENCH);
		timer_clear(T_INIT);
		timer_clear(T_RESID);
		timer_clear(T_NORM);
		timer_clear(T_MG3P);

		timer_start(T_INIT);

	/*----------------------------------------------------------------------
	  c Read in and broadcast input data
	  c---------------------------------------------------------------------*/

		printf("\n\n NAS Parallel Benchmarks 2.3 UPC version"
	       " - MG Benchmark\n\n");

		fp = fopen("mg.input", "r");

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
				fscanf(fp, "%d", &debug_vec[i]);
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
				debug_vec[i] = DEBUG_DEFAULT ;
	    	}
		}

		sh_nx=nx[lt];    
		sh_ny=ny[lt];    
		sh_nz=nz[lt];
		sh_lt=lt; 
		sh_nit=nit;

		for (i = 0; i <= 7; i++) 
		    sh_debug_vec[i]=debug_vec[i];

		if ( (nx[lt] != ny[lt]) || (nx[lt] != nz[lt]) ) 
		    Class = 'U';
		else 
			if( nx[lt] == 32 && nit == 4 ) 
			    Class = 'S';
			else 
				if( nx[lt] == 64 && nit == 40 ) 
				    Class = 'W';
				else 
					if( nx[lt] == 256 && nit == 20 ) 
					    Class = 'B';
				else 
					if( nx[lt] == 512 && nit == 20 ) 
					    Class = 'C';
					else 
						if( nx[lt] == 256 && nit == 4 ) 
					    	Class = 'A';
						else
						    Class = 'U';

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
    upc_barrier ;
    
    lt=sh_lt;
    nit=sh_nit; 
    lb=sh_lb;
    nx[lt]=sh_nx;ny[lt]=sh_ny;nz[lt]=sh_nz; 

    for (i = 0; i <= 7; i++) 
		debug_vec[i]=sh_debug_vec[i];

    a[0]=sh_a[0]; a[1]=sh_a[1]; a[2]=sh_a[2]; a[3]=sh_a[3];
    c[0]=sh_c[0]; c[1]=sh_c[1]; c[2]=sh_c[2]; c[3]=sh_c[3];
 
    setup(&n1,&n2,&n3,lt);

	memset(u + ir[lt],0, n1 * n2 * n3 * sizeof(double));
    zran3(v,n1,n2,n3,nx[lt],ny[lt],lt);
	norm2u3(v,n1,n2,n3,&rnm2,&rnmu,nx[lt],ny[lt],nz[lt]);

	if (!MYTHREAD) 
	{
	    printf(" Size: %3dx%3dx%3d (class %1c)\n",
		   nx[lt], ny[lt], nz[lt], Class);
	    printf(" Iterations: %3d\n", nit);
	}

	resid(u + ir[lt],v,r + ir[lt],n1,n2,n3,a,lt);

	norm2u3(r + ir[lt],n1,n2,n3,&rnm2,&rnmu,nx[lt],ny[lt],nz[lt]);

	/*c---------------------------------------------------------------------
	  c     One iteration for startup
	  c---------------------------------------------------------------------*/
	mg3P(u,v,r,a,c,n1,n2,n3,lt);

	resid(u + ir[lt],v,r,n1,n2,n3,a,lt);

	setup(&n1,&n2,&n3,lt);

	memset(u + ir[lt], 0, n1 * n2 * n3 * sizeof(double));

    zran3(v,n1,n2,n3,nx[lt],ny[lt],lt);

    upc_barrier ;
    
    if (!MYTHREAD) 
	{
		timer_stop(T_INIT);
		timer_start(T_BENCH);
    }

   	//--------------------------------------------------------------------------------
	// Beginning of the timed part
	//--------------------------------------------------------------------------------
	if(!MYTHREAD)
	{
		timer_start(T_RESID) ;
	}
	resid(u + ir[lt],v,r + ir[lt],n1,n2,n3,a,lt);
	if(!MYTHREAD)
	{
		timer_stop(T_RESID) ;
		timer_start(T_NORM) ;
	}
	norm2u3(r + ir[lt],n1,n2,n3,&rnm2,&rnmu,nx[lt],ny[lt],nz[lt]);
	if(!MYTHREAD)
	{
		timer_stop(T_NORM) ;
	}

	for ( it = 1; it <= nit; it++) 
	{
		if(!MYTHREAD)
		{
			timer_start(T_MG3P) ;
		}
	    mg3P(u,v,r,a,c,n1,n2,n3,lt);
		if(!MYTHREAD)
		{
			timer_stop(T_MG3P) ;
			timer_start(T_RESID) ;
		}
	    resid(u + ir[lt],v,r + ir[lt],n1,n2,n3,a,lt);
		if(!MYTHREAD)
		{
			timer_stop(T_RESID) ;
		}
	}

	if(!MYTHREAD)
	{
		timer_start(T_NORM) ;
	}
	norm2u3(r + ir[lt],n1,n2,n3,&rnm2,&rnmu,nx[lt],ny[lt],nz[lt]);
	if(!MYTHREAD)
	{
		timer_stop(T_NORM) ;
	}

    upc_barrier ;
   	//--------------------------------------------------------------------------------
	// End of the timed part
	//--------------------------------------------------------------------------------
    
    if (!MYTHREAD) 
	{
		timer_stop(T_BENCH);
		t = timer_read(T_BENCH);
		tinit = timer_read(T_INIT);

		printf("\n\n\tTime Resid function : %0.4f sec\n", timer_read(T_RESID)) ;
		printf("\tTime Norm2u3 function : %0.4f sec\n", timer_read(T_NORM)) ;
		printf("\tTime mg3p function : %0.4f sec\n\n", timer_read(T_MG3P)) ;
		printf("\tTime comm3 function : %0.4f sec\n\n", timer_read(T_COMM3)) ;
		printf("\n\tTime take3 function : %0.4f sec\n\n", timer_read(T_TAKE3)) ;

		verified = FALSE;
		verify_value = 0.0;

		printf(" Initialization time: %15.3f seconds\n", tinit);
		printf(" Benchmark completed\n");

		if (Class != 'U') 
		{
		    if (Class == 'S') 
				verify_value = 0.530770700573e-04;
			else if (Class == 'W') 
				verify_value = 0.250391406439e-17;  /* 40 iterations*/
				/*				0.183103168997d-044 iterations*/
			else if (Class == 'A') 
				verify_value = 0.2433365309e-5;
			else if (Class == 'B') 
				verify_value = 0.180056440132e-5;
			else if (Class == 'C') 
			verify_value = 0.570674826298e-06;

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
	    	int nn = nx[lt]*ny[lt]*nz[lt];
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

static void setup(int *n1, int *n2, int *n3, int lt) 
{
    int k;

    int dx, dy, log_p, d, i, j;
    
    
    int ax, next[4],mi[4][MAXLEVEL+1],mip[4][MAXLEVEL+1];
    int ng[4][MAXLEVEL+1];
    int idi[4], pi[4], idin[4][2];
    int s, dir,ierr;
	
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
				    give_ex[ax][k+1] = 1;
		
				if ((mip[ax][k]==3)&&(mi[ax][k]==2)) 
				    take_ex[ax][k+1] = 1;
	    	}
		}
	  
		if( mi[1][k]==2 || mi[2][k]==2 || mi[3][k]==2 )
		    dead[k] = 1;
	  
	 	m1[k] = mi[1][k];
		m2[k] = mi[2][k];
		m3[k] = mi[3][k];


		for (ax=1; ax<4; ax++) 
		{
		    // Directions are inversed from the Fortran version
	    	// because we are pulling and not pushing
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
		ir[j]=ir[j+1] + m1[j+1]*m2[j+1]*m3[j+1];
      
    if (debug_vec[1] >=  1 ) 
	{
		if (!MYTHREAD) 
		{
		    printf(" in setup, \n");
		    printf("  P#  lt  nx  ny  nz  n1  n2  n3 is1 is2 is3 ie1 ie2 ie3\n");
		}
		upc_barrier ;
	
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
		    upc_barrier ;
		}
    }
    k = lt;
}

/**
 * multigrid V-cycle routine
 * down cycle.
 * restrict the residual from the find grid to the coarse
 */
static void mg3P(double *u, double *v, double *r, double a[4],
		 double c[4], int n1, int n2, int n3, int k) 
{
    int j ;
	int dimtab = n1 * n2 ;
	int shift_k = k * dimtab ;

    for (k = lt; k >= lb+1; k--) 
	{
		j = k-1;

		rprj3(r + ir[k], m1[k], m2[k], m3[k],
		      r + ir[j], m1[j], m2[j], m3[j], k);
    }

	k = lb;

	/*  compute an approximate solution on the coarsest grid */
    memset(u + ir[k], 0,  m1[k] *  m2[k] *  m3[k] * sizeof(double));
    psinv(r + ir[k], u + ir[k], m1[k], m2[k], m3[k], c, k);

    for (k = lb+1; k <= lt-1; k++)
	{
		j = k-1;
		
		/* prolongate from level k-1  to k */
		memset(u + ir[k], 0,  m1[k] *  m2[k] *  m3[k] * sizeof(double));
		interp(u + ir[j], m1[j], m2[j], m3[j],
	       u + ir[k], m1[k], m2[k], m3[k], k);

        /* compute residual for level k */
		resid(u + ir[k], r + ir[k], r + ir[k], m1[k], m2[k], m3[k], a, k);
        /* apply smoother */
		psinv(r + ir[k], u + ir[k], m1[k], m2[k], m3[k], c, k);
    }

    j = lt - 1;
    k = lt;
	
    interp(u + ir[j], m1[j], m2[j], m3[j], u + ir[k], n1, n2, n3, k);
    resid(u + ir[k], v, r + ir[k], n1, n2, n3, a, k);
    psinv(r + ir[k], u + ir[k], n1, n2, n3, c, k);
}

/**
 * psinv applies an approximate inverse as smoother:  u = u + Cr
 * 
 * This  implementation costs  15A + 4M per result, where
 * A and M denote the costs of Addition and Multiplication.  
 * Presuming coefficient c(3) is zero (the NPB assumes this,
 * but it is thus not a general case), 2A + 1M may be eliminated,
 * resulting in 13A + 3M.
 * Note that this vectorizes, and is also fine for cache 
 * based machines.  
 */
static void psinv( double *r, double *u, int n1, int n2, int n3,
		   double c[4], int k) 
{
    int i3, i2, i1;
    double r1[M], r2[M];
	int dim = n2 * n1 ;
	double *rp, *up ;
	
    for (i3 = 1; i3 < n3-1; i3++) 
	{
		rp = r + i3 * dim ;
		up = u + i3 * dim ;

		for (i2 = 1; i2 < n2-1; i2++) 
		{
    		rp += n1 ; // = v + i3 * dim + i2 * n1
			up += n1 ; // = u + i3 * dim + i2 * n1
			
   	    	for (i1 = 0; i1 < n1; i1++) 
			{
				r1[i1] = r[(i3) * dim + (i2-1) * n1 + (i1)] + r[(i3) * dim + (i2+1) * n1 + (i1)]
				    + r[(i3-1) * dim + (i2) * n1 + (i1)] + r[(i3+1) * dim + (i2) * n1 + (i1)];
				r2[i1] = r[(i3-1) * dim + (i2-1) * n1 + (i1)] + r[(i3-1) * dim + (i2+1) * n1 + (i1)]
				    + r[(i3+1) * dim + (i2-1) * n1 + (i1)] + r[(i3+1) * dim + (i2+1) * n1 + (i1)];
	    	}
            for (i1 = 1; i1 < n1-1; i1++) 
			{
				up[i1] = up[i1]
				    + c[0] * rp[i1]
				    + c[1] * ( rp[i1-1] + rp[i1+1]
		    		+ r1[i1] )
				    + c[2] * ( r2[i1] + r1[i1-1] + r1[i1+1] );
	    	}
		}
    }

	/* exchange boundary points */
    comm3(u,n1,n2,n3,k);

    if (!MYTHREAD) 
	{
		if (debug_vec[0] >= 1 ) 
	    	rep_nrm(u,n1,n2,n3,"   psinv",k);

		if ( debug_vec[3] >= k ) 
		    showall(u,n1,n2,n3);
    }
}

/**
 * resid computes the residual:  r = v - Au
 * 
 * This  implementation costs  15A + 4M per result, where
 * A and M denote the costs of Addition (or Subtraction) and 
 * Multiplication, respectively. 
 * Presuming coefficient a(1) is zero (the NPB assumes this,
 * but it is thus not a general case), 3A + 1M may be eliminated,
 * resulting in 12A + 3M.
 * Note that this vectorizes, and is also fine for cache 
 * based machines.  
 */
static double t = 0.0 ;

static void resid( double *u, double *v, double *r,
		   int n1, int n2, int n3, double a[4], int k ) 
{
    int i3, i2, i1;
    double u1[M], u2[M];
	int dim = n2 * n1 ;
	double *vp, *up, *rp ;
	
	// Remember to remove iter declaration 
	timer_clear(8) ;
	timer_start(8) ;

	for (i3 = 1; i3 < n3-1; i3++) 
	{
		vp = v + i3 * dim ;
		up = u + i3 * dim ;
		rp = r + i3 * dim ;
		for (i2 = 1; i2 < n2-1; i2++) 
		{
			vp += n1 ; // = v + i3 * dim + i2 * n1
			rp += n1 ; // = u + i3 * dim + i2 * n1
			up += n1 ; // = u + i3 * dim + i2 * n1
			
            for (i1 = 0; i1 < n1; i1++) 
			{
				u1[i1] = u[(i3) * dim + (i2-1) * n1 + (i1)] + u[(i3) * dim + (i2+1) * n1 + (i1)]
				    + u[(i3-1) * dim + (i2) * n1 + (i1)] + u[(i3+1) * dim + (i2) * n1 + (i1)];
				u2[i1] = u[(i3-1) * dim + (i2-1) * n1 + (i1)] + u[(i3-1) * dim + (i2+1) * n1 + (i1)]
				    + u[(i3+1) * dim + (i2-1) * n1 + (i1)] + u[(i3+1) * dim + (i2+1) * n1 + (i1)];
		    }
		    for (i1 = 1; i1 < n1-1; i1++) 
			{
				rp[i1] = vp[i1]
				    - a[0] * up[i1]
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

	timer_stop(8) ;
	t += timer_read(8) ;
	
	/* exchange boundary datas */
    comm3(r,n1,n2,n3,k);

    if (!MYTHREAD) 
	{
		if (debug_vec[0] >= 1 ) 
	    	rep_nrm(r,n1,n2,n3,"   resid",k);

		if ( debug_vec[2] >= k ) 
		    showall(r,n1,n2,n3);
    }
}

/**
 * rprj3 projects onto the next coarser grid,
 * using a trilinear Finite Element projection:  s = r' = P r
 *      
 * This  implementation costs  20A + 4M per result, where
 * A and M denote the costs of Addition and Multiplication.  
 * Note that this vectorizes, and is also fine for cache 
 * based machines.  
 */
static void rprj3( double *r, int m1k, int m2k, int m3k,
		   double *s, int m1j, int m2j, int m3j, int k ) 
{
    int j3, j2, j1, i3, i2, i1, d1, d2, d3;
    double x1[M], y1[M], x2, y2;
	int dimr = m1k * m2k ;
	int dims = m1j * m2j ;
	double  *sp, *rp ;
	
    if (m1k == 3) 
        d1 = 2;
    else 
        d1 = 1;

    if (m2k == 3) 
        d2 = 2;
    else
        d2 = 1;

    if (m3k == 3) 
        d3 = 2;
    else 
        d3 = 1;
    
    for (j3 = 1; j3 < m3j - 1; j3++) 
	{
		i3 = 2 * j3 - d3 ;
		
		for (j2 = 1; j2 < m2j-1; j2++) 
		{
            i2 = 2 * j2 - d2 ;
			rp = r + (i3 + 1) * dimr + (i2 + 1) * m1k ; // = u + i3 * dim + i2 * n1
			//sp = s + i3 * dims + i2 * n1 ; // = s + i3 * dim + i2 * n1

            for (j1 = 1; j1 < m1j; j1++) 
			{
				i1 = 2 * j1 - d1 ;
				x1[i1] = r[(i3+1) * dimr + (i2) * m1k + (i1)] + rp[m1k + i1]
				    + r[(i3) * dimr + (i2+1) * m1k + (i1)] + rp[dimr + i1];
				y1[i1] = r[(i3) * dimr + (i2) * m1k + (i1)] + r[(i3+2) * dimr + (i2) * m1k + (i1)]
				    + r[(i3) * dimr + (i2+2) * m1k + (i1)] + rp[dimr + m1k + (i1)];
		    }

            for (j1 = 1; j1 < m1j - 1; j1++) 
			{
				i1 = 2 * j1 - d1 ;
				y2 = r[(i3) * dimr + (i2) * m1k + (i1+1)] + r[(i3+2) * dimr + (i2) * m1k + (i1+1)]
				    + r[(i3) * dimr + (i2+2) * m1k + (i1+1)] + rp[dimr + m1k + (i1+1)];
				x2 = r[(i3+1) * dimr + (i2) * m1k + (i1+1)] + rp[m1k + (i1+1)]
				    + r[(i3) * dimr + (i2+1) * m1k + (i1+1)] + rp[dimr + (i1+1)];
				s[j3 * dims + j2 * m1j + j1] =
				    0.5 * rp[i1+1]
				    + 0.25 * ( rp[i1] + rp[i1+2] + x2)
				    + 0.125 * ( x1[i1] + x1[i1+2] + y2)
				    + 0.0625 * ( y1[i1] + y1[i1+2] );
	    	}
		}
    }

    comm3(s, m1j, m2j, m3j, k - 1) ;

    if (!MYTHREAD) 
	{
		if (debug_vec[0] >= 1 ) 
	    	rep_nrm(s, m1j, m2j, m3j, "   rprj3", k - 1) ;

		if (debug_vec[4] >= k ) 
		    showall(s, m1j, m2j, m3j) ;
    }
}

/**
 * interp adds the trilinear interpolation of the correction
 * from the coarser grid to the current approximation:  u = u + Qu'
 * 
 * Observe that this  implementation costs  16A + 4M, where
 * A and M denote the costs of Addition and Multiplication.  
 * Note that this vectorizes, and is also fine for cache 
 * based machines.  Vector machines may get slightly better 
 * performance however, with 8 separate "do i1" loops, rather than 4.
 *
 * note that m = 1037 in globals.h but for this only need to be
 * 535 to handle up to 1024^3
 * integer m
 * parameter( m=535 )
 */
static void interp( double *z, int mm1, int mm2, int mm3,
		    double *u, int n1, int n2, int n3, int k ) 
{
    int i3, i2, i1, d1, d2, d3, t1, t2, t3;
	int dimz = mm1 * mm2 ;
	int dimu = n1 * n2 ;
    double z1[M], z2[M], z3[M];

    if ( n1 != 3 && n2 != 3 && n3 != 3 ) 
	{
		for (i3 = 0; i3 < mm3-1; i3++) 
		{
            for (i2 = 0; i2 < mm2-1; i2++) 
			{
				for (i1 = 0; i1 < mm1; i1++) 
				{
				    z1[i1] = z[(i3) * dimz + (i2+1) * mm1 + (i1)] + z[(i3) * dimz + (i2) * mm1 + (i1)];
				    z2[i1] = z[(i3+1) * dimz + (i2) * mm1 + (i1)] + z[(i3) * dimz + (i2) * mm1 + (i1)];
				    z3[i1] = z[(i3+1) * dimz + (i2+1) * mm1 + (i1)] + z[(i3+1) * dimz + (i2) * mm1 + (i1)] + z1[i1];
				}
				for (i1 = 0; i1 < mm1-1; i1++) 
				{
				    u[(2*i3) * dimu + (2*i2) * n1 + (2*i1)] = u[(2*i3) * dimu + (2*i2) * n1 + (2*i1)]
						+z[(i3) * dimz + (i2) * mm1 + (i1)];
				    u[(2*i3) * dimu + (2*i2) * n1 + (2*i1+1)] = u[(2*i3) * dimu + (2*i2) * n1 + (2*i1+1)]
						+0.5*(z[(i3) * dimz + (i2) * mm1 + (i1+1)] + z[(i3) * dimz + (i2) * mm1 + (i1)]);
				}
				for (i1 = 0; i1 < mm1-1; i1++) 
				{
				    u[(2*i3) * dimu + (2*i2+1) * n1 + (2*i1)] = u[(2*i3) * dimu + (2*i2+1) * n1 + (2*i1)]
						+0.5 * z1[i1];
				    u[(2*i3) * dimu + (2*i2+1) * n1 + (2*i1+1)] = u[(2*i3) * dimu + (2*i2+1) * n1 + (2*i1+1)]
						+0.25*( z1[i1] + z1[i1+1] );
				}
				for (i1 = 0; i1 < mm1-1; i1++) 
				{
				    u[(2*i3+1) * dimu + (2*i2) * n1 + (2*i1)] = u[(2*i3+1) * dimu + (2*i2) * n1 + (2*i1)]
						+0.5 * z2[i1];
				    u[(2*i3+1) * dimu + (2*i2) * n1 + (2*i1+1)] = u[(2*i3+1) * dimu + (2*i2) * n1 + (2*i1+1)]
						+0.25*( z2[i1] + z2[i1+1] );
				}
				for (i1 = 0; i1 < mm1-1; i1++) 
				{
				    u[(2*i3+1) * dimu + (2*i2+1) * n1 + (2*i1)] = u[(2*i3+1) * dimu + (2*i2+1) * n1 + (2*i1)]
						+0.25* z3[i1];
				    u[(2*i3+1) * dimu + (2*i2+1) * n1 + (2*i1+1)] = u[(2*i3+1) * dimu + (2*i2+1) * n1 + (2*i1+1)]
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
				    u[(2*i3-d3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-d1-1)] =
						u[(2*i3-d3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-d1-1)]
						+z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)];
				}
				for ( i1 = 1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-d3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-t1-1)] =
						u[(2*i3-d3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-t1-1)]
						+0.5*(z[(i3-1) * dimz + (i2-1) * mm1 + (i1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)]);
				}
		    }
            for ( i2 = 1; i2 <= mm2-1; i2++) 
			{
				for ( i1 = d1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-d3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-d1-1)] =
						u[(2*i3-d3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-d1-1)]
						+0.5*( z[(i3-1) * dimz + (i2) * mm1 + (i1-1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)] );
				}
				for ( i1 = 1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-d3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-t1-1)] =
						u[(2*i3-d3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-t1-1)]
						+0.25*( z[(i3-1) * dimz + (i2) * mm1 + (i1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1)]
				        + z[(i3-1) * dimz + (i2) * mm1 + (i1-1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)] );
				}
		    }
		}
		for ( i3 = 1; i3 <= mm3-1; i3++) 
		{
        	for ( i2 = d2; i2 <= mm2-1; i2++) 
			{
				for ( i1 = d1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-t3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-d1-1)] =
						u[(2*i3-t3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-d1-1)]
						+0.5*(z[(i3) * dimz + (i2-1) * mm1 + (i1-1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)]);
				}
				for ( i1 = 1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-t3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-t1-1)] =
						u[(2*i3-t3-1) * dimu + (2*i2-d2-1) * n1 + (2*i1-t1-1)]
						+0.25*(z[(i3) * dimz + (i2-1) * mm1 + (i1)] + z[(i3) * dimz + (i2-1) * mm1 + (i1-1)]
				        + z[(i3-1) * dimz + (i2-1) * mm1 + (i1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)]);
				}
		    }
		    for ( i2 = 1; i2 <= mm2-1; i2++) 
			{
				for ( i1 = d1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-t3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-d1-1)] =
						u[(2*i3-t3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-d1-1)]
						+0.25*(z[(i3) * dimz + (i2) * mm1 + (i1-1)] + z[(i3) * dimz + (i2-1) * mm1 + (i1-1)]
				        + z[(i3-1) * dimz + (i2) * mm1 + (i1-1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)]);
				}
				for ( i1 = 1; i1 <= mm1-1; i1++) 
				{
				    u[(2*i3-t3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-t1-1)] =
						u[(2*i3-t3-1) * dimu + (2*i2-t2-1) * n1 + (2*i1-t1-1)]
						+0.125*(z[(i3) * dimz + (i2) * mm1 + (i1)] + z[(i3) * dimz + (i2-1) * mm1 + (i1)]
						+ z[(i3) * dimz + (i2) * mm1 + (i1-1)] + z[(i3) * dimz + (i2-1) * mm1 + (i1-1)]
						+ z[(i3-1) * dimz + (i2) * mm1 + (i1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1)]
						+ z[(i3-1) * dimz + (i2) * mm1 + (i1-1)] + z[(i3-1) * dimz + (i2-1) * mm1 + (i1-1)]);
				}
		    }
		}
    }
    
    if (!MYTHREAD)
	{
    	if (debug_vec[0] >= 1 ) 
		{
			rep_nrm(z,mm1,mm2,mm3,"z: inter",k-1);
			rep_nrm(u,n1,n2,n3,"u: inter",k);
    	}

	    if ( debug_vec[5] >= k ) 
		{
			showall(z,mm1,mm2,mm3);
			showall(u,n1,n2,n3);
    	}
	}
}

/**
 * norm2u3 evaluates approximations to the L2 norm and the
 * uniform (or L-infinity or Chebyshev) norm, under the
 * assumption that the boundaries are periodic or zero.  Add the
 * boundaries in with half weight (quarter weight on the edges
 * and eighth weight at the corners) for inhomogeneous boundaries.
 */
static void norm2u3(double *r, int n1, int n2, int n3,
		    double *rnm2, double *rnmu, int nx, int ny, int nz) 
{
    static shared double s[THREADS]; // Should be zeroed as being static
    static shared double max[THREADS];
    
    int i3, i2, i1, n;
    double p_s = 0, p_a = 0;
	int dim = n2 * n1 ;
	double rv ;
	
    n = nx * ny * nz ;

    for (i3 = 1; i3 < n3-1; i3++) 
	{
		for (i2 = 1; i2 < n2-1; i2++) 
		{
            for (i1 = 1; i1 < n1-1; i1++) 
			{
				rv = r[i3 * dim + i2 * n1 + i1] ;
				p_s = p_s + rv * rv ;
				p_a = max(p_a, fabs(rv)) ;
		    }
		}
    }
    
	*((double *)(max + MYTHREAD)) = p_a ;
	upc_reduce(max + MYTHREAD , 1, MYUPC_MAX) ;

	*((double *)(s + MYTHREAD)) = p_s ;
	upc_reduce(s + MYTHREAD , 1, MYUPC_ADD) ;
    
    *rnm2 = sqrt(s[0]/(double)n);
    *rnmu = max[0];   
}

/**
 * report on norm
 */
static void rep_nrm(double *u, int n1, int n2, int n3,
		    const char *title, int kk) 
{
    double rnm2, rnmu;


    norm2u3(u,n1,n2,n3,&rnm2,&rnmu,nx[kk],ny[kk],nz[kk]);
    printf(" Level%2d in %8s: norms =%21.14e%21.14e\n",
	   kk, title, rnm2, rnmu);
}

/**
 * comm3 organizes the communication on all borders
 */
static void comm3(double *u, int n1, int n2, int n3, int kk) 
{
    int axis;
    static int cnt;
    
    int i1, i2, i3;
	int dim = n1 *n2 ;
	
	timer_start(T_COMM3) ;
	
    if (!dead[kk]) 
	{
		if(THREADS>1) 
		{
		    for (axis=1; axis<=3; axis++) 
			{
				give3(axis, DIRm1, u, n1, n2, n3, kk);
				give3(axis, DIRp1, u, n1, n2, n3, kk);
				upc_barrier ;
				take3(axis, DIRm1, u, n1, n2, n3, kk);
				take3(axis, DIRp1, u, n1, n2, n3, kk);
	    	} 
		} 
		else 
		{
    		/* axis = 1 */
			for ( i3 = 1; i3 < n3-1; i3++)
			{
				for ( i2 = 1; i2 < n2-1; i2++) 
				{
				    u[i3 * dim + i2 * n1 + n1 - 1] = u[i3 * dim + i2 * n1 + 1];
				    u[i3 * dim + i2 * n1] = u[i3 * dim + i2 * n1 + n1 - 2];
				}
			}

		    /* axis = 2 */
		    for ( i3 = 1; i3 < n3-1; i3++)
			{
				memcpy(u + i3 * dim + (n2 - 1) *  n1, u + i3 * dim + n1, n1 * sizeof(double)) ;
				memcpy(u + i3 * dim, u + i3 * dim + (n2 - 2) * n1, n1 * sizeof(double)) ;
			}

	    	/* axis = 3 */
	    	for ( i2 = 0; i2 < n2; i2++)
			{
				memcpy(u + (n3 - 1) * dim + i2 * n1, u + dim + i2 * n1, n1 * sizeof(double)) ;
				memcpy(u + i2 * n1, u + (n3-2) * dim + i2 * n1, n1 * sizeof(double)) ;
			}
		}
    } 
	else 
	{
		memset(u, 0,  n1 *  n2 *  n3 * sizeof(double));

		for (axis=1; axis<=3; axis++) 
		{
		    upc_barrier ;
		}
    }
	timer_stop(T_COMM3) ;
}

static void give3(int axis, int dir, double *u, int n1, int n2, int n3, int k) 
{
    int i3,i2,i1;
    
    int b_len=0;
	int dim = n1 * n2 ;
	
    switch(axis) 
	{
    case 1:
	if (dir==DIRm1)
	    for(i3=1; i3<n3-1; i3++) 
			for(i2=0; i2<n2; i2++)
			    pbuff[axis][dir][b_len++]=u[(i3 * dim) + (i2 * n1) + (1)];
	else
	    for(i3=1; i3<n3-1; i3++) 
			for(i2=0; i2<n2; i2++) 
		    	pbuff[axis][dir][b_len++]=u[(i3 * dim) + (i2 * n1) + (n1-2)];
	break;
	
    case 2:
	if (dir==DIRm1)
	    for(i3=1; i3<n3-1; i3++) 
			for(i1=0; i1<n1; i1++)
			    pbuff[axis][dir][b_len++]=u[(i3 * dim) + n1 + (i1)];
	else
	    for(i3=1; i3<n3-1; i3++) 
			for(i1=0; i1<n1; i1++)
				pbuff[axis][dir][b_len++]=u[(i3 * dim) + ((n2-2) * n1) + (i1)];
	break;
	
    case 3:
	if (dir==DIRm1)
	    for(i2=0; i2<n2; i2++)
			for(i1=0; i1<n1; i1++) 
		    	pbuff[axis][dir][b_len++]=u[dim + (i2 * n1) + (i1)];
	else
	    for(i2=0; i2<n2; i2++)
			for(i1=0; i1<n1; i1++) 
		    	pbuff[axis][dir][b_len++]=u[(n3-2) * dim + (i2 * n1) + (i1)];
	break;
    }

    rem_buff_len[axis][MYTHREAD]=b_len;
}


static double lbuff[NM2];

static void take3(int axis, int dir, double *u, int n1, int n2, int n3, int k) 
{
    
    int i1, i2, i3;
    int b_i=0;
   	int dim = n2 * n1 ;
    int fellow = nbr[axis][dir][k];
   
	timer_start(T_TAKE3) ;

    upc_memget(lbuff, rem_buff[fellow][axis][dir], rem_buff_len[axis][fellow]*sizeof(double));

    switch(axis) 	
	{
    case 1:
	if (dir==DIRm1)
	    for(i3=1; i3<n3-1; i3++) 
			for(i2=0; i2<n2; i2++)
			    u[(i3 * dim) + (i2 * n1) + (n1-1)]=lbuff[b_i++];
	else
	    for(i3=1; i3<n3-1; i3++) 
			for(i2=0; i2<n2; i2++)
		    	u[(i3 * dim) + (i2 * n1)]=lbuff[b_i++];
	break;
	
    case 2:
	if (dir==DIRm1)
	{
	    for(i3=1; i3<n3-1; i3++)
		{
			memcpy(u + i3 * dim + (n2-1) * n1, lbuff + b_i + 1, n1 * sizeof(double)) ;
			b_i += n1 ;
		}
	}
	else
	    for(i3=1; i3<n3-1; i3++) 
		{
			memcpy(u + i3 * dim, lbuff + b_i, n1 * sizeof(double)) ;
			b_i += n1 ;
		}
	break;
	
    case 3:
	if (dir==DIRm1)
	    for(i2=0; i2<n2; i2++)
		{
			memcpy(u + (n3-1) * dim + i2 * n1, lbuff + b_i + 1, n1 * sizeof(double)) ;
			b_i += n1 ;
		}
	else
	    for(i2=0; i2<n2; i2++)
		{
			memcpy(u + i2 * n1, lbuff + b_i, n1 * sizeof(double)) ;
			b_i += n1 ;
		}
	break;    
    }
	timer_stop(T_TAKE3) ;
}

/**
 * zran3 loads +1  at ten randomly chosen points,
 * loads -1 at a different ten random points,
 * and zero elsewhere.
 */
static void zran3(double *z, int n1, int n2, int n3, int nx, int ny, int k) 
{
#define MM	10
#define	A	pow(5.0,13)
#define	X	314159265.e0    
    
    int i0, m0, m1;
    int i1, i2, i3, d1, e1, e2, e3;
    double xx, x0, x1, a1, a2, ai;

    double ten[MM][2], best ;
	static shared double bests[THREADS] ;
    int i, j1[MM][2], j2[MM][2], j3[MM][2];

    double rdummy;

    static shared double red_best;
    static shared int red_winner;

    static shared int jg[4][MM][2];
   	int dim = n2 * n1 ;
	
    a1 = power( A, nx );
    a2 = power( A, nx*ny );

    memset(z, 0, n1 * n2 * n3 * sizeof(double));

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
            vranlc( d1, &xx, A, &z[i3 * dim + i2 * n1]);
            rdummy = randlc( &x1, a1 );
		}
		rdummy = randlc( &x0, a2 );
    }

    /*--------------------------------------------------------------------
             call comm3(z,n1,n2,n3)
	 -------------------------------------------------------------------*/

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
				if ( z[i3 * dim + i2 * n1 + i1] > ten[0][1] ) 
				{
				    ten[0][1] = z[i3 * dim + i2 * n1 + i1];
				    j1[0][1] = i1;
				    j2[0][1] = i2;
				    j3[0][1] = i3;
				    bubble( ten, j1, j2, j3, MM, 1 );
				}
				if ( z[i3 * dim + i2 * n1 + i1] < ten[0][0] ) 
				{
				    ten[0][0] = z[i3 * dim + i2 * n1 + i1];
				    j1[0][0] = i1;
				    j2[0][0] = i2;
				    j3[0][0] = i3;
				    bubble( ten, j1, j2, j3, MM, 0 );
				}
	    	}
		}
    }

    upc_forall(i1=0; i1<4; i1++;) 
	{
		upc_forall(i=0; i<MM; i++; ) 
		{
	    	upc_forall(i0=0; i0<2; i0++; &jg[i1][i][i0]) 
				jg[i1][i][i0]=0;
		}
    }
    
    upc_barrier ;
    
    /*--------------------------------------------------------------------
      c     Now which of these are globally best?
      c-------------------------------------------------------------------*/

    
    i1 = MM - 1;
    i0 = MM - 1;

    for (i = MM - 1 ; i >= 0; i--) 
	{

		bests[MYTHREAD] = z[j3[i1][1] * dim + j2[i1][1] * n1 + j1[i1][1]];

		upc_reduce(bests + MYTHREAD, 1, MYUPC_MAX) ;

		ten[i][1] = bests[0] ;

		if(ten[i][1] == z[j3[i1][1] * dim + j2[i1][1] * n1 +j1[i1][1]])
		{
        	jg[0][i][1] = MYTHREAD;
		    jg[1][i][1] = is1 - 1 + j1[i1][1];
		    jg[2][i][1] = is2 - 1 + j2[i1][1];
	    	jg[3][i][1] = is3 - 1 + j3[i1][1];
		    i1 = i1-1;
		}
	
		upc_barrier ;

		bests[MYTHREAD] = z[j3[i0][0] * dim + j2[i0][0] * n1 + j1[i0][0]] ;

		upc_reduce(bests + MYTHREAD, 1, MYUPC_MIN) ;

		ten[i][0] = bests[0] ;
	
		if(ten[i][0] == z[j3[i0][0] * dim + j2[i0][0] * n1 + j1[i0][0]])
		{
	    	jg[0][i][0] = MYTHREAD;
		    jg[1][i][0] = is1 - 1 + j1[i0][0];
		    jg[2][i][0] = is2 - 1 + j2[i0][0];
	    	jg[3][i][0] = is3 - 1 + j3[i0][0];
		    i0 = i0-1;
		}
	    upc_barrier ;
	}

    m1 = i1+1; 
    m0 = i0+1; 

#ifdef DEBUG
	if (!MYTHREAD) 
	{
	    printf(" negative charges at");
	    for (i = 0; i < MM; i++) 
		{
			if (i%5 == 0) 
				printf("\n");
			printf(" (%3d,%3d,%3d)", jg[1][i][0], jg[2][i][0], jg[3][i][0]);
	    }
	    printf("\n positive charges at");
	    for (i = 0; i < MM; i++) 
		{
			if (i%5 == 0) 
				printf("\n");
			printf(" (%3d,%3d,%3d)", jg[1][i][1], jg[2][i][1], jg[3][i][1]);
	    }
	    printf("\n small random numbers were\n");
	    for (i = MM-1; i >= 0; i--) 
		{
			printf(" %15.8e", ten[i][0]);
	    }
	    printf("\n and they were found on processor number\n");
	    for (i = MM-1; i >= 0; i--) 
		{
			printf(" %4d", jg[0][i][0]);
	    }
	    printf("\n large random numbers were\n");
	    for (i = MM-1; i >= 0; i--) 
		{
			printf(" %15.8e", ten[i][1]);
	    }
	    printf("\n and they were found on processor number\n");
	    for (i = MM-1; i >= 0; i--) 
		{
			printf(" %4d", jg[0][i][1]);
	    }
	    printf("\n");
	}
#endif

    memset(z, 0, n1 * n2 * n3 * sizeof(double));

    for (i = MM-1; i >= m0; i--) 
		z[j3[i][0] * dim + j2[i][0] * n1 + j1[i][0]] = -1.0;
    
    for (i = MM-1; i >= m1; i--) 
		z[j3[i][1] * dim + j2[i][1] * n1 + j1[i][1]] = 1.0;

    comm3(z,n1,n2,n3,k) ;
}

static void showall(double *z, int n1, int n2, int n3) 
{
    int i1,i2,i3;
    int m1, m2, m3;
	int dim = n1 * n2 ;
	
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
				printf("%6.3f", z[i3 * dim + i2 * n1 + i1]);
	    	}
		    printf("\n");
		}
		printf(" - - - - - - - \n");
    }
    printf("\n");
}

/**
 * power  raises an integer, disguised as a double
 * precision real, to an integer power
 */
static double power( double a, int n ) 
{
    double aj;
    int nj;
    double rdummy;
    double power;

    power = 1.0;
    nj = n;
    aj = a;

    while (nj != 0) 
	{
		if( (nj%2) == 1 ) 
			rdummy =  randlc( &power, aj );
		rdummy = randlc( &aj, aj );
		nj = nj / 2;
    }
    
    return (power);
}

/**
 * Does a bubble sort in direction dir
 */
static void bubble( double ten[M][2], int j1[M][2], int j2[M][2],
		    int j3[M][2], int m, int ind ) 
{
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
				return;
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
				return;
		}
    }
}

/*---- end of program ------------------------------------------------*/

