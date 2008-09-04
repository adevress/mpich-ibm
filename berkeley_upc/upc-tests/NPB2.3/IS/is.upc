/*************************************************************************
 *                                                                       * 
 *        N  A  S     P A R A L L E L     B E N C H M A R K S  2.3       *
 *                                                                       * 
 *                                  I S                                  * 
 *                                                                       * 
 ************************************************************************* 
 *                                                                       * 
 *   This benchmark is part of the NAS Parallel Benchmark 2.3 suite.     *
 *   It is described in NAS Technical Report 95-020.                     * 
 *                                                                       * 
 *   Permission to use, copy, distribute and modify this software        * 
 *   for any purpose with or without fee is hereby granted.  We          * 
 *   request, however, that all derived work reference the NAS           * 
 *   Parallel Benchmarks 2.3. This software is provided "as is"          *
 *   without express or implied warranty.                                * 
 *                                                                       * 
 *   Information on NPB 2.3, including the technical report, the         *
 *   original specifications, source code, results and information       * 
 *   on how to submit new results, is available at:                      * 
 *                                                                       * 
 *          http:  www.nas.nasa.gov NAS NPB                              * 
 *                                                                       * 
 *   Send comments or suggestions to  npb@nas.nasa.gov                   * 
 *   Send bug reports to              npb-bugs@nas.nasa.gov              * 
 *                                                                       * 
 *         NAS Parallel Benchmarks Group                                 * 
 *         NASA Ames Research Center                                     * 
 *         Mail Stop: T27A-1                                             * 
 *         Moffett Field, CA   94035-1000                                * 
 *                                                                       * 
 *         E-mail:  npb@nas.nasa.gov                                     * 
 *         Fax:     (415) 604-3957                                       * 
 *                                                                       * 
 ************************************************************************* 
 *                                                                       * 
 *   Author: M. Yarrow                                                   * 
 *                                                                       * 
 *   UPC Version : Frederic Vroman										 *
 *   																	 *
 *************************************************************************/
#include "npbparams.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <upc_relaxed.h>

/******************/
/* default values */
/******************/
#ifndef CLASS
#define CLASS 'S'
#endif


/*************/
/*  CLASS S  */
/*************/
#if CLASS == 'S'
#define  TOTAL_KEYS_LOG_2    16
#define  MAX_KEY_LOG_2       11
#define  NUM_BUCKETS_LOG_2   9
#endif


/*************/
/*  CLASS W  */
/*************/
#if CLASS == 'W'
#define  TOTAL_KEYS_LOG_2    20
#define  MAX_KEY_LOG_2       16
#define  NUM_BUCKETS_LOG_2   10
#endif

/*************/
/*  CLASS A  */
/*************/
#if CLASS == 'A'
#define  TOTAL_KEYS_LOG_2    23
#define  MAX_KEY_LOG_2       19
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS B  */
/*************/
#if CLASS == 'B'
#define  TOTAL_KEYS_LOG_2    25
#define  MAX_KEY_LOG_2       21
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS C  */
/*************/
#if CLASS == 'C'
#define  TOTAL_KEYS_LOG_2    27
#define  MAX_KEY_LOG_2       23
#define  NUM_BUCKETS_LOG_2   10
#endif


#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)
#define  MAX_KEY             (1 << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_KEYS            (TOTAL_KEYS/THREADS)

/*****************************************************************/
/* On larger number of processors, since the keys are (roughly)  */ 
/* gaussian distributed, the first and last processor sort keys  */ 
/* in a large interval, requiring array sizes to be larger. Note */
/* that for large THREADS, NUM_KEYS is, however, a small number*/   
/*****************************************************************/
#if THREADS < 256
#define  SIZE_OF_BUFFERS     3*NUM_KEYS/2  
#else                                      
#define  SIZE_OF_BUFFERS     3*NUM_KEYS    
#endif                                     
                                           
                                           
#define  MAX_PROCS           256           
#define  MAX_ITERATIONS      10
#define  TEST_ARRAY_SIZE     5


/***********************************/
/* Enable separate communication,  */
/* computation timing and printout */
/***********************************/
//#define  TIMING_ENABLED
shared double timecounter[THREADS] ;
shared double barriercounter[THREADS] ;

/*************************************/
/* Typedef: if necessary, change the */
/* size of int here by changing the  */
/* int type to, say, long            */
/*************************************/
typedef  int  INT_TYPE;


/********************/
/* Some global info */
/********************/
INT_TYPE *key_buff_ptr_global,         /* used by full_verfy to get */
         total_local_keys,             /* copies of rank info        */
         total_lesser_keys;

shared INT_TYPE lastkeys[THREADS] ; 

shared int passed_verification_shd[THREADS] ;
int *passed_verification ;                                 


/************************************/
/* These are the three main arrays. */
/* See SIZE_OF_BUFFERS def above    */
/************************************/
INT_TYPE key_array[SIZE_OF_BUFFERS],    
         key_buff2[SIZE_OF_BUFFERS],
         bucket_ptrs[NUM_BUCKETS],
         process_bucket_distrib_ptr1[NUM_BUCKETS+TEST_ARRAY_SIZE],   
         process_bucket_distrib_ptr2[NUM_BUCKETS+TEST_ARRAY_SIZE],
				 recv_displ[THREADS], recv_count[THREADS],
				 send_displ[THREADS], send_count[THREADS] ;
				 
shared INT_TYPE bucket_size_totals_shd[(NUM_BUCKETS+TEST_ARRAY_SIZE) * THREADS] ;
INT_TYPE *bucket_size_totals ;


shared INT_TYPE bucket_size_shd[THREADS * (NUM_BUCKETS+TEST_ARRAY_SIZE)] ;     /* Top 5 elements for */
INT_TYPE * bucket_size ;

shared INT_TYPE key_buff1_shd[THREADS * SIZE_OF_BUFFERS] ; 
INT_TYPE *key_buff1 ;

typedef struct
{
	int count ;
	int displ ;
} send_info ;
shared send_info send_infos_shd[THREADS][THREADS] ;
send_info *send_infos ;

/**********************/
/* Partial verif info */
/**********************/
INT_TYPE test_index_array[TEST_ARRAY_SIZE],
         test_rank_array[TEST_ARRAY_SIZE],

         S_test_index_array[TEST_ARRAY_SIZE] = 
                             {48427,17148,23627,62548,4431},
         S_test_rank_array[TEST_ARRAY_SIZE] = 
                             {0,18,346,64917,65463},

         W_test_index_array[TEST_ARRAY_SIZE] = 
                             {357773,934767,875723,898999,404505},
         W_test_rank_array[TEST_ARRAY_SIZE] = 
                             {1249,11698,1039987,1043896,1048018},

         A_test_index_array[TEST_ARRAY_SIZE] = 
                             {2112377,662041,5336171,3642833,4250760},
         A_test_rank_array[TEST_ARRAY_SIZE] = 
                             {104,17523,123928,8288932,8388264},

         B_test_index_array[TEST_ARRAY_SIZE] = 
                             {41869,812306,5102857,18232239,26860214},
         B_test_rank_array[TEST_ARRAY_SIZE] = 
                             {33422937,10244,59149,33135281,99}, 

         C_test_index_array[TEST_ARRAY_SIZE] = 
                             {44172927,72999161,74326391,129606274,21736814},
         C_test_rank_array[TEST_ARRAY_SIZE] = 
                             {61147,882988,266290,133997595,133525895};



/***********************/
/* function prototypes */
/***********************/
double	randlc( double *, double * );

void full_verify( void );
void    timer_clear( int );
void    timer_start( int );
void    timer_stop( int );
double  timer_read( int );

void c_print_results( const char   *name,
                      const char   class,
                      int    n1,
                      int    n2,
                      int    n3,
                      int    niter,
                      int    nthreads,
                      double t,
                      double mops,
                      const char   *optype,
                      int    passed_verification,
                      const char   *npbversion,
                      const char   *compiletime,
                      const char   *cc,
                      const char   *clink,
                      const char   *c_lib,
                      const char   *c_inc,
                      const char   *cflags,
                      const char   *clinkflags,
                      const char   *rand);



/*
 *    FUNCTION RANDLC (X, A)
 *
 *  This routine returns a uniform pseudorandom double precision number in the
 *  range (0, 1) by using the linear congruential generator
 *
 *  x_{k+1} = a x_k  (mod 2^46)
 *
 *  where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
 *  before repeating.  The argument A is the same as 'a' in the above formula,
 *  and X is the same as x_0.  A and X must be odd double precision integers
 *  in the range (1, 2^46).  The returned value RANDLC is normalized to be
 *  between 0 and 1, i.e. RANDLC = 2^(-46) * x_1.  X is updated to contain
 *  the new seed x_1, so that subsequent calls to RANDLC using the same
 *  arguments will generate a continuous sequence.
 *
 *  This routine should produce the same results on any computer with at least
 *  48 mantissa bits in double precision floating point data.  On Cray systems,
 *  double precision should be disabled.
 *
 *  David H. Bailey     October 26, 1990
 *
 *     IMPLICIT DOUBLE PRECISION (A-H, O-Z)
 *     SAVE KS, R23, R46, T23, T46
 *     DATA KS/0/
 *
 *  If this is the first call to RANDLC, compute R23 = 2 ^ -23, R46 = 2 ^ -46,
 *  T23 = 2 ^ 23, and T46 = 2 ^ 46.  These are computed in loops, rather than
 *  by merely using the ** operator, in order to insure that the results are
 *  exact on all systems.  This code assumes that 0.5D0 is represented exactly.
 */


/*****************************************************************/
/*************           R  A  N  D  L  C             ************/
/*************                                        ************/
/*************    portable random number generator    ************/
/*****************************************************************/

double	randlc(X, A)
double *X;
double *A;
{
      static int        KS=0;
      static double	R23, R46, T23, T46;
      double		T1, T2, T3, T4;
      double		A1;
      double		A2;
      double		X1;
      double		X2;
      double		Z;
      int     		i, j;

      if (KS == 0) 
      {
        R23 = 1.0;
        R46 = 1.0;
        T23 = 1.0;
        T46 = 1.0;
    
        for (i=1; i<=23; i++)
        {
          R23 = 0.50 * R23;
          T23 = 2.0 * T23;
        }
        for (i=1; i<=46; i++)
        {
          R46 = 0.50 * R46;
          T46 = 2.0 * T46;
        }
        KS = 1;
      }

/*  Break A into two parts such that A = 2^23 * A1 + A2 and set X = N.  */

      T1 = R23 * *A;
      j  = T1;
      A1 = j;
      A2 = *A - T23 * A1;

/*  Break X into two parts such that X = 2^23 * X1 + X2, compute
    Z = A1 * X2 + A2 * X1  (mod 2^23), and then
    X = 2^23 * Z + A2 * X2  (mod 2^46).                            */

      T1 = R23 * *X;
      j  = T1;
      X1 = j;
      X2 = *X - T23 * X1;
      T1 = A1 * X2 + A2 * X1;
      
      j  = R23 * T1;
      T2 = j;
      Z = T1 - T23 * T2;
      T3 = T23 * Z + A2 * X2;
      j  = R46 * T3;
      T4 = j;
      *X = T3 - T46 * T4;
      return(R46 * *X);
} 



/*****************************************************************/
/************   F  I  N  D  _  M  Y  _  S  E  E  D    ************/
/************                                         ************/
/************ returns parallel random number seq seed ************/
/*****************************************************************/

/*
 * Create a random number sequence of total length nn residing
 * on np number of processors.  Each processor will therefore have a 
 * subsequence of length nn/np.  This routine returns that random 
 * number which is the first random number for the subsequence belonging
 * to processor rank kn, and which is used as seed for proc kn ran # gen.
 */

double   find_my_seed( long kn,       /* my processor rank, 0<=kn<=num procs */
                       long np,       /* np = num procs                      */
                       long nn,       /* total num of ran numbers, all procs */
                       double s,      /* Ran num seed, for ex.: 314159265.00 */
                       double a )     /* Ran num gen mult, try 1220703125.00 */
{

  long   i;

  double t1,t2,t3,an;
  long   mq,nq,kk,ik;



      nq = nn / np;

      for( mq=0; nq>1; mq++,nq/=2 )
          ;

      t1 = a;

      for( i=1; i<=mq; i++ )
        t2 = randlc( &t1, &t1 );

      an = t1;

      kk = kn;
      t1 = s;
      t2 = an;

      for( i=1; i<=100; i++ )
      {
        ik = kk / 2;
        if( 2 * ik !=  kk ) 
            t3 = randlc( &t1, &t2 );
        if( ik == 0 ) 
            break;
        t3 = randlc( &t2, &t2 );
        kk = ik;
      }

      return( t1 );

}




/*****************************************************************/
/*************      C  R  E  A  T  E  _  S  E  Q      ************/
/*****************************************************************/

void	create_seq( double seed, double a )
{
	double x;
	int    i, j, k;

        k = MAX_KEY/4;

	for (i=0; i<NUM_KEYS; i++)
	{
	    x = randlc(&seed, &a);
	    x += randlc(&seed, &a);
    	    x += randlc(&seed, &a);
	    x += randlc(&seed, &a);  

            key_array[i] = k*x;
	}
}




/*****************************************************************/
/*************    F  U  L  L  _  V  E  R  I  F  Y     ************/
/*****************************************************************/

void full_verify()
{
    INT_TYPE    i, j;
    INT_TYPE    k;

    INT_TYPE    m, unique_keys;
    

/*  Now, finally, sort the keys:  */
    for( i=0; i<total_local_keys; i++ )
      key_array[--key_buff_ptr_global[key_buff2[i]] - total_lesser_keys] = key_buff2[i];
    
    lastkeys[MYTHREAD] = key_array[total_local_keys - 1] ;
		
    upc_barrier ;
    
    if(MYTHREAD > 0)
      k = lastkeys[MYTHREAD - 1] ;
    
    /*  Confirm that neighbor's greatest key value 
	is not greater than my least key value       */              
    j = 0;
    if( MYTHREAD > 0 )
      if( k > key_array[0] )
	j++;
    
    /*  Confirm keys correctly sorted: count incorrectly sorted keys, if any */
    for( i=1; i<total_local_keys; i++ )
      if( key_array[i-1] > key_array[i] )
	j++;
    
    
    if( j != 0 )
      {
        printf( "Processor %d:  Full_verify: number of keys out of sort: %d\n",
                MYTHREAD, j );
      }
    else
      (*passed_verification)++;
}


typedef int elem_t;


void upc_reduced_sum(shared elem_t* buffer, size_t size)
{
	shared elem_t * shared *ptrs ;
	int thr_cnt;
	int p ;
	elem_t *local_array, *lp ;
	
	ptrs = (shared elem_t * shared*)upc_all_alloc(THREADS, sizeof(elem_t *)) ;
	ptrs[MYTHREAD] = (shared elem_t *)buffer ;
	
	local_array = (elem_t *)malloc(size * sizeof(elem_t)) ;
	lp = (elem_t *)buffer ;

	timer_start(6);
	upc_barrier ;
	timer_stop(6);

	/* reduce */
	upc_forall(thr_cnt=1; thr_cnt<THREADS; thr_cnt <<= 1; continue)
	{ 	
		if (!(MYTHREAD%(thr_cnt<<1)))
		{	
			upc_memget(local_array, ptrs[MYTHREAD + thr_cnt], size * sizeof(elem_t)) ;

			for(p = 0; p < size; p++)
				lp[p] += local_array[p] ;
		}

		timer_start(6);
		upc_barrier ;
		timer_stop(6);
	}
}


/*****************************************************************/
/*************             R  A  N  K             ****************/
/*****************************************************************/
void rank( int iteration )
{

    INT_TYPE    i, j, k;
    INT_TYPE    l, m;

    INT_TYPE    shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    INT_TYPE    key;
    INT_TYPE    bucket_sum_accumulator;
    INT_TYPE    local_bucket_sum_accumulator;
    INT_TYPE    min_key_val, max_key_val;
    INT_TYPE    *key_buff_ptr;
		
		shared [] INT_TYPE * key_buff2_remptr ;

		INT_TYPE		remote_bucket_size[NUM_BUCKETS + TEST_ARRAY_SIZE] ;

		int total_displ = 0 ;
		send_info infos ;

/*  Iteration alteration of keys */  
    if( !MYTHREAD )                    
    {
      key_array[iteration] = iteration ;
      key_array[iteration+MAX_ITERATIONS] = MAX_KEY - iteration;
    }

/*  Initialize */
		memset(bucket_size, 0, (NUM_BUCKETS + TEST_ARRAY_SIZE) * sizeof(*bucket_size)) ;
		memset(bucket_size_totals, 0, NUM_BUCKETS + TEST_ARRAY_SIZE) ;
		memset(process_bucket_distrib_ptr1, 0, NUM_BUCKETS + TEST_ARRAY_SIZE) ;
		memset(process_bucket_distrib_ptr2, 0, NUM_BUCKETS + TEST_ARRAY_SIZE) ;

/*  Determine where the partial verify test keys are, load into  */
/*  top of array bucket_size using the private pointer           */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
	    if( (test_index_array[i]/NUM_KEYS) == MYTHREAD )
  	    bucket_size[NUM_BUCKETS+i] = key_array[test_index_array[i] % NUM_KEYS];


/*  Determine the number of keys in each bucket */
    for( i=0; i<NUM_KEYS; i++ )
        bucket_size[key_array[i] >> shift]++;


/*  Accumulative bucket sizes are the bucket pointers */
    bucket_ptrs[0] = 0;
    for( i=1; i< NUM_BUCKETS; i++ )  
        bucket_ptrs[i] = bucket_ptrs[i-1] + bucket_size[i-1];


/*  Sort into appropriate bucket */
    for( i=0; i<NUM_KEYS; i++ )  
    {
        key = key_array[i];
        key_buff1[bucket_ptrs[key >> shift]++] = key;
    }

	memcpy(bucket_size_totals, bucket_size, (NUM_BUCKETS+TEST_ARRAY_SIZE) * sizeof(*bucket_size)) ;
#ifdef  TIMING_ENABLED
    timer_stop( 2 );
    timer_start( 3 );
    timer_start( 4 );
#endif
	/*  Get the bucket size totals for the entire problem. These 
    	will be used to determine the redistribution of keys      */
	upc_reduced_sum(&bucket_size_totals_shd[MYTHREAD], NUM_BUCKETS+TEST_ARRAY_SIZE) ;

	/* All other threads copy the result locally. With 
	 * upc reduced only the thread 0 has the final result */
	if(MYTHREAD != 0)
		upc_memget(bucket_size_totals, bucket_size_totals_shd, (NUM_BUCKETS+TEST_ARRAY_SIZE) * sizeof(*bucket_size_totals_shd)) ;
		
#ifdef  TIMING_ENABLED
	timer_stop( 4 ) ;
    timer_start( 5 ) ;
#endif
/*  Determine Redistibution of keys: accumulate the bucket size totals 
    till this number surpasses NUM_KEYS (which the average number of keys
    per processor).  Then all keys in these buckets go to processor 0.
    Continue accumulating again until supassing 2*NUM_KEYS. All keys
    in these buckets go to processor 1, etc.  This algorithm guarantees
    that all processors have work ranking; no processors are left idle.
    The optimum number of buckets, however, does not result in as high
    a degree of load balancing (as even a distribution of keys as is
    possible) as is obtained from increasing the number of buckets, but
    more buckets results in more computation per processor so that the
    optimum number of buckets turns out to be 1024 for machines tested.
    Note that process_bucket_distrib_ptr1 and ..._ptr2 hold the bucket
    number of first and last bucket which each processor will have after   
    the redistribution is done.                                          */
    bucket_sum_accumulator = 0;
    local_bucket_sum_accumulator = 0;
	send_infos[0].displ = 0 ;
    process_bucket_distrib_ptr1[0] = 0;
		
    for( i = 0, j = 0; i < NUM_BUCKETS; i++ )  
    {
        bucket_sum_accumulator       += bucket_size_totals[i];
        local_bucket_sum_accumulator += bucket_size[i];
        if( bucket_sum_accumulator >= (j+1)*NUM_KEYS )  
        {
			send_infos[j].count = local_bucket_sum_accumulator; 
						
            if( j != 0 )
            {
	      send_infos[j].displ = send_infos[j-1].displ + send_infos[j-1].count ;
	      
	      process_bucket_distrib_ptr1[j] = process_bucket_distrib_ptr2[j-1]+1;
	    }

    	    process_bucket_distrib_ptr2[j++] = i;
            local_bucket_sum_accumulator = 0;
        }
    }

    timer_start(6);
    upc_barrier ;
    timer_stop(6);
	/**
	 * Equivalent to the mpi_alltoall + mpialltoallv in the c + mpi version
	 * of the NAS Parallel benchmark.
	 *
	 * Each thread copy locally his keys from the other processors (keys
	 * redistribution).
	 */
		for(i = 0; i < THREADS; i++)
		{
			infos = send_infos_shd[MYTHREAD][i] ;

			if(i == MYTHREAD)
				memcpy(key_buff2 + total_displ, key_buff1 + infos.displ, infos.count * sizeof(INT_TYPE)) ;
			else
				upc_memget(key_buff2 + total_displ, key_buff1_shd + i + infos.displ * THREADS, infos.count * sizeof(INT_TYPE)) ;

			total_displ += infos.count ;
		}
		timer_start(6);
		upc_barrier ;
		timer_stop(6);

#ifdef  TIMING_ENABLED
    timer_stop( 5 ); 
    timer_stop( 3 ); 
    timer_start( 2 );
#endif

/*  The starting and ending bucket numbers on each processor are
    multiplied by the interval size of the buckets to obtain the 
    smallest possible min and greatest possible max value of any 
    key on each processor                                          */
    min_key_val = process_bucket_distrib_ptr1[MYTHREAD] << shift;
    max_key_val = ((process_bucket_distrib_ptr2[MYTHREAD] + 1) << shift)-1;

/*  Clear the work array */
	memset(key_buff1, 0, (max_key_val - min_key_val + 1) * sizeof(*key_buff1)) ;

/*  Determine the total number of keys on all other 
    processors holding keys of lesser value         */
    m = 0;
    for( k=0; k<MYTHREAD; k++ )
        for( i= process_bucket_distrib_ptr1[k];
             i<=process_bucket_distrib_ptr2[k];
             i++ )  
            m += bucket_size_totals[i]; /*  m has total # of lesser keys */

/*  Determine total number of keys on this processor */
    j = 0;                                 
    for( i= process_bucket_distrib_ptr1[MYTHREAD];
         i<=process_bucket_distrib_ptr2[MYTHREAD];
         i++ )  
        j += bucket_size_totals[i];     /* j has total # of local keys   */

/*  Ranking of all keys occurs in this section:                 */
/*  shift it backwards so no subtractions are necessary in loop */
    key_buff_ptr = key_buff1 - min_key_val;

/*  In this section, the keys themselves are used as their 
    own indexes to determine how many of each there are: their
    individual population                                       */
    for( i=0; i<j; i++ )
        key_buff_ptr[key_buff2[i]]++;  /* Now they have individual key   */
                                       /* population                     */

/*  To obtain ranks of each key, successively add the individual key
    population, not forgetting to add m, the total of lesser keys,
    to the first key population                                          */
    key_buff_ptr[min_key_val] += m;    
    for( i=min_key_val; i<max_key_val; i++ )   
        key_buff_ptr[i+1] += key_buff_ptr[i];  


/* This is the partial verify test section */
/* Observe that test_rank_array vals are   */
/* shifted differently for different cases */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
    {                                             
        k = bucket_size_totals[i+NUM_BUCKETS];    /* Keys were hidden here */
        if( min_key_val <= k  &&  k <= max_key_val )
				{
#if (CLASS == 'S')
					if( i <= 2 )
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
					else
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
#endif
#if (CLASS == 'W')
					if( i < 2 )
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]+(iteration-2) )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
					else
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
						(*passed_verification)++;
					}
#endif
#if (CLASS == 'A')
					if( i <= 2 )
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]+(iteration-1) )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
					else
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]-(iteration-1) )
						{
							printf( "Failed partial verification: "
									"iteration %d, processor %d, test key %d\n", 
									 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
#endif
#if (CLASS == 'B')
					if( i == 1 || i == 2 || i == 4 )
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
					else
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
						{
							printf( "Failed partial verification: "
									"iteration %d, processor %d, test key %d\n", 
									 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
#endif
#if (CLASS == 'C')
					if( i <= 2 )
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
					else
					{
						if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
						{
							printf( "Failed partial verification: "
										"iteration %d, processor %d, test key %d\n", 
										 iteration, MYTHREAD, i );
						}
						else
							(*passed_verification)++;
					}
#endif
				}
    }

/*  Make copies of rank info for use by full_verify: these variables
    in rank are local; making them global slows down the code, probably
    since they cannot be made register by compiler                        */

    if( iteration == MAX_ITERATIONS ) 
    {
        key_buff_ptr_global = key_buff_ptr;
        total_local_keys    = j;
        total_lesser_keys   = m;
    }
}      


/*****************************************************************/
/*************             M  A  I  N             ****************/
/*****************************************************************/

int main( argc, argv )
    int argc;
    char **argv;
{
    int             i, iteration, itemp;
    double          maxtime, barrier_time, timeval ;

		/**
		 * buket_size buffer initialization.
		 */
		bucket_size = (INT_TYPE *)(bucket_size_shd + MYTHREAD) ;
		bucket_size_totals = (INT_TYPE *)(bucket_size_totals_shd + MYTHREAD) ;

		/**
		 * Passed_verification private pointer initialization
		 */
		passed_verification = (int *)(passed_verification_shd + MYTHREAD) ;

		/**
		 * Initialize the send_count private pointer
		 */
		send_infos = (send_info *)&send_infos_shd[0][MYTHREAD] ;

		/**
		 * key_buff1 buffer initialization.
		 */
		key_buff1 = (INT_TYPE *)(key_buff1_shd + MYTHREAD) ;

/*  Initialize the verification arrays if a valid class */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
		{
#if (CLASS == 'S')
    	test_index_array[i] = S_test_index_array[i];
      test_rank_array[i]  = S_test_rank_array[i];
#endif
#if (CLASS == 'A')
      test_index_array[i] = A_test_index_array[i];
      test_rank_array[i]  = A_test_rank_array[i];
#endif
#if (CLASS == 'W')
      test_index_array[i] = W_test_index_array[i];
      test_rank_array[i]  = W_test_rank_array[i];
#endif
#if (CLASS == 'B')
      test_index_array[i] = B_test_index_array[i];
      test_rank_array[i]  = B_test_rank_array[i];
#endif
#if (CLASS == 'C')
      test_index_array[i] = C_test_index_array[i];
      test_rank_array[i]  = C_test_rank_array[i];
#endif
		}

/*  Printout initial NPB info */
    if( !MYTHREAD )
    {
        printf( "\n\n NAS Parallel Benchmarks 2.3 -- IS Benchmark\n\n" );
        printf( " Size:  %d  (class %c)\n", TOTAL_KEYS, CLASS );
        printf( " Iterations:   %d\n", MAX_ITERATIONS );
        printf( " Number of processes:     %d\n", THREADS );
    }

/*  Generate random number sequence and subsequent keys on all procs */
    create_seq( find_my_seed( MYTHREAD, 
                              THREADS, 
                              4*TOTAL_KEYS,
                              314159265.00,      /* Random number gen seed */
                              1220703125.00 ),   /* Random number gen mult */
							                1220703125.00 );   /* Random number gen mult */


/*  Do one interation for free (i.e., untimed) to guarantee initialization of  
    all data and code pages and respective tables */
    rank( 1 );  

/*  Start verification counter */
    (*passed_verification) = 0;

    if( !MYTHREAD && CLASS != 'S' ) printf( "\n   iteration\n" );

/*  Initialize timer  */             
    timer_clear( 0 );

/*  Initialize separate communication, computation timing */
#ifdef  TIMING_ENABLED 
    for( i=1; i<=5; i++ ) 
			timer_clear( i );
#endif

/*  Start timer  */             
    timer_start( 0 );

#ifdef  TIMING_ENABLED
    timer_start( 1 );
    timer_start( 2 );
#endif


/*  This is the main iteration */
    for( iteration=1; iteration<=MAX_ITERATIONS; iteration++ )
    {
        if( !MYTHREAD && CLASS != 'S' ) printf( "        %d\n", iteration );
        rank( iteration );
	timer_start(6);
				upc_barrier ;
				timer_stop(6);
    }


#ifdef  TIMING_ENABLED
    timer_stop( 2 );
    timer_stop( 1 );
#endif

/*  Stop timer, obtain time for processors */
    timer_stop( 0 );

    timecounter[MYTHREAD] = timer_read( 0 );
    barriercounter[MYTHREAD] = timer_read( 6 );
    

/*  End of timing, obtain maximum time of all processors */
		upc_barrier ;

		if(!MYTHREAD)
		{
			maxtime = timecounter[0] ;
			barrier_time = barriercounter[0];
			for(i = 1; i < THREADS; i ++)
			{
				timeval = timecounter[i] ;
				if(maxtime < timeval)
					maxtime = timeval ;
				if (barrier_time < barriercounter[i]) {
				  barrier_time = barriercounter[i];
				}
			}
		}

#ifdef  TIMING_ENABLED
		{
        double    tmin, tsum, tmax, val ;
   			int p ;
				
        if( !MYTHREAD )
        {
            printf( "              min                avg                max\n" );
        }

				for( i=1; i<=5; i++ )
        {
    	    timecounter[MYTHREAD] = timer_read( i ) ;
					upc_barrier ;
						
 					if(!MYTHREAD)
					{
						tmax = timecounter[0] ;
						tmin = tmax ;
						tsum = tmax ;
						for(p = 1; p < THREADS; p ++) 
						{
							val = timecounter[p] ;
							if(tmax < val)
								tmax = val ;
							if(tmin > val)
								tmin = val ;
							tsum += val ;
						}
						switch(i)
						{
							case 1: printf("Total Time : \t\t\t") ;
							break;;
							case 2: printf("Sequential time : \t\t") ;
							break;;
							case 3: printf("Communication time : \t\t") ;
							break;;
							case 4: printf("MPI_Allreduce(UPC) time : \t\t") ;
							break;;
							case 5: printf("MPI_Alltoall + MPI_Alltoallv(UPC) time :") ;
							break;;
						} ;
						printf( "    %f           %f            %f\n",
                   tmin, tsum/((double) THREADS), tmax );
					}
					upc_barrier ;
        }
  
				if( !MYTHREAD )
	        printf( "\n" );
    }
#endif

/*  This tests that keys are in sequence: sorting of last ranked key seq
    occurs here, but is an untimed operation                             */
    full_verify();

	upc_barrier ;

/*  The final printout  */
    if( !MYTHREAD )
    {
/*  Obtain verification counter sum */
			for(i = 1 ;i < THREADS; i++)
				*passed_verification += passed_verification_shd[i] ;

			if( *passed_verification != 5 * MAX_ITERATIONS + THREADS)
          *passed_verification = 0;
			printf ("barrier time in seconds =                %12.2f\n", barrier_time);
			c_print_results( "IS",
                       CLASS,
                       TOTAL_KEYS,
                       0,
                       0,
                       MAX_ITERATIONS,
                       THREADS,
                       maxtime,
                       ((double) (MAX_ITERATIONS*TOTAL_KEYS)) /maxtime/1000000.,
                       "keys ranked", 
                       *passed_verification,
                       NPBVERSION,
                       COMPILETIME,
                       CC,
                       CLINK,
                       C_LIB,
                       C_INC,
                       CFLAGS,
                       CLINKFLAGS,
											 "randlc");
    }
 
		upc_barrier ;
		return 0 ;
		 /**************************/
}        /*  E N D  P R O G R A M  */
         /**************************/
