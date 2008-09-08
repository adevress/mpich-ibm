#ifndef GUPS_H_INCLUDE
#define GUPS_H_INCLUDE

#include <stdint.h>

#define uint64 uint64_t
#define int64 int64_t


/* Macros for timing */
struct tms t;
#define WSEC() (times(&t) / (double)sysconf(_SC_CLK_TCK))
//#define CPUSEC() (clock() / (double)CLOCKS_PER_SEC)


/* Random number generator */
#ifdef  LONG_IS_64BITS
#undef  LONG_IS_64BITS
#endif
#ifdef LONG_IS_64BITS
#define POLY 0x0000000000000007UL
#define PERIOD 1317624576693539401L
#else
#define POLY 0x0000000000000007ULL
#define PERIOD 1317624576693539401LL
#endif

/* Types used by program (should be 64 bits) */
typedef uint64_t u64Int;
typedef int64_t s64Int;
#ifdef LONG_IS_64BITS
#define FSTR64 "%ld"
#define ZERO64B 0L
#else
#define FSTR64 "%lld"
#define ZERO64B 0LL
#endif



/* Log size of substitution table (suggested: half of primary cache) */
#define LSTSIZE 9
#define STSIZE (1 << LSTSIZE)

/* Log size of main table (suggested: half of global memory) */
#ifndef LTABSIZE
#define LTABSIZE 25L
#endif
#define TABSIZE (1L << LTABSIZE)

/* Number of updates to table (suggested: 4x number of table entries) */



#define LOOKAHEAD_WIN 1024
#define NBOXES 2 /* increase this if there are too many race conditions and the
		     benchmark does not verify */


//typedef  uint64  Update_Win[LOOKAHEAD_WIN];
extern uint64 *outbuf0, *outbuf1;

typedef struct MB 
{
  int next;
  shared [] uint64*  mbox[NBOXES];
} MailBox;

typedef shared [] uint64* WinPtr;

#define CACHE_LINE_PAD 1024

#define CPUSEC() ((double)clock()/CLOCKS_PER_SEC)
#define RTSEC() ((double)UPC_Wtime())
#include <sys/time.h>

double UPC_Wtime()
{
  struct timeval sampletime;
  double time;

  gettimeofday( &sampletime, NULL );
  time = sampletime.tv_sec + (sampletime.tv_usec / 1000000.0);
  return( time );
}



#endif
