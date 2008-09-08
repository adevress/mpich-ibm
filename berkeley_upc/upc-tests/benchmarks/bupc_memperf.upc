/*    $Source: /var/local/cvs/upcr/upc-tests/benchmarks/bupc_memperf.upc,v $ */
/*      $Date: 2005/03/13 06:33:30 $ */
/*  $Revision: 1.1 $ */
/*  Description: UPC memcpy extensions performance microbenchmark */
/*  Copyright 2005, Dan Bonachea <bonachea@cs.berkeley.edu> */

#include <upc.h>
#include <stdio.h>
#include <string.h>

/* usage: bupc_memperf (<iters> (<maxsz> ([ABGPFL]))) 
   A = _async operations
   B = blocking operations
   P = puts
   G = gets
   F = Flood test
   L = Latency test
 */

int main(int argc, char **argv) {
  int iters=0, maxsz=0;
  int doasync=0, doblocking=0, doput=0, doget=0, doflood=0, dolatency=0;
  const char *optype = "ABPGFL";
  int peerid = (MYTHREAD+1)%THREADS, iamsender = !(MYTHREAD&0x1);
  if (argc > 1) iters = atoi(argv[1]);
  if (!iters) iters = 1000;
  if (argc > 2) maxsz = atoi(argv[2]);
  if (!maxsz) maxsz=2*1048576;
  if (argc > 3) optype = argv[3];
  if (strchr(optype,'A') || strchr(optype,'a')) doasync = 1;
  if (strchr(optype,'B') || strchr(optype,'b')) doblocking = 1;
  if (!doasync && !doblocking) { doasync = 1; doblocking = 1; }
  if (strchr(optype,'P') || strchr(optype,'p')) doput = 1;
  if (strchr(optype,'G') || strchr(optype,'g')) doget = 1;
  if (!doput && !doget) { doput = 1; doget = 1; }
  if (strchr(optype,'F') || strchr(optype,'f')) doflood = 1;
  if (strchr(optype,'L') || strchr(optype,'l')) dolatency = 1;
  if (!doflood && !dolatency) { doflood = 1; dolatency = 1; }

  shared char *data = upc_all_alloc(THREADS, maxsz*2);
  shared [] char *remote = (shared [] char *)(data + peerid);
  char *local = ((char *)(data+MYTHREAD))+maxsz;
  bupc_handle_t *handles = malloc(iters*sizeof(bupc_handle_t));

  if (!MYTHREAD) printf("Timer granularity: <= %.3f us, overhead: ~ %.3f us\n",
       bupc_tick_granularityus(), bupc_tick_overheadus()); fflush(stdout);

  #define LATENCYTEST(desc, op, numiters, datasz, report) do {            \
    if (iamsender) {                                                      \
      bupc_tick_t start = bupc_ticks_now();                               \
        for (int i=0; i < numiters; i++) {                                \
          op;                                                             \
        }                                                                 \
      upc_barrier;                                                        \
      if (report) {                                                       \
        double secs = bupc_ticks_to_us(bupc_ticks_now()-start)/1000000.0; \
        double latencyus = secs*1000000.0/numiters;                       \
        printf("%3i: %10i byte, %11.6f secs: %11.3f us/iter (%s)\n",      \
                MYTHREAD, datasz, secs, latencyus, desc);                 \
        fflush(stdout);                                                   \
      }                                                                   \
    } else upc_barrier;                                                   \
  } while (0)

  upc_barrier;
  if (dolatency) {
    if (!MYTHREAD) printf("Round-trip latency test iters=%i maxsz=%i\n",iters,maxsz); fflush(stdout);

    /* operation warm-up */
    LATENCYTEST("upc_memget", upc_memget(local, remote, 8), iters, 8, 0);
    LATENCYTEST("upc_memput", upc_memput(remote, local, 8), iters, 8, 0);
    LATENCYTEST("bupc_memget_async", bupc_waitsync(bupc_memget_async(local, remote, 8)), iters, 8, 0);
    LATENCYTEST("bupc_memput_async", bupc_waitsync(bupc_memput_async(remote, local, 8)), iters, 8, 0);
    for (int sz = 1; sz <= maxsz; sz*=2) {
      upc_barrier;
      /* per-size warm-up */
      LATENCYTEST("upc_memget", upc_memget(local, remote, sz), 1, sz, 0);
      LATENCYTEST("upc_memput", upc_memput(remote, local, sz), 1, sz, 0);
      LATENCYTEST("bupc_memget_async", bupc_waitsync(bupc_memget_async(local, remote, sz)), 1, sz, 0);
      LATENCYTEST("bupc_memput_async", bupc_waitsync(bupc_memput_async(remote, local, sz)), 1, sz, 0);
      upc_barrier;
      if (doblocking && doget) 
        LATENCYTEST("upc_memget", upc_memget(local, remote, sz), iters, sz, 1);
      upc_barrier;
      if (doasync && doget) 
        LATENCYTEST("bupc_memget_async", bupc_waitsync(bupc_memget_async(local, remote, sz)), iters, sz, 1);
      upc_barrier;
      if (doblocking && doput) 
        LATENCYTEST("upc_memput", upc_memput(remote, local, sz), iters, sz, 1);
      upc_barrier;
      if (doasync && doput) 
        LATENCYTEST("bupc_memput_async", bupc_waitsync(bupc_memput_async(remote, local, sz)), iters, sz, 1);
    }
  }

  #define FLOODTEST(desc, op, numiters, datasz, report, reap) do {            \
    if (iamsender) {                                                          \
      bupc_tick_t start = bupc_ticks_now();                                   \
        for (int i=0; i < numiters; i++) {                                    \
          op;                                                                 \
        }                                                                     \
        if (reap) for (int i=0; i < numiters; i++) bupc_waitsync(handles[i]); \
      upc_barrier;                                                            \
      if (report) {                                                           \
        double secs = bupc_ticks_to_us(bupc_ticks_now()-start)/1000000.0;     \
        double bwKB = (((double)datasz) * numiters / 1024.0) / secs;          \
        printf("%3i: %10i byte, %11.6f secs: %11.3f KB/sec (%s)\n",           \
                MYTHREAD, datasz, secs, bwKB, desc);                          \
        fflush(stdout);                                                       \
      }                                                                       \
    } else upc_barrier;                                                       \
  } while (0)

  upc_barrier;
  if (doflood) {
    if (!MYTHREAD) printf("Flood bandwidth test iters=%i maxsz=%i\n",iters,maxsz); fflush(stdout);

    /* operation warm-up */
    FLOODTEST("upc_memget", upc_memget(local, remote, 8), iters, 8, 0, 0);
    FLOODTEST("upc_memput", upc_memput(remote, local, 8), iters, 8, 0, 0);
    FLOODTEST("bupc_memget_async", handles[i] = bupc_memget_async(local, remote, 8), iters, 8, 0, 1);
    FLOODTEST("bupc_memput_async", handles[i] = bupc_memput_async(remote, local, 8), iters, 8, 0, 1);
    for (int sz = 1; sz <= maxsz; sz*=2) {
      upc_barrier;
      /* per-size warm-up */
      FLOODTEST("upc_memget", upc_memget(local, remote, sz), 1, sz, 0, 0);
      FLOODTEST("upc_memput", upc_memput(remote, local, sz), 1, sz, 0, 0);
      FLOODTEST("bupc_memget_async", handles[i] = bupc_memget_async(local, remote, sz), 1, sz, 0, 1);
      FLOODTEST("bupc_memput_async", handles[i] = bupc_memput_async(remote, local, sz), 1, sz, 0, 1);
      upc_barrier;
      if (doblocking && doget) 
        FLOODTEST("upc_memget", upc_memget(local, remote, sz), iters, sz, 1, 0);
      upc_barrier;
      if (doasync && doget) 
        FLOODTEST("bupc_memget_async", handles[i] = bupc_memget_async(local, remote, sz), iters, sz, 1, 1);
      upc_barrier;
      if (doblocking && doput) 
        FLOODTEST("upc_memput", upc_memput(remote, local, sz), iters, sz, 1, 0);
      upc_barrier;
      if (doasync && doput) 
        FLOODTEST("bupc_memput_async", handles[i] = bupc_memput_async(remote, local, sz), iters, sz, 1, 1);
    }
  }

  upc_barrier;
  if (!MYTHREAD) printf("done.\n");
  return 0;
}
