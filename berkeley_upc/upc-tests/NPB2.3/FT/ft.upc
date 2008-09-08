/*--------------------------------------------------------------------

  NAS Parallel Benchmarks 2.3 UPC versions - FT

  This benchmark is an UPC version of the NPB FT code.
 
  The UPC versions are developed by HPCL-GWU and are derived from 
  the OpenMP version (develloped by RWCP), derived from the serial
  Fortran versions in "NPB 2.3-serial" developed by NAS.

  Permission to use, copy, distribute and modify this software for any
  purpose with or without fee is hereby granted.
  This software is provided "as is" without express or implied warranty.
 
  Information on the UPC project at GWU is available at:

           http://upc.gwu.edu

  Information on OpenMP activities at RWCP is available at:

           http://pdplab.trc.rwcp.or.jp/pdperf/Omni/
 
  Information on NAS Parallel Benchmarks 2.3 is available at:
 
           http://www.nas.nasa.gov/NAS/NPB/

--------------------------------------------------------------------*/
/*--------------------------------------------------------------------

  Authors: D. Bailey
           W. Saphir

  OpenMP C version: S. Satoh

  UPC version: S. Chauvin
--------------------------------------------------------------------*/

#include <assert.h>
#include <upc_relaxed.h>

/* WITH_LIBCOLLOP
 *
 * This flag should be defined to try to compile using libupc_collop
 * (which includes upc_all_reduce*). However, for some yet unknown
 * reason, the checksum() function will end up with a seg. fault when
 * using upc_all_reduce (as of Jan 2002).
 */
#ifdef WITH_LIBCOLLOP
# include <upc_collop.h>
#endif

#include <npb-C.h>

/* global variables */
#include "global.h"

/* function declarations */
static void evolve (dcomplex * u0, dcomplex * u1,
		    int t, int *indexmap, int d[3]);
static void compute_initial_conditions (dcomplex u0[NZ][NY][NX], int d[3]);
static void ipow46 (double a, int exponent, double *result);
static void setup (void);
static void compute_indexmap (int *indexmap, int d[3]);
static void print_timers (void);
static void fft (int dir, dcomplex x1[NZ][NY][NX], dcomplex x2[NZ][NY][NX]);
static void cffts1 (int is, int d[3], dcomplex * x,
		    dcomplex * xout,
		    dcomplex y0[NX][FFTBLOCKPAD],
		    dcomplex y1[NX][FFTBLOCKPAD]);
static void cffts2 (int is, int d[3], dcomplex * x,
		    dcomplex * xout,
		    dcomplex y0[NX][FFTBLOCKPAD],
		    dcomplex y1[NX][FFTBLOCKPAD]);
static void cffts3 (int is, int d[3], dcomplex * x,
		    dcomplex * xout,
		    dcomplex y0[NX][FFTBLOCKPAD],
		    dcomplex y1[NX][FFTBLOCKPAD]);
static void fft_init (int n);
static void cfftz (int is, int m, int n, dcomplex x[NX][FFTBLOCKPAD],
		   dcomplex y[NX][FFTBLOCKPAD]);
static void fftz2 (int is, int l, int m, int n, int ny, int ny1,
		   dcomplex u[NX], dcomplex x[NX][FFTBLOCKPAD],
		   dcomplex y[NX][FFTBLOCKPAD]);
static int ilog2 (int n);
static void checksum (int i, dcomplex u1[NZ][NY][NX], int d[3]);
static void verify (int d1, int d2, int d3, int nt,
		    boolean * verified, char *class);

void transpose_x_yz (int l1, int l2,
		     dcomplex xin[], dcomplex xout[]);
void transpose_xy_z (int l1, int l2,
		     dcomplex xin[], dcomplex xout[]);
void transpose2_local (int n1, int n2,
		       dcomplex xin[], dcomplex xout[]);
void transpose2_global (shared[]dcomplex * xin[THREADS], dcomplex xout[]);
void transpose2_finish (int n1, int n2,
			dcomplex xin[], dcomplex xout[]);


#define NBP 100

/* Static reshuffle array */
typedef struct
  {
    dcomplex arr[NX*NY*NZ];
  }
reshuffle_arr_line;

shared[1] reshuffle_arr_line big_reshuffle_arr[THREADS];


/* Debugging tool */
typedef struct
  {
    double val;
    int x, y, z;
  }
p3d;



#ifndef DEBUG_OUTPUT
#define output(a, b, c, d, e)
#else
void 
output (dcomplex * o, int nx, int ny, int nz, char *name)
{
  FILE *f;
  int x, y, z;
  int i;
  static int count;
  char buf[100];
  int marker = 0x00200000;

  sprintf (buf, "output_%02d_%04d_%s", MYTHREAD, count++, name);

  f = fopen (buf, "w");

  if (f == NULL)
    perror ("output");

  fwrite (&marker, sizeof (marker), 1, f);
  fwrite (o, sizeof (*o), nx * ny * nz, f);
  fwrite (&marker, sizeof (marker), 1, f);

  fclose (f);
}
#endif

/*--------------------------------------------------------------------
c FT benchmark
c-------------------------------------------------------------------*/

int 
main (int argc, char **argv)
{

  /*c-------------------------------------------------------------------
     c------------------------------------------------------------------- */

  int i;

    /*------------------------------------------------------------------
      c u0, u1, u2 are the main arrays in the problem. 
      c Depending on the decomposition, these arrays will have different 
      c dimensions. To accomodate all possibilities, we allocate them as 
      c one-dimensional arrays and pass them to subroutines for different 
      c views
      c  - u0 contains the initial (transformed) initial condition
      c  - u1 and u2 are working arrays
      c  - indexmap maps i,j,k of u0 to the correct i^2+j^2+k^2 for the
      c    time evolution operator. 
      c-----------------------------------------------------------------*/

    /*--------------------------------------------------------------------
      c Large arrays are in common so that they are allocated on the
      c heap rather than the stack. This common block is not
      c referenced directly anywhere else. Padding is to avoid accidental 
      c cache problems, since all array sizes are powers of two.
      c-------------------------------------------------------------------*/
  static dcomplex u0[NZ][NY][NX];
//    static dcomplex pad1[3];
  static dcomplex u1[NZ][NY][NX];
//    static dcomplex pad2[3];
  static dcomplex u2[NZ][NY][NX];
//    static dcomplex pad3[3];
  static int indexmap[MAXDIM][MAXDIM][MAXDIM];

  int iter;
  double total_time, mflops;
  boolean verified;
  char class;

//    pad1[0]=pad2[0]=pad3[0];

    /*--------------------------------------------------------------------
      c Run the entire problem once to make sure all data is touched. 
      c This reduces variable startup costs, which is important for such a 
      c short benchmark. The other NPB 2 implementations are similar. 
      c-------------------------------------------------------------------*/
  if (!MYTHREAD)
    {
      for (i = 0; i < T_MAX; i++)
	{
	  timer_clear (i);
	}
    }
  setup ();

  // All the processors compute the indexmap (we need a private copy anyway)

  compute_indexmap (&indexmap[0][0][0], dims[2]);
  compute_initial_conditions (u1, dims[0]);
  fft_init (dims[0][0]);
  fft (1, u1, u0);
  /* end parallel */

  output (&u0[0][0][0], 0, 0, 0, "Go_Go_Go");

    /*--------------------------------------------------------------------
      c Start over from the beginning. Note that all operations must
      c be timed, in contrast to other benchmarks. 
      c-------------------------------------------------------------------*/
  if (!MYTHREAD)
    {
      for (i = 0; i < T_MAX; i++)
	{
	  timer_clear (i);
	}

      timer_start (T_TOTAL);
#if (TIMERS_ENABLED == TRUE)
      timer_start (T_SETUP);
#endif
    }
  upc_barrier;

  compute_indexmap (&indexmap[0][0][0], dims[2]);

  compute_initial_conditions (u1, dims[0]);

  fft_init (dims[0][0]);

#if (TIMERS_ENABLED == TRUE)
  upc_barrier;			// XXX Fortran calls synchup()

  if (!MYTHREAD)
    {
      timer_stop (T_SETUP);
      timer_start (T_FFT);
    }
#endif

  fft (1, u1, u0);

#if (TIMERS_ENABLED == TRUE)
  if (!MYTHREAD)
    timer_stop (T_FFT);
#endif

  for (iter = 1; iter <= niter; iter++)
    {
#if (TIMERS_ENABLED == TRUE)
      if (!MYTHREAD)
	timer_start (T_EVOLVE);
#endif

      evolve (&u0[0][0][0], &u1[0][0][0], iter, &indexmap[0][0][0], dims[0]);

#if (TIMERS_ENABLED == TRUE)
      if (!MYTHREAD)
	{
	  timer_stop (T_EVOLVE);
	  timer_start (T_FFT);
	}
#endif

      fft (-1, u1, u2);

#if (TIMERS_ENABLED == TRUE)
      if (!MYTHREAD)
	timer_stop (T_FFT);
      upc_barrier;
      if (!MYTHREAD)
	timer_start (T_CHECKSUM);
#endif

      checksum (iter, u2, dims[0]);

#if (TIMERS_ENABLED == TRUE)
      if (!MYTHREAD)
	timer_stop (T_CHECKSUM);
#endif
    }

  if (!MYTHREAD)
    verify (NX, NY, NZ, niter, &verified, &class);

  /* end parallel */

  if (!MYTHREAD)
    {
      timer_stop (T_TOTAL);
      total_time = timer_read (T_TOTAL);

      if (total_time != 0.0)
	{
	  mflops = 1.0e-6 * (double) (NTOTAL) *
	    (14.8157 + 7.19641 * log ((double) (NTOTAL))
	     + (5.23518 + 7.21113 * log ((double) (NTOTAL))) * niter)
	    / total_time;
	}
      else
	{
	  mflops = 0.0;
	}

      c_print_results ("FT", class, NX, NY, NZ, niter, THREADS,
		   total_time, mflops, "          floating point", verified,
		       NPBVERSION, COMPILETIME,
		       NPB_CS1, NPB_CS2, NPB_CS3, NPB_CS4, NPB_CS5, NPB_CS6, NPB_CS7);
#if (TIMERS_ENABLED == TRUE)
      print_timers ();
#endif
    }

  return 0;
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
evolve (dcomplex * u0, dcomplex * u1,
	int t, int *indexmap, int d[3])
{

    /*--------------------------------------------------------------------
      c-------------------------------------------------------------------*/

    /*--------------------------------------------------------------------
      c evolve u0 -> u1 (t time steps) in fourier space
      c-------------------------------------------------------------------*/

  int i, j, k;
  char name[100];

  sprintf (name, "before_evolving_%d_%d_%d", d[0], d[1], d[2]);

  output (u0, d[0], d[1], d[2], name);

#define imap(z,y,x) indexmap[x*d[1]*d[0]+y*d[0]+z]

  for (k = 0; k < d[2]; k++)
    {
      for (j = 0; j < d[1]; j++)
	{
	  for (i = 0; i < d[0]; i++)
	    {
	      crmul (u1[k * d[1] * d[0] + j * d[0] + i],
		     u0[k * d[1] * d[0] + j * d[0] + i],
		     ex[t * imap (i, j, k)]);
	    }
	}
    }

#undef imap

  output (u1, d[0], d[1], d[2],
	  "after_evolving");
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
compute_initial_conditions (dcomplex u0[NZ][NY][NX], int d[3])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c Fill in array u0 with initial conditions from 
c random number generator 
c-------------------------------------------------------------------*/

  int k;
  double x0, start, an, dummy;
  static double tmp[NX * 2 * MAXDIM + 1];
  int i, j, t;

  start = SEED;
/*--------------------------------------------------------------------
c Jump to the starting element for our first plane.
c-------------------------------------------------------------------*/
  ipow46 (A, (zstart[0] - 1) * 2 * NX * NY + (ystart[0] - 1) * 2 * NX, &an);
  dummy = randlc (&start, an);
  ipow46 (A, 2 * NX * NY, &an);

/*--------------------------------------------------------------------
c Go through by z planes filling in one square at a time.
c-------------------------------------------------------------------*/
  for (k = 0; k < dims[0][2]; k++)
    {
      x0 = start;
      vranlc (2 * NX * dims[0][1], &x0, A, tmp);

      t = 1;
      for (j = 0; j < dims[0][1]; j++)
	for (i = 0; i < NX; i++)
	  {
	    u0[k][j][i].real = tmp[t++];
	    u0[k][j][i].imag = tmp[t++];
	  }

      if (k != dims[0][2])
	dummy = randlc (&start, an);
    }
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
ipow46 (double a, int exponent, double *result)
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c compute a^exponent mod 2^46
c-------------------------------------------------------------------*/

  double dummy, q, r;
  int n, n2;

/*--------------------------------------------------------------------
c Use
c   a^n = a^(n/2)*a^(n/2) if n even else
c   a^n = a*a^(n-1)       if n odd
c-------------------------------------------------------------------*/
  *result = 1;
  if (exponent == 0)
    return;
  q = a;
  r = 1;
  n = exponent;

  while (n > 1)
    {
      n2 = n / 2;
      if (n2 * 2 == n)
	{
	  dummy = randlc (&q, q);
	  n = n2;
	}
      else
	{
	  dummy = randlc (&r, q);
	  n = n - 1;
	}
    }
  dummy = randlc (&r, q);
  *result = r;
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
setup (void)
{

    /*--------------------------------------------------------------------
      c-------------------------------------------------------------------*/

  int i;

  printf ("\n\n NAS Parallel Benchmarks 2.3 UPC C version"
	  " - FT Benchmark\n\n");

  niter = NITER_DEFAULT;

  /* 1004 format(' Number of processes :     ', i7)
     1005 format(' Processor array     :     ', i3, 'x', i3)
     1006 format(' WARNING: compiled for ', i5, ' processes. ',
     >       ' Will not verify. ') */

  // Determine layout type
  if (THREADS == 1)
    {
      np1 = 1;
      np2 = 1;
      layout_type = 0;
    }
  else if (THREADS < NZ)
    {
      np1 = 1;
      np2 = THREADS;
      layout_type = 1;
    }
  else
    {
      np1 = NZ;
      np2 = NZ / THREADS;
      layout_type = 2;
    }

  switch (layout_type)
    {
    case 0:
      for (i = 0; i < 3; i++)
	{
	  dims[i][0] = NX;
	  dims[i][1] = NY;
	  dims[i][2] = NZ;
	}
      break;
    case 1:
      dims[0][0] = NX;
      dims[0][1] = NY;
      dims[0][2] = NZ;

      dims[1][0] = NX;
      dims[1][1] = NY;
      dims[1][2] = NZ;

      dims[2][0] = NZ;
      dims[2][1] = NX;
      dims[2][2] = NY;
      break;
    case 2:
      dims[0][0] = NX;
      dims[0][1] = NY;
      dims[0][2] = NZ;

      dims[1][0] = NY;
      dims[1][1] = NX;
      dims[1][2] = NZ;

      dims[2][0] = NZ;
      dims[2][1] = NX;
      dims[2][2] = NY;
      break;
    }

  // Processor coordinates
  ME1 = MYTHREAD / np2;
  ME2 = MYTHREAD % np2;

  upc_barrier;			// MPI_Comm_split

  for (i = 0; i < 3; i++)
    {
      dims[i][1] = dims[i][1] / np1;
      dims[i][2] = dims[i][2] / np2;
    }

  switch (layout_type)
    {
    case 0:
      for (i = 0; i < 3; i++)
	{
	  xstart[i] = 1;
	  xend[i] = NX;
	  ystart[i] = 1;
	  yend[i] = NY;
	  zstart[i] = 1;
	  zend[i] = NZ;
	}
      break;
    case 1:
      xstart[0] = 1;
      xend[0] = NX;
      ystart[0] = 1;
      yend[0] = NY;
      zstart[0] = 1 + ME2 * NZ / np2;
      zend[0] = (ME2 + 1) * NZ / np2;

      xstart[1] = 1;
      xend[1] = NX;
      ystart[1] = 1;
      yend[1] = NY;
      zstart[1] = 1 + ME2 * NZ / np2;
      zend[1] = (ME2 + 1) * NZ / np2;

      xstart[2] = 1;
      xend[2] = NX;
      ystart[2] = 1 + ME2 * NY / np2;
      yend[2] = (ME2 + 1) * NY / np2;
      zstart[2] = 1;
      zend[2] = NZ;
      break;

    case 2:
      xstart[0] = 1;
      xend[0] = NX;
      ystart[0] = 1 + ME1 * NY / np1;
      yend[0] = (ME1 + 1) * NY / np1;
      zstart[0] = 1 + ME2 * NZ / np2;
      zend[0] = (ME2 + 1) * NZ / np2;

      xstart[1] = 1 + ME1 * NX / np1;
      xend[1] = (ME1 + 1) * NX / np1;
      ystart[1] = 1;
      yend[1] = NY;
      zstart[1] = zstart[0];
      zend[1] = zend[0];

      xstart[2] = xstart[1];
      xend[2] = xend[1];
      ystart[2] = 1 + ME2 * NY / np2;
      yend[2] = (ME2 + 1) * NY / np2;
      zstart[2] = 1;
      zend[2] = NZ;
      break;
    };



    /*--------------------------------------------------------------------
      c Set up info for blocking of ffts and transposes.  This improves
      c performance on cache-based systems. Blocking involves
      c working on a chunk of the problem at a time, taking chunks
      c along the first, second, or third dimension. 
      c
      c - In cffts1 blocking is on 2nd dimension (with fft on 1st dim)
      c - In cffts2/3 blocking is on 1st dimension (with fft on 2nd and 3rd dims)

      c Since 1st dim is always in processor, we'll assume it's long enough 
      c (default blocking factor is 16 so min size for 1st dim is 16)
      c The only case we have to worry about is cffts1 in a 2d decomposition. 
      c so the blocking factor should not be larger than the 2nd dimension. 
      c-------------------------------------------------------------------*/

  fftblock = FFTBLOCK_DEFAULT;
  fftblockpad = FFTBLOCKPAD_DEFAULT;

  if (layout_type == 2)
    {
      if (dims[1][0] < fftblock)
	fftblock = dims[1][0];
      if (dims[1][1] < fftblock)
	fftblock = dims[1][1];
      if (dims[1][2] < fftblock)
	fftblock = dims[1][2];
    }

  if (fftblock != FFTBLOCK_DEFAULT)
    fftblockpad = fftblock + 3;

  printf (" Size                : %3dx%3dx%3d\n", NX, NY, NZ);
  printf (" Iterations          :     %7d\n", niter);
  printf (" Layout              : %d (%d,%d)\n", layout_type, np1, np2);
  printf (" dims                : (%d,%d,%d)(%d,%d,%d)(%d,%d,%d)\n",
	  dims[0][0], dims[0][1], dims[0][2],
	  dims[1][0], dims[1][1], dims[1][2],
	  dims[2][0], dims[2][1], dims[2][2]);
  for (i = 0; i < 3; i++)
    printf (" start,end %d : x= %d,%d y= %d,%d z= %d,%d\n", i,
	    xstart[i], xend[i],
	    ystart[i], yend[i],
	    zstart[i], zend[i]);

  /* BUG!!!!! */
  reshuffle_arr_shd[MYTHREAD] = (shared[] dcomplex*)&big_reshuffle_arr[MYTHREAD].arr;
//    (shared[]dcomplex *) upc_local_alloc (NX * NY * NZ, sizeof (dcomplex));
//  assert (reshuffle_arr_shd[MYTHREAD] != NULL);

  upc_barrier 1;
  // Broadcast the shared pointers
  for (i = 0; i < THREADS; i++)
    reshuffle_arr[i] = reshuffle_arr_shd[i];

  upc_barrier;
  if (!MYTHREAD)
    printf ("setup(): returning\n");
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
compute_indexmap (int *indexmap, int d[3])
{

    /*--------------------------------------------------------------------
      c-------------------------------------------------------------------*/

    /*--------------------------------------------------------------------
      c compute function from local (i,j,k) to ibar^2+jbar^2+kbar^2 
      c for time evolution exponent. 
      c-------------------------------------------------------------------*/

  int i, j, k, ii, ii2, jj, ij2, kk;
  double ap;

    /*--------------------------------------------------------------------
      c basically we want to convert the fortran indices 
      c   1 2 3 4 5 6 7 8 
      c to 
      c   0 1 2 3 -4 -3 -2 -1
      c The following magic formula does the trick:
      c mod(i-1+n/2, n) - n/2
      c-------------------------------------------------------------------*/


  switch (layout_type)
    {
    case 0:
      for (i = 0; i < dims[2][0]; i++)
	{
	  ii = (i + 1 + xstart[2] - 2 + NX / 2) % NX - NX / 2;
	  ii2 = ii * ii;
	  for (j = 0; j < dims[2][1]; j++)
	    {
	      jj = (j + 1 + ystart[2] - 2 + NY / 2) % NY - NY / 2;
	      ij2 = jj * jj + ii2;
	      for (k = 0; k < dims[2][2]; k++)
		{
		  kk = (k + 1 + zstart[2] - 2 + NZ / 2) % NZ - NZ / 2;
		  indexmap[k * d[1] * d[0] + j * d[0] + i] = kk * kk + ij2;
		}
	    }
	}
      break;
    case 1:
      /*      printf("start x=%d y=%d z=%d\n"
         "NX=%d NY=%d NZ=%d\n"
         "dims[2][0]=%d [1]=%d [2]=%d\n",
         xstart[2], ystart[2], zstart[2], 
         NX, NY, NZ, 
         dims[2][0], dims[2][1], dims[2][2]);
       */
      for (i = 0; i < dims[2][1]; i++)
	{
	  ii = (i + 1 + xstart[2] - 2 + NX / 2) % NX - NX / 2;
	  ii2 = ii * ii;
	  for (j = 0; j < dims[2][2]; j++)
	    {
	      jj = (j + 1 + ystart[2] - 2 + NY / 2) % NY - NY / 2;
	      ij2 = jj * jj + ii2;
	      for (k = 0; k < dims[2][0]; k++)
		{
		  kk = (k + 1 + zstart[2] - 2 + NZ / 2) % NZ - NZ / 2;

		  indexmap[j * d[1] * d[0] + i * d[0] + k] = kk * kk + ij2;
		}
	    }
	}
      break;
    case 2:
      for (i = 0; i < dims[2][1]; i++)
	{
	  ii = (i + 1 + xstart[2] - 2 + NX / 2) % NX - NX / 2;
	  ii2 = ii * ii;
	  for (j = 0; j < dims[2][2]; j++)
	    {
	      jj = (j + 1 + ystart[2] - 2 + NY / 2) % NY - NY / 2;
	      ij2 = jj * jj + ii2;
	      for (k = 0; k < dims[2][0]; k++)
		{
		  kk = (k + 1 + zstart[2] - 2 + NZ / 2) % NZ - NZ / 2;
		  indexmap[j * d[1] * d[0] + i * d[0] + k] = kk * kk + ij2;
		}
	    }
	}
      break;
    }

    /*--------------------------------------------------------------------
      c compute array of exponentials for time evolution. 
      c-------------------------------------------------------------------*/
  ap = -4.0 * ALPHA * PI * PI;

  ex[0] = 1.0;
  ex[1] = exp (ap);
  for (i = 2; i <= EXPMAX; i++)
    {
      ex[i] = ex[i - 1] * ex[1];
    }
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
print_timers (void)
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int i;
  static char *tstrings[] =
  {"          total ",
   "          setup ",
   "            fft ",
   "         evolve ",
   "       checksum ",
   "         fftlow ",
   "        fftcopy ",
   "      transpose "};

  for (i = 0; i < T_MAX; i++)
    {
      if (timer_read (i) != 0.0)
	{
	  printf ("timer %2d(%16s( :%10.6f\n", i, tstrings[i], timer_read (i));
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
fft (int dir, dcomplex x1[NZ][NY][NX], dcomplex x2[NZ][NY][NX])
{

    /*--------------------------------------------------------------------
      c-------------------------------------------------------------------*/

  dcomplex y0[NX][FFTBLOCKPAD];
  dcomplex y1[NX][FFTBLOCKPAD];

    /*--------------------------------------------------------------------
      c note: args x1, x2 must be different arrays
      c note: args for cfftsx are (direction, layout, xin, xout, scratch)
      c       xin/xout may be the same and it can be somewhat faster
      c       if they are
      c-------------------------------------------------------------------*/

  if (dir == 1)
    {

      switch (layout_type)
	{
	case 0:
	  cffts1 (1, dims[0], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  cffts2 (1, dims[1], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  cffts3 (1, dims[2], &x1[0][0][0], &x2[0][0][0], y0, y1);	/* x1 -> x2 */
	  break;
	case 1:
	  output (&x1[0][0][0], dims[0][0], dims[0][1], dims[0][2],
		  "fft1_1");
	  cffts1 (1, dims[0], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  output (&x1[0][0][0], dims[0][0], dims[0][1], dims[0][2],
		  "fft1_2");
	  cffts2 (1, dims[1], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  output (&x1[0][0][0], dims[1][0], dims[1][1], dims[1][2],
		  "fft1_3");

#if (TIMERS_ENABLED==TRUE)
	  if (!MYTHREAD)
	    timer_start (T_TRANSPOSE);
#endif
	  transpose_xy_z (1, 2, (dcomplex *) x1, (dcomplex *) x2);
#if (TIMERS_ENABLED==TRUE)
	  if (!MYTHREAD)
	    timer_stop (T_TRANSPOSE);
#endif

	  output (&x2[0][0][0], dims[2][0], dims[2][1], dims[2][2],
		  "fft1_4");
	  cffts1 (1, dims[2], &x2[0][0][0], &x2[0][0][0], y0, y1);	/* x2 -> x2 */
	  output (&x2[0][0][0], dims[2][0], dims[2][1], dims[2][2],
		  "fft1_5");
	  break;

	  /*      
	     case 2:
	     cffts1(1, dims[0], &x1[0][0][0], &x1[0][0][0], y0, y1);  
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_start(T_TRANSPOSE);
	     transpose_x_y(0, 1, (dcomplex*)x1, (dcomplex*)x2);
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_stop(T_TRANSPOSE);
	     cffts1(1, dims[1], &x2[0][0][0], &x2[0][0][0], y0, y1);  
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_start(T_TRANSPOSE);
	     transpose_x_z(1, 2, (dcomplex*)x2, (dcomplex*)x1);
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_stop(T_TRANSPOSE);
	     cffts1(1, dims[2], &x1[0][0][0], &x2[0][0][0], y0, y1);  
	     break;
	   */
	}
    }
  else
    {
      switch (layout_type)
	{
	case 0:
	  cffts3 (-1, dims[2], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  cffts2 (-1, dims[1], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  cffts1 (-1, dims[0], &x1[0][0][0], &x2[0][0][0], y0, y1);	/* x1 -> x2 */
	  break;
	case 1:
	  output (&x1[0][0][0], dims[2][0], dims[2][1], dims[2][2],
		  "fft_1");
	  cffts1 (-1, dims[2], &x1[0][0][0], &x1[0][0][0], y0, y1);	/* x1 -> x1 */
	  output (&x1[0][0][0], dims[2][0], dims[2][1], dims[2][2],
		  "fft_2");
#if (TIMERS_ENABLED==TRUE)
	  if (!MYTHREAD)
	    timer_start (T_TRANSPOSE);
#endif
	  transpose_x_yz (2, 1, (dcomplex *) x1, (dcomplex *) x2);
#if (TIMERS_ENABLED==TRUE)
	  if (!MYTHREAD)
	    timer_stop (T_TRANSPOSE);
#endif
	  output (&x2[0][0][0], dims[1][0], dims[1][1], dims[1][2],
		  "fft_3");
	  cffts2 (-1, dims[1], &x2[0][0][0], &x2[0][0][0], y0, y1);	/* x2 -> x2 */
	  output (&x2[0][0][0], dims[1][0], dims[1][1], dims[1][2],
		  "fft_4");
	  cffts1 (-1, dims[0], &x2[0][0][0], &x2[0][0][0], y0, y1);	/* x2 -> x2 */
	  output (&x2[0][0][0], dims[0][0], dims[0][1], dims[0][2],
		  "fft_5");
	  break;
	  /*
	     case 2:
	     cffts1(-1, dims[2], &x1[0][0][0], &x1[0][0][0], y0, y1); 
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_start(T_TRANSPOSE);
	     transpose_x_z(2, 1, (dcomplex*)x1, (dcomplex*)x2);

	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_stop(T_TRANSPOSE);
	     cffts1(-1, dims[1], &x2[0][0][0], &x2[0][0][0], y0, y1); 
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_start(T_TRANSPOSE);
	     transpose_x_y(1, 0, (dcomplex*)x2, (dcomplex*)x1);
	     if (!MYTHREAD) 
	     if (TIMERS_ENABLED == TRUE) timer_stop(T_TRANSPOSE);
	     cffts1(-1, dims[0], &x1[0][0][0], &x2[0][0][0], y0, y1); 
	     break;
	   */
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
cffts1 (int is, int d[3], dcomplex * x,
	dcomplex * xout,
	dcomplex y0[NX][FFTBLOCKPAD],
	dcomplex y1[NX][FFTBLOCKPAD])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int logd[3];
  int i, j, k, jj;
  char name[100];

  for (i = 0; i < 3; i++)
    {
      logd[i] = ilog2 (d[i]);
    }

  for (k = 0; k < d[2]; k++)
    {
      for (jj = 0; jj <= d[1] - fftblock; jj += fftblock)
	{
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (j = 0; j < fftblock; j++)
	    {
	      for (i = 0; i < d[0]; i++)
		{
		  y0[i][j].real = x[k * d[1] * d[0] + (j + jj) * d[0] + i].real;
		  y0[i][j].imag = x[k * d[1] * d[0] + (j + jj) * d[0] + i].imag;
		}
	    }

	  // DEBUG !
	  for (j = fftblock; j < fftblockpad; j++)
	    {
	      for (i = 0; i < d[0]; i++)
		{
		  y0[i][j].real = 0;
		  y0[i][j].imag = 0;
		}
	    }

/*          if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */

/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTLOW); */
	  sprintf (name, "bef_cfftz_%d_%d_y0", logd[0], d[0]);

	  //      output(&y0[0][0], 1, NX, FFTBLOCKPAD, name);

	  cfftz (is, logd[0], d[0], y0, y1);

	  sprintf (name, "aft_cfftz_%d_%d_y0", logd[0], d[0]);
	  //      output(&y0[0][0], 1, NX, FFTBLOCKPAD, name);

/*          if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTLOW); */
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (j = 0; j < fftblock; j++)
	    {
	      for (i = 0; i < d[0]; i++)
		{
		  xout[k * d[1] * d[0] + (j + jj) * d[0] + i].real = y0[i][j].real;
		  xout[k * d[1] * d[0] + (j + jj) * d[0] + i].imag = y0[i][j].imag;
		}
	    }
/*          if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
cffts2 (int is, int d[3], dcomplex * x,
	dcomplex * xout,
	dcomplex y0[NX][FFTBLOCKPAD],
	dcomplex y1[NX][FFTBLOCKPAD])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int logd[3];
  int i, j, k, ii;

  for (i = 0; i < 3; i++)
    {
      logd[i] = ilog2 (d[i]);
    }
  for (k = 0; k < d[2]; k++)
    {
      for (ii = 0; ii <= d[0] - fftblock; ii += fftblock)
	{
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (j = 0; j < d[1]; j++)
	    {
	      for (i = 0; i < fftblock; i++)
		{
		  y0[j][i].real = x[k * d[1] * d[0] + j * d[0] + i + ii].real;
		  y0[j][i].imag = x[k * d[1] * d[0] + j * d[0] + i + ii].imag;
		}
	    }
/*          if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTLOW); */
	  cfftz (is, logd[1],
		 d[1], y0, y1);

/*          if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTLOW); */
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (j = 0; j < d[1]; j++)
	    {
	      for (i = 0; i < fftblock; i++)
		{
		  xout[k * d[1] * d[0] + j * d[0] + i + ii].real = y0[j][i].real;
		  xout[k * d[1] * d[0] + j * d[0] + i + ii].imag = y0[j][i].imag;
		}
	    }
/*           if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */
	}
    }
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
cffts3 (int is, int d[3], dcomplex * x,
	dcomplex * xout,
	dcomplex y0[NX][FFTBLOCKPAD],
	dcomplex y1[NX][FFTBLOCKPAD])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int logd[3];
  int i, j, k, ii;

  for (i = 0; i < 3; i++)
    {
      logd[i] = ilog2 (d[i]);
    }
  for (j = 0; j < d[1]; j++)
    {
      for (ii = 0; ii <= d[0] - fftblock; ii += fftblock)
	{
/*          if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (k = 0; k < d[2]; k++)
	    {
	      for (i = 0; i < fftblock; i++)
		{
		  y0[k][i].real = x[k * d[1] * d[0] + j * d[0] + i + ii].real;
		  y0[k][i].imag = x[k * d[1] * d[0] + j * d[0] + i + ii].imag;
		}
	    }

/*           if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */
/*           if (TIMERS_ENABLED == TRUE) timer_start(T_FFTLOW); */
	  cfftz (is, logd[2],
		 d[2], y0, y1);
/*           if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTLOW); */
/*           if (TIMERS_ENABLED == TRUE) timer_start(T_FFTCOPY); */
	  for (k = 0; k < d[2]; k++)
	    {
	      for (i = 0; i < fftblock; i++)
		{
		  xout[k * d[1] * d[0] + j * d[0] + i + ii].real = y0[k][i].real;
		  xout[k * d[1] * d[0] + j * d[0] + i + ii].imag = y0[k][i].imag;
		}
	    }
/*           if (TIMERS_ENABLED == TRUE) timer_stop(T_FFTCOPY); */
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
fft_init (int n)
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c compute the roots-of-unity array that will be used for subsequent FFTs. 
c-------------------------------------------------------------------*/

  int m, ku, i, j, ln;
  double t, ti;


/*--------------------------------------------------------------------
c   Initialize the U array with sines and cosines in a manner that permits
c   stride one access at each FFT iteration.
c-------------------------------------------------------------------*/
  m = ilog2 (n);
  u[0].real = (double) m;
  u[0].imag = 0.0;
  ku = 1;
  ln = 1;

  for (j = 1; j <= m; j++)
    {
      t = PI / ln;

      for (i = 0; i <= ln - 1; i++)
	{
	  ti = i * t;
	  u[i + ku].real = cos (ti);
	  u[i + ku].imag = sin (ti);
	}

      ku = ku + ln;
      ln = 2 * ln;
    }

  // Initialize the lock

  sum_write = upc_global_lock_alloc();
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
cfftz (int is, int m, int n, dcomplex x[NX][FFTBLOCKPAD],
       dcomplex y[NX][FFTBLOCKPAD])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c   Computes NY N-point complex-to-complex FFTs of X using an algorithm due
c   to Swarztrauber.  X is both the input and the output array, while Y is a 
c   scratch array.  It is assumed that N = 2^M.  Before calling CFFTZ to 
c   perform FFTs, the array U must be initialized by calling CFFTZ with IS 
c   set to 0 and M set to MX, where MX is the maximum value of M for any 
c   subsequent call.
c-------------------------------------------------------------------*/

  int i, j, l, mx;

/*--------------------------------------------------------------------
c   Check if input parameters are invalid.
c-------------------------------------------------------------------*/
  mx = (int) (u[0].real);
  if ((is != 1 && is != -1) || m < 1 || m > mx)
    {
      printf ("CFFTZ: Either U has not been initialized, or else\n"
	      "one of the input parameters is invalid%5d%5d%5d\n",
	      is, m, mx);
      exit (1);
    }

/*--------------------------------------------------------------------
c   Perform one variant of the Stockham FFT.
c-------------------------------------------------------------------*/
  for (l = 1; l <= m; l += 2)
    {
      fftz2 (is, l, m, n, fftblock, fftblockpad, u, x, y);
      if (l == m)
	break;
      fftz2 (is, l + 1, m, n, fftblock, fftblockpad, u, y, x);
    }

/*--------------------------------------------------------------------
c   Copy Y to X.
c-------------------------------------------------------------------*/
  if (m % 2 == 1)
    {
      for (j = 0; j < n; j++)
	{
	  for (i = 0; i < fftblock; i++)
	    {
	      x[j][i].real = y[j][i].real;
	      x[j][i].imag = y[j][i].imag;
	    }
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
fftz2 (int is, int l, int m, int n, int ny, int ny1,
       dcomplex u[NX], dcomplex x[NX][FFTBLOCKPAD],
       dcomplex y[NX][FFTBLOCKPAD])
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c   Performs the L-th iteration of the second variant of the Stockham FFT.
c-------------------------------------------------------------------*/

  int k, n1, li, lj, lk, ku, i, j, i11, i12, i21, i22;
  dcomplex u1;

/*--------------------------------------------------------------------
c   Set initial parameters.
c-------------------------------------------------------------------*/

  n1 = n / 2;
  if (l - 1 == 0)
    {
      lk = 1;
    }
  else
    {
      lk = 2 << ((l - 1) - 1);
    }
  if (m - l == 0)
    {
      li = 1;
    }
  else
    {
      li = 2 << ((m - l) - 1);
    }
  lj = 2 * lk;
  ku = li;

  for (i = 0; i < li; i++)
    {

      i11 = i * lk;
      i12 = i11 + n1;
      i21 = i * lj;
      i22 = i21 + lk;
      if (is >= 1)
	{
	  u1.real = u[ku + i].real;
	  u1.imag = u[ku + i].imag;
	}
      else
	{
	  u1.real = u[ku + i].real;
	  u1.imag = -u[ku + i].imag;
	}

/*--------------------------------------------------------------------
c   This loop is vectorizable.
c-------------------------------------------------------------------*/
      for (k = 0; k < lk; k++)
	{
	  for (j = 0; j < ny; j++)
	    {
	      double x11real, x11imag;
	      double x21real, x21imag;
	      /* BUG !!!!!!!! */
	      x11real = x[i11 + k][j].real;
	      x11imag = x[i11 + k][j].imag;
	      x21real = x[i12 + k][j].real;
	      x21imag = x[i12 + k][j].imag;
	      y[i21 + k][j].real = x11real + x21real;
	      y[i21 + k][j].imag = x11imag + x21imag;
	      y[i22 + k][j].real = u1.real * (x11real - x21real)
		- u1.imag * (x11imag - x21imag);
	      y[i22 + k][j].imag = u1.real * (x11imag - x21imag)
		+ u1.imag * (x11real - x21real);
	    }
	}
    }
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static int 
ilog2 (int n)
{
/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int nn, lg;

  if (n == 1)
    {
      return 0;
    }
  lg = 1;
  nn = 2;
  while (nn < n)
    {
      nn = nn << 1;
      lg++;
    }

  return lg;
}

void 
transpose_x_yz (int l1, int l2,
		dcomplex * xin, dcomplex * xout)
{
  output (xin, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xin_transpose_x_yz");

  transpose2_local (dims[l1][0], dims[l1][1] * dims[l1][2],
		    xin, (dcomplex *) reshuffle_arr[MYTHREAD]);

  output ((dcomplex *) reshuffle_arr[MYTHREAD], dims[l1][0], dims[l1][1], dims[l1][2],
	  "xout_transpose_x_yz");

  transpose2_global (reshuffle_arr, xin);

  output (xin, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xin_global_transpose_x_yz");

  transpose2_finish (dims[l1][0], dims[l1][1] * dims[l1][2],
		     xin, xout);

  output (xout, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xout_finish_transpose_x_yz");
}

void 
transpose_xy_z (int l1, int l2,
		dcomplex * xin, dcomplex * xout)
{
#ifdef DEBUG
  if (!MYTHREAD)
    printf ("transpose_xy_z(): entering\n");
#endif
  output (xin, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xin_transpose_xy_z");

  transpose2_local (dims[l1][0] * dims[l1][1], dims[l1][2],
		    xin, (dcomplex *) reshuffle_arr[MYTHREAD]);

  output ((dcomplex *) reshuffle_arr[MYTHREAD], dims[l1][0], dims[l1][1], dims[l1][2],
	  "xout_transpose_xy_z");

  transpose2_global (reshuffle_arr, xin);

  output (xin, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xin_global_transpose_xy_z");

  transpose2_finish (dims[l1][0] * dims[l1][1], dims[l1][2],
		     xin, xout);

  output (xout, dims[l1][0], dims[l1][1], dims[l1][2],
	  "xout_finish_transpose_xy_z");
}

void 
transpose2_local (int n1, int n2,
		  dcomplex xin[], dcomplex xout[])
{
//    dcomplex z[TRANSBLOCKPAD][TRANSBLOCK];

  int i, j;			//,j, ii, jj;

#if 0
  if ((n1 < TRANSBLOCK) || (n2 < TRANSBLOCK))
    {
#endif

      if (n1 >= n2)		// XXX was (n1>n2) but Fortran says (n1 .ge. n2)

	for (j = 0; j < n2; j++)
	  for (i = 0; i < n1; i++)
	    //xout[n1][n2]   xin[n2][n1]
	    //xout[i] [j]    xin[j] [i]
	    xout[i * n2 + j] = xin[j * n1 + i];
      else
	for (i = 0; i < n1; i++)
	  for (j = 0; j < n2; j++)
	    xout[i * n2 + j] = xin[j * n1 + i];
#if 0
    }
  else
    {

      for (j = 0; j < n2 - 1; j += TRANSBLOCK)
	for (i = 0; i < n1 - 1; i += TRANSBLOCK)
	  {
	    for (jj = 0; jj < TRANSBLOCK; jj++)
	      for (ii = 0; ii < TRANSBLOCK; ii++)
		z[ii][jj] = xin[(j + jj) * n1 + i + ii];
	    for (ii = 0; ii < TRANSBLOCK; ii++)
	      for (jj = 0; jj < TRANSBLOCK; jj++)
		xout[(i + ii) * n2 + j + jj] = z[ii][jj];
	  }
    }
#endif
}

#if 0
typedef struct
{
  dcomplex a[NX * NY * NZ / (THREADS * THREADS)];
}
chunk_copy;

void 
transpose2_global (shared[]dcomplex * xin[THREADS], dcomplex * xout)
{
  int i;
  int chunk = NX * NY * NZ / (THREADS * THREADS);
  shared chunk_copy *pxin;
  chunk_copy *pxout;

  pxout = (chunk_copy *) xout;

  barrier 11;

  for (i = 0; i < THREADS; i++)
    {
      pxin = (shared chunk_copy *) & xin[i][chunk * MYTHREAD];
      *pxout = *pxin;
      pxout++;
    }
}
#endif

void 
transpose2_global (shared[]dcomplex * xin[THREADS], dcomplex * xout)
{
  int i, j;
  int chunk = NX * NY * NZ / (THREADS * THREADS);
  shared [] dcomplex* tmp;
  
  upc_barrier;
  // XXX Fortran version uses an MPI_alltoall() here
  for (i = 0; i < THREADS; i++)
    {
      tmp = xin[i];
      /* BUG!!!!! xin[i][j] won't work */
      upc_memget (&xout[chunk * i], &tmp[chunk * MYTHREAD], chunk * sizeof (*xout));
    }
  upc_barrier;			// XXX MPI_alltoall() is collective so we have to sync

}

void 
transpose2_finish (int n1, int n2,
		   dcomplex xin[], dcomplex xout[])
{
  int p, i, j;

  /* Data layout from Fortran:
   *   xin [np2][n1/np2][n2]
   *   xin  [p]   [j]   [i]
   *   xout[n1/np2][n2*np2]
   *   xout [j]       [i]
   */

  for (p = 0; p < np2; p++)
    for (j = 0; j < n1 / np2; j++)
      for (i = 0; i < n2; i++)
	xout[j * (n2 * np2) + p * n2 + i] = xin[p * n2 * (n1 / np2) + j * n2 + i];
}

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/
//shared dcomplex _dbg_sum;

static void 
checksum (int i, dcomplex u1[NZ][NY][NX], int d[3])

{

    /*--------------------------------------------------------------------
      c-------------------------------------------------------------------*/

  int j, q, r, s;
  dcomplex chk, allchk;
  shared[] double *sum_r, *sum_i;
  //#ifdef DEBUG
  shared   dcomplex *dbg_sum;
  //#endif

  chk.real = 0.0;
  chk.imag = 0.0;

  // Work on the local data

  for (j = 1; j <= 1024; j++)
    {
      q = (j % NX) + 1;
      if (q >= xstart[0] && q <= xend[0])
	{
	  r = ((3 * j) % NY) + 1;
	  if (r >= ystart[0] && r <= yend[0])
	    {
	      s = ((5 * j) % NZ) + 1;
	      if (s >= zstart[0] && s <= zend[0])
		{
		  cadd (chk, chk, u1[s - zstart[0]][r - ystart[0]][q - xstart[0]]);
		}
	    }
	}
    }

#ifdef WITH_LIBCOLLOP
  /* upc_all_reduce! */
  if(!MYTHREAD) printf("sums[i]=(%f,%f)\n", sums[i].real, sums[i].imag);
  sum_r = upc_all_reduce_double (&chk.real, 1, UPC_ADD,
				 NULL, upc_threadof (&sums[i]));
#ifdef DEBUG
  printf("-reduce1 done-\n");
#endif
  sum_i = upc_all_reduce_double (&chk.imag, 1, UPC_ADD,
				 NULL, upc_threadof (&sums[i]));
#ifdef DEBUG
  printf("-reduce2 done-\n");
#endif
#endif

//#ifdef DEBUG
  if (!MYTHREAD)
    printf ("all_reduce: done.\n");

  dbg_sum = (shared dcomplex*)upc_all_alloc(1, sizeof(dcomplex));
  //assert(dbg_sum != NULL);
  dbg_sum->real = dbg_sum->imag = 0;
  upc_barrier;
  
  upc_lock (sum_write);
  {
    double tmp = dbg_sum->real;
    tmp += chk.real;
    dbg_sum->real = tmp;
    //dbg_sum->real += chk.real;
    //dbg_sum->imag += chk.imag;
    tmp = dbg_sum->imag;
    tmp += chk.imag;
    dbg_sum->imag = tmp;
  }
  upc_unlock (sum_write);
  upc_barrier 10;
  if (!MYTHREAD)
    printf ("end of sequential sum\n");
//#endif

  if (MYTHREAD == upc_threadof (&sums[i]))
    {				/* complex % real */
      dcomplex* mysum;
      mysum = (dcomplex*)&sums[i];

#ifdef DEBUG
      /* Makes sure that we get the same result with upc_all_reduce() */
//      if (abs (*sum_r - dbg_sum->real) > 1e-5)
//	fprintf (stdout, "all_reduce: r %f instead of %f\n", *sum_r, dbg_sum->real);
//      if (abs (*sum_i - dbg_sum->imag) > 1e-5)
//	fprintf (stdout, "all_reduce: i %f instead of %f\n", *sum_i, dbg_sum->imag);
//      upc_free(dbg_sum);
#endif
//      mysum->real += *((double *) sum_r);
//      mysum->imag += *((double *) sum_i);
      mysum->real += dbg_sum->real;
      mysum->imag += dbg_sum->imag;
      upc_free(dbg_sum);

      mysum->real = mysum->real / (double) (NTOTAL);
      mysum->imag = mysum->imag / (double) (NTOTAL);

      printf ("T = %5d     Checksum = %22.12e %22.12e\n",
	      i, mysum->real, mysum->imag);

//      upc_free (sum_r);
//      upc_free (sum_i);
    }
  upc_barrier;
}


/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

static void 
verify (int d1, int d2, int d3, int nt,
	boolean * verified, char *class)
{

/*--------------------------------------------------------------------
c-------------------------------------------------------------------*/

  int size, i;
  double err, epsilon;

/*--------------------------------------------------------------------
c   Sample size reference checksums
c-------------------------------------------------------------------*/

/*--------------------------------------------------------------------
c   Class S size reference checksums
c-------------------------------------------------------------------*/
  double vdata_real_s[6 + 1] =
  {0.0,
   5.546087004964e+02,
   5.546385409189e+02,
   5.546148406171e+02,
   5.545423607415e+02,
   5.544255039624e+02,
   5.542683411902e+02};
  double vdata_imag_s[6 + 1] =
  {0.0,
   4.845363331978e+02,
   4.865304269511e+02,
   4.883910722336e+02,
   4.901273169046e+02,
   4.917475857993e+02,
   4.932597244941e+02};
/*--------------------------------------------------------------------
c   Class W size reference checksums
c-------------------------------------------------------------------*/
  double vdata_real_w[6 + 1] =
  {0.0,
   5.673612178944e+02,
   5.631436885271e+02,
   5.594024089970e+02,
   5.560698047020e+02,
   5.530898991250e+02,
   5.504159734538e+02};
  double vdata_imag_w[6 + 1] =
  {0.0,
   5.293246849175e+02,
   5.282149986629e+02,
   5.270996558037e+02,
   5.260027904925e+02,
   5.249400845633e+02,
   5.239212247086e+02};
/*--------------------------------------------------------------------
c   Class A size reference checksums
c-------------------------------------------------------------------*/
  double vdata_real_a[6 + 1] =
  {0.0,
   5.046735008193e+02,
   5.059412319734e+02,
   5.069376896287e+02,
   5.077892868474e+02,
   5.085233095391e+02,
   5.091487099959e+02};
  double vdata_imag_a[6 + 1] =
  {0.0,
   5.114047905510e+02,
   5.098809666433e+02,
   5.098144042213e+02,
   5.101336130759e+02,
   5.104914655194e+02,
   5.107917842803e+02};
/*--------------------------------------------------------------------
c   Class B size reference checksums
c-------------------------------------------------------------------*/
  double vdata_real_b[20 + 1] =
  {0.0,
   5.177643571579e+02,
   5.154521291263e+02,
   5.146409228649e+02,
   5.142378756213e+02,
   5.139626667737e+02,
   5.137423460082e+02,
   5.135547056878e+02,
   5.133910925466e+02,
   5.132470705390e+02,
   5.131197729984e+02,
   5.130070319283e+02,
   5.129070537032e+02,
   5.128182883502e+02,
   5.127393733383e+02,
   5.126691062020e+02,
   5.126064276004e+02,
   5.125504076570e+02,
   5.125002331720e+02,
   5.124551951846e+02,
   5.124146770029e+02};
  double vdata_imag_b[20 + 1] =
  {0.0,
   5.077803458597e+02,
   5.088249431599e+02,
   5.096208912659e+02,
   5.101023387619e+02,
   5.103976610617e+02,
   5.105948019802e+02,
   5.107404165783e+02,
   5.108576573661e+02,
   5.109577278523e+02,
   5.110460304483e+02,
   5.111252433800e+02,
   5.111968077718e+02,
   5.112616233064e+02,
   5.113203605551e+02,
   5.113735928093e+02,
   5.114218460548e+02,
   5.114656139760e+02,
   5.115053595966e+02,
   5.115415130407e+02,
   5.115744692211e+02};
/*--------------------------------------------------------------------
c   Class C size reference checksums
c-------------------------------------------------------------------*/
  double vdata_real_c[20 + 1] =
  {0.0,
   5.195078707457e+02,
   5.155422171134e+02,
   5.144678022222e+02,
   5.140150594328e+02,
   5.137550426810e+02,
   5.135811056728e+02,
   5.134569343165e+02,
   5.133651975661e+02,
   5.132955192805e+02,
   5.132410471738e+02,
   5.131971141679e+02,
   5.131605205716e+02,
   5.131290734194e+02,
   5.131012720314e+02,
   5.130760908195e+02,
   5.130528295923e+02,
   5.130310107773e+02,
   5.130103090133e+02,
   5.129905029333e+02,
   5.129714421109e+02};
  double vdata_imag_c[20 + 1] =
  {0.0,
   5.149019699238e+02,
   5.127578201997e+02,
   5.122251847514e+02,
   5.121090289018e+02,
   5.121143685824e+02,
   5.121496764568e+02,
   5.121870921893e+02,
   5.122193250322e+02,
   5.122454735794e+02,
   5.122663649603e+02,
   5.122830879827e+02,
   5.122965869718e+02,
   5.123075927445e+02,
   5.123166486553e+02,
   5.123241541685e+02,
   5.123304037599e+02,
   5.123356167976e+02,
   5.123399592211e+02,
   5.123435588985e+02,
   5.123465164008e+02};

  epsilon = 1.0e-12;
  *verified = TRUE;
  *class = 'U';

  if (d1 == 64 &&
      d2 == 64 &&
      d3 == 64 &&
      nt == 6)
    {
      *class = 'S';
      for (i = 1; i <= nt; i++)
	{
	  err = (get_real (sums[i]) - vdata_real_s[i]) / vdata_real_s[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	  err = (get_imag (sums[i]) - vdata_imag_s[i]) / vdata_imag_s[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	}
    }
  else if (d1 == 128 &&
	   d2 == 128 &&
	   d3 == 32 &&
	   nt == 6)
    {
      *class = 'W';
      for (i = 1; i <= nt; i++)
	{
	  err = (get_real (sums[i]) - vdata_real_w[i]) / vdata_real_w[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	  err = (get_imag (sums[i]) - vdata_imag_w[i]) / vdata_imag_w[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	}
    }
  else if (d1 == 256 &&
	   d2 == 256 &&
	   d3 == 128 &&
	   nt == 6)
    {
      *class = 'A';
      for (i = 1; i <= nt; i++)
	{
	  err = (get_real (sums[i]) - vdata_real_a[i]) / vdata_real_a[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	  err = (get_imag (sums[i]) - vdata_imag_a[i]) / vdata_imag_a[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	}
    }
  else if (d1 == 512 &&
	   d2 == 256 &&
	   d3 == 256 &&
	   nt == 20)
    {
      *class = 'B';
      for (i = 1; i <= nt; i++)
	{
	  err = (get_real (sums[i]) - vdata_real_b[i]) / vdata_real_b[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	  err = (get_imag (sums[i]) - vdata_imag_b[i]) / vdata_imag_b[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	}
    }
  else if (d1 == 512 &&
	   d2 == 512 &&
	   d3 == 512 &&
	   nt == 20)
    {
      *class = 'C';
      for (i = 1; i <= nt; i++)
	{
	  err = (get_real (sums[i]) - vdata_real_c[i]) / vdata_real_c[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	  err = (get_imag (sums[i]) - vdata_imag_c[i]) / vdata_imag_c[i];
	  if (fabs (err) > epsilon)
	    {
	      *verified = FALSE;
	      break;
	    }
	}
    }

  if ((*class != 'U') && (*verified))
    {
      printf ("Result verification successful\n");
    }
  else
    {
      printf ("Result verification failed\n");
    }

  printf ("class = %1c\n", *class);
}

int 
p3d_comp (const void *v_a, const void *v_b)
{
  const p3d *a = v_a;
  const p3d *b = v_b;

  if (a->val < b->val)
    return -1;
  else if (a->val == b->val)
    return 0;
  else
    return 1;
}
