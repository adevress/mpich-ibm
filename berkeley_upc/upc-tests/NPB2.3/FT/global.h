#ifdef __BERKELEY_UPC__
#pragma upc upc_code
#endif
#include "npbparams.h"


/* Layout distribution parameters:
 * 2D processor array -> 2D grid decomposition (by pencils)
 * If processor array is 1xN or -> 1D grid decomposition (by planes)
 * If processor array is 1x1 -> 0D grid decomposition
 */

int np1, np2;
int layout_type;		// 0 = layout_0D, 1 = layout_1D, 2 = layout_2D

int ME1, ME2;

/*
 * If processor array is 1x1 -> 0D grid decomposition


 * Cache blocking params. These values are good for most
 * RISC processors.  
 * FFT parameters:
 *  fftblock controls how many ffts are done at a time. 
 *  The default is appropriate for most cache-based machines
 *  On vector machines, the FFT can be vectorized with vector
 *  length equal to the block size, so the block size should
 *  be as large as possible. This is the size of the smallest
 *  dimension of the problem: 128 for class A, 256 for class B and
 *  512 for class C.
 */

#define	FFTBLOCK_DEFAULT	16
#define	FFTBLOCKPAD_DEFAULT	18

#define FFTBLOCK	FFTBLOCK_DEFAULT
#define FFTBLOCKPAD	FFTBLOCKPAD_DEFAULT

/* COMMON block: blockinfo */
int fftblock;
int fftblockpad;

#define TRANSBLOCK 32
#define TRANSBLOCKPAD 34

/*
 * we need a bunch of logic to keep track of how
 * arrays are laid out. 


 * Note: this serial version is the derived from the parallel 0D case
 * of the ft NPB.
 * The computation proceeds logically as

 * set up initial conditions
 * fftx(1)
 * transpose (1->2)
 * ffty(2)
 * transpose (2->3)
 * fftz(3)
 * time evolution
 * fftz(3)
 * transpose (3->2)
 * ffty(2)
 * transpose (2->1)
 * fftx(1)
 * compute residual(1)

 * for the 0D, 1D, 2D strategies, the layouts look like xxx
 *        
 *            0D        1D        2D
 * 1:        xyz       xyz       xyz
 * 2:        xyz       xyz       yxz
 * 3:        xyz       zyx       zxy

 * the array dimensions are stored in dims(coord, phase)
 */

/* COMMON block: layout */
static int dims[3][3];
static int xstart[3];
static int ystart[3];
static int zstart[3];
static int xend[3];
static int yend[3];
static int zend[3];

#define	T_TOTAL		0
#define	T_SETUP		1
#define	T_FFT		2
#define	T_EVOLVE	3
#define	T_CHECKSUM	4
#define	T_FFTLOW	5
#define	T_FFTCOPY	6
#define	T_MAX		7
#define T_TRANSPOSE     8

#define TIMERS_ENABLED  TRUE

/* other stuff */

#define	SEED	314159265.0
#define	A	1220703125.0
#define	PI	3.141592653589793238
#define	ALPHA	1.0e-6

#define	EXPMAX	(NITER_DEFAULT*(NX*NX/4+NY*NY/4+NZ*NZ/4))

/* COMMON block: excomm */
static double ex[EXPMAX + 1];	/* ex(0:expmax) */

/*
 * roots of unity array
 * relies on x being largest dimension?
 */

/* COMMON block: ucomm */
// used in fft_init(), cfftz(), fftz2()
static dcomplex u[NX];
shared[]
     dcomplex *shared reshuffle_arr_shd[THREADS];
shared[]
     dcomplex *reshuffle_arr[THREADS];


/* for checksum data */

/* COMMON block: sumcomm */
     static shared dcomplex sums[NITER_DEFAULT + 1];	/* sums(0:niter_default) */

     static upc_lock_t *sum_write;


/* number of iterations */

/* COMMON block: iter */
     static int niter;
