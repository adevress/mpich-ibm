#include <upc.h>
#include <stdio.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include "gups.h"
#include <hpcc.h>



shared uint64 *table;
uint64 *outbuf0, *outbuf1, *outbuf;
MailBox *incoming_mailboxes, *outgoing_mailboxes;
shared void* init_arena;
uint64 Stable[STSIZE];
shared int64 errors[THREADS];
shared uint64 tabsize, logtabsize;
int count_phases = 0;
bupc_handle_t handle[64];

/*
  Utility routine to start random number generator at Nth step
*/
uint64 startr(int64 n)
{
    int i, j;
    uint64 m2[64];
    uint64 temp, ran;

    while (n < 0) n += PERIOD;
    while (n > PERIOD) n -= PERIOD;
    if (n == 0) return 0x1;

    temp = 0x1;
    for (i=0; i<64; i++)
    {
	m2[i] = temp;
	temp = (temp << 1) ^ ((int64) temp < 0 ? POLY : 0);
	temp = (temp << 1) ^ ((int64) temp < 0 ? POLY : 0);
    }
    
    for (i=62; i>=0; i--)
	if ((n >> i) & 1)
	    break;

    ran = 0x2;
    while (i > 0)
    {
	temp = 0;
	for (j=0; j<64; j++)
	    if ((ran >> j) & 1)
		temp ^= m2[j];
	ran = temp;
	i -= 1;
	if ((n >> i) & 1)
	    ran = (ran << 1) ^ ((int64) ran < 0 ? POLY : 0);
    }
  
    return ran;
}    


void Fill_Lookahead_Stream (uint64 *stream, uint64 seed) 
{
  int j;
  for (j=0; j < LOOKAHEAD_WIN; j++)
    stream[j] = (stream[j] << 1) ^ (((int64) stream[j] < 0) ? POLY : 0);
}



void Fill_Outbuf (uint64 *idx, uint64 *obuf, uint64 block, uint64 tabsize) 
{
  int i;
  uint64  th;
  int64 static_contrib;
  uint64 *th_off;
  uint64 *lptr;
  int dth;
  /* (idx[j] & (tabsize - 1)) gives the index of the element in table
     upc_threadof(...) gives the thread 
  */

  /* zero the number of active  updates */  
  static_contrib = tabsize - 1;
  for(i = 0; i < THREADS; i++)
    obuf[i*LOOKAHEAD_WIN] = 0;
  
  for( i = 0; i < block; i++) {
    th = idx[i] & static_contrib;

    th = ((int)th) % THREADS;
    if((int)th == MYTHREAD) {
      lptr = (uint64*)(table + (ptrdiff_t)(idx[i] & static_contrib));
      *lptr = *lptr ^ Stable[idx[i] >> (64-LSTSIZE)];    
    } else {
      th_off = obuf + th*LOOKAHEAD_WIN;
      th_off[1 + th_off[0]] = idx[i];
      th_off[0] = th_off[0] + 1;
    }
  }
 
} 



void Init_Push_Updates (uint64 *obuf, MailBox *obox) 
{
  
  int box_no,i;
  shared void *dest;
  uint64 *buf;
  count_phases++;

  /* init remote transfers */
  
 
  for(i = (MYTHREAD+1)%THREADS; i != MYTHREAD; i = (i+1)%THREADS) {
    buf = obuf + i*LOOKAHEAD_WIN;
    if(buf[0]) {
      box_no = obox[i].next;
      dest = (shared void *) obox[i].mbox[box_no];
      handle[i] = bupc_memput_async(dest, buf, (size_t)(sizeof(uint64)*(buf[0]+1)));
      obox[i].next = (obox[i].next + 1) % NBOXES;
    }
  }
}



void Process_Incoming_Updates(MailBox  *incoming, uint64 tabsize) 
{
  MailBox *curbox;
  int curbuf;
  uint64 *data;
  int i, j, k, lim;
  uint64* lptr;
  shared [] uint64* local;
  uint64 static_contrib;
  int box_no;
  static_contrib = tabsize - 1;
  
 
  for(i = (THREADS+MYTHREAD-1)%THREADS; i != MYTHREAD; i = (THREADS+i-1)%THREADS) {
    curbox = &incoming[i];
    box_no = curbox->next;
    local = curbox->mbox[box_no];
    data = (uint64*) local;
    lim = data[0];
    for( k = 0; k < lim; k++) {
      lptr = (uint64*)(table  + (int)(data[k+1] & static_contrib));
      *lptr = *lptr ^ Stable[data[k+1] >> (64-LSTSIZE)];
    }
    data[0] = 0;
    curbox->next = (curbox->next+1)%NBOXES;
  }
}


void Finish_Push_Updates (MailBox *obox) 
{
  int i; 
  /* TODO - take this barrier out */
   for(i=0; i < THREADS; i++)
     bupc_waitsync(handle[i]);

   upc_barrier;
}

uint64 Verify(int64 nupdate, int64 tabsize) 
{
  uint64 temp;
  uint64 i, errs;
  uint64 th, idx;
  uint64 static_contrib;
  ptrdiff_t offset;
  static_contrib = tabsize-1;
  /* Verification of results (in serial or "safe" mode; optional) */
  temp = 0x1;
  for (i=0; i<nupdate; i++)
    {
      temp = (temp << 1) ^ (((int64) temp < 0) ? POLY : 0);
      offset = (ptrdiff_t)(temp & static_contrib);
      th = upc_threadof(table + offset);
      if (th == MYTHREAD) 
	table[offset] = table[offset] ^ Stable[temp >> (64-LSTSIZE)];
    }
  
  errs = 0;
  for (i=MYTHREAD; i < tabsize ; i+=THREADS) {
    if (table[(ptrdiff_t)i] != i) {
	errs++;
    }
  }
  
  errors[MYTHREAD] = errs;
  upc_barrier;

  if(MYTHREAD == 0) {
    errs = 0;
    for(i = 0; i < THREADS; i++)
      errs = errs + errors[i];
    return errs;
  } 
  
  return 0;
  
}

/* There's a race condition between init_push_updates, finish_updates 
   and process_updates.
   Right now element 0 in an update window holds the number of updates
   The code sends the count and the values together as a contiguous buffer.
   On the receiving side, the code checks for the 0th entry and decides
   that a full window has been received. 

   TODO:
   If the code does not verify - need to separate the two and use
   the send count as a synchronization event. Alternately I can 
   use semaphores. 
   
   Also: there's no control flow between the senders and receivers.
   Both advance round-robin over a buffer of windows of depth NBOXES.
   If the sender runs ahead of the receiver the code is losing updates.
   Again, if it doesn't verify, either increase NBOXES (if you like
   living dangerously) or add control flow between the send/receive windows.

*/ 
 /* Perform updates to main table.  The scalar equivalent is:
   *
   *     u64Int Ran;
   *     Ran = 1;
   *     for (i=0; i<NUPDATE; i++) {
   *       Ran = (Ran << 1) ^ (((s64Int) Ran < 0) ? POLY : 0);
   *       Table[Ran & (TABSIZE-1)] ^= Ran;
   *     }
   */


uint64 update_table(int64 logtabsize,
		  int64 tabsize)
{
    uint64 ran[LOOKAHEAD_WIN];              /* Current random numbers */
    uint64 t1[LOOKAHEAD_WIN];
    uint64 temp;
    double icputime;               /* CPU time to init table */
    double is;
    double cputime;               /* CPU time to update table */
    double s;
    int64 start, stop, size;
    uint64 *local_table;
    int64 i;
    int64 j;
    uint64 seed;
    uint64 nupdate;
    uint64 ltabsize;
    
    /* Initialize main table */
    local_table = (uint64*) (table + MYTHREAD);
    ltabsize  = tabsize/THREADS;
    nupdate = 4*tabsize;
    local_table[0] = MYTHREAD;
    for(i = 0, seed=MYTHREAD; i < ltabsize; i++, seed += THREADS)
      local_table[i] = seed;
  
    upc_barrier;
    size = nupdate/(uint64)THREADS;
    
    /* compute initial address stream */
    for (j=0,i=MYTHREAD; j<LOOKAHEAD_WIN; j++, i+= THREADS) {
      ran[j] = startr ((nupdate/(LOOKAHEAD_WIN*THREADS)) * i);
    }
    
    Fill_Lookahead_Stream (ran, ran[LOOKAHEAD_WIN-1]);

    Fill_Outbuf(ran, outbuf0, LOOKAHEAD_WIN, tabsize);
    Init_Push_Updates(outbuf0, outgoing_mailboxes);
        
    for (i=1; i<size/LOOKAHEAD_WIN; i++)
    {

      Fill_Lookahead_Stream (ran, ran[LOOKAHEAD_WIN-1]);
      if(outbuf == outbuf0)
	outbuf = outbuf1;
      else 
	outbuf = outbuf0;
      Fill_Outbuf(ran, outbuf, LOOKAHEAD_WIN, tabsize);
      Finish_Push_Updates(outgoing_mailboxes);
      Init_Push_Updates(outbuf, outgoing_mailboxes);
      Process_Incoming_Updates(incoming_mailboxes, tabsize);
    }

   
    //    Finish_Push_Updates(outgoing_mailboxes);
    //Process_Incoming_Updates(incoming_mailboxes, tabsize);

    upc_barrier;
   
    
    return 0;
    
    

}



/* UPC shared arrays layout sucks.
   There's no way to declare multi-dimensional data (with THREADS)  which is
   exactly what is needed here.
  
   Anyway - using bupc_ptradd might eliminate the need for local_allocs and 
   all-to-all communication.
*/
void Setup_Data(uint64 tabsize) 
{

  int Update_Win;
  shared [] uint64* local_arena;
  int i,j, idx;
  uint64 *lptr;
  shared WinPtr *sdata, *wdata;
  
  
  /* incoming mboxes have NBOXES windows associated
     outgoing mboxes have only one window associated as requested by 
     the HPCC requirements
  */
  /* Layout:
     THREADS  mailboxes, followed by the space for each update window (NBOXES per mbox),
     followed by THREADS outboxes and 2 update windows for comm buffers
  */ 
  
  
  Update_Win = LOOKAHEAD_WIN*sizeof(uint64) + CACHE_LINE_PAD;
  incoming_mailboxes = (MailBox*)calloc(THREADS, sizeof(MailBox));
  sdata = (shared WinPtr*)upc_all_alloc(THREADS*THREADS*NBOXES, sizeof(WinPtr));
  
  if(!incoming_mailboxes || !sdata) {
    printf("Out of memory\n");
    upc_global_exit(-1);
  }
  wdata = sdata + MYTHREAD*THREADS*NBOXES;
  for(i = 0; i < THREADS; i++)
    for(j = 0; j < NBOXES; j++) {
      local_arena = (shared [] uint64*)upc_alloc(Update_Win);
      if(!local_arena) {
	printf("Out of memory\n");
	upc_global_exit(-1);
      }
      incoming_mailboxes[i].mbox[j] = local_arena;
      lptr = (uint64*)local_arena;
      memset(lptr, 0, Update_Win);
      *wdata = local_arena;
      wdata ++; 
    }

  outbuf0 = (uint64*)upc_alloc(Update_Win);
  if(!outbuf0) {
	printf("Out of memory\n");
	upc_global_exit(-1);
  }
  memset(outbuf0, 0, Update_Win);
  outbuf1 = (uint64*)upc_alloc(Update_Win);
  if(!outbuf1) {
    printf("Out of memory\n");
    upc_global_exit(-1);
  }
  memset(outbuf1, 0, Update_Win);
  
  outgoing_mailboxes = (MailBox*)calloc(THREADS, sizeof(MailBox));
  table = (shared uint64*) upc_all_alloc(tabsize*sizeof(uint64), 1);
  
  if(!table) {
    printf("Out of memory\n");
    upc_global_exit(-1);
  }
  
  for(i = 0; i < THREADS; i++) {
    wdata = sdata + i*THREADS*NBOXES + MYTHREAD*NBOXES;;
    for(j = 0; j < NBOXES; j++) {
      outgoing_mailboxes[i].mbox[j] = *wdata;
      wdata++;
    }
  }
  handle[MYTHREAD] = BUPC_COMPLETE_HANDLE;
}

int
RandomAccess(HPCC_Params *params, int doIO, double *GUPs, int *failure)
{

  uint64 totalMem, logTableSize, TableSize, temp, errs;
  FILE *outFile;
  int i;
  double cputime, realtime;
  if(MYTHREAD == 0)
    {
      if (doIO)
        {
#if 0
          outFile = fopen( params->outFname, "a" );
#else
          outFile = stdout;
#endif
          if (! outFile)
            {
              outFile = stderr;
              fprintf( outFile, "Cannot open output file.\n" );
              return 1;
            }
        }
      
      /* calculate local memory per node for the update table */
      totalMem = params->HPLMaxProcMem;
      totalMem /= sizeof(uint64);
      
      /* calculate the size of update array (must be a power of 2) */
      for (totalMem *= 0.5, logTableSize = 0, TableSize = 1;
           totalMem >= 1.0;
           totalMem *= 0.5, logTableSize++, TableSize <<= 1)
        ; /* EMPTY */
      
      
      tabsize = TableSize;
      logtabsize = logTableSize;
     
      
      /* Print parameters for run */
      if (doIO)
        {
          fprintf( outFile, "Running on %d processors\n", THREADS );
          fprintf( outFile, "Main table size   = 2^%llu = %llu words\n", 
                   (unsigned long long)logTableSize,(unsigned long long)TableSize);
          fprintf( outFile, "Subst table size  = 2^%d = %d words\n", LSTSIZE, STSIZE);
          fprintf( outFile, "Number of updates = %llu\n", (unsigned long long)(4*TableSize));
        }
      
      temp = params->HPLMaxProcMem;
      
    }
  
  upc_barrier;

  Setup_Data(tabsize);
  
  Stable[0] = 0;
  for (i=1; i<STSIZE; i++)
    Stable[i] = Stable[i-1] +
#ifdef LONG_IS_64BITS
      0x0123456789abcdefL;
#else
  0x0123456789abcdefLL;
#endif
  
  /* Begin timing here */
  cputime = -CPUSEC();
  realtime = -RTSEC();
  
  errs = update_table(logtabsize, tabsize);
  
  /* End timed section */
  cputime += CPUSEC();
  realtime += RTSEC();

  if(MYTHREAD==0)
    {
      /* make sure no division by zero */
      *GUPs = (realtime > 0.0 ? 1.0 / realtime : -1.0);
      *GUPs *= 1e-9*(4*tabsize);
      
      /* Print timing results */
      if (doIO)
	{
	  fprintf( outFile, "CPU time used  = %.6f seconds\n", cputime);
	  fprintf( outFile, "Real time used = %.6f seconds\n", realtime);
	  fprintf( outFile, "%.9f Billion(10^9) Updates    per second [GUP/s]\n", *GUPs );
	}
    }
  
  errs = Verify(4*tabsize, tabsize);

  if(MYTHREAD == 0) {
    if (doIO)
      {
	fprintf( outFile, "Found %llu errors in %llu locations (%s).\n",
		 (unsigned long long)errs, (unsigned long long)tabsize, (errs <= 0.01*tabsize) ? "passed" : "failed");
      }
    
    if (errs <= 0.01*tabsize)
      *failure = 0;
    else
      *failure = 1;
    
    if (doIO)
      {
	fflush( outFile );
#if 0
	fclose( outFile );
#endif
      }
  }
  return *failure;
}
int main(int argc, char **argv )
{
  double d_gups;
  int r, i_failure;
  HPCC_Params p;
  int pow2_size;
  uint64 errs;
  if( argc == 2 )
    {
     
      sscanf( argv[1], "%d", &pow2_size );
      for( r=0, p.HPLMaxProcMem=sizeof(u64Int); r<pow2_size; r++, p.HPLMaxProcMem <<= 1 );
    }
  else
    p.HPLMaxProcMem = 16*1024*1024;

  strcpy( p.outFname, "output.txt" );
  count_phases = 0;
  r = RandomAccess( &p, 1, &d_gups, &i_failure );

  return r;
}






