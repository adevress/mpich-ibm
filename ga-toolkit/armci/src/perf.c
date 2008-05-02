/* 
 *    Author: Jialin Ju, PNNL
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <math.h>
#include "mp3.h"
#include "armci.h"

#define SIZE 550
#define MAXPROC 8
#define CHUNK_NUM 28
#define FORCE_1D_

#ifndef ABS
#define ABS(a) ((a)>0? (a): -(a))
#endif

/* tells to use ARMCI_Malloc_local instead of plain malloc */
#define MALLOC_LOC 

/*
#define FORCE_DCMF_ADVANCE
*/
#define FORCE_DCMF_ADVANCE_LOOP 1000000
#ifdef FORCE_DCMF_ADVANCE
#include <dcmf.h>
#endif


/*
#define TEST_DCMF
*/
#ifdef TEST_DCMF
#include <dcmf.h>
#include <string.h>

#warning Compile DCMF version of the ARMCI performance test.
/*********************************************************************************************/
/* This memregion exchange code was originally copied from armci/src/x/dcmf/armcix_impl.[ch] */
/*********************************************************************************************/

typedef struct dcmf_connection_t
{
  DCMF_Memregion_t local_mem_region;
  DCMF_Memregion_t remote_mem_region;
  unsigned peer;
  unsigned unused0;
  unsigned unused1;
  unsigned unused3;
}
dcmf_connection_t __attribute__ ((__aligned__ (16)));

dcmf_connection_t * __connection;
DCMF_Memregion_t __local_mem_region;
DCMF_Protocol_t __dcmf_get_protocol;
DCMF_Protocol_t __dcmf_put_protocol;

/**
 * \see DCMF_Callback_t
 */
void dcmf_cb_decrement (void * clientdata)
{
  unsigned * value = (unsigned *) clientdata;
  (*value)--;
}

/**
 * \see DCMF_RecvSend
 */
void dcmf_RecvMemregion1 (void           * clientdata,
                          const DCQuad   * msginfo,
                          unsigned         count,
                          unsigned         peer,
                          const char     * src,
                          unsigned         bytes)
{
  dcmf_connection_t * connection = (dcmf_connection_t *) clientdata;
  memcpy (&connection[peer].remote_mem_region, src, bytes);
}


/**
 * \see DCMF_RecvSend
 */
DCMF_Request_t * dcmf_RecvMemregion2 (void             * clientdata,
                                             const DCQuad     * msginfo,
                                             unsigned           count,
                                             unsigned           peer,
                                             unsigned           sndlen,
                                             unsigned         * rcvlen,
                                             char            ** rcvbuf,
                                             DCMF_Callback_t  * cb_done)
{
  dcmf_connection_t * connection = (dcmf_connection_t *) clientdata;

  *rcvlen = sndlen;
  *rcvbuf = (char *) &connection[peer].remote_mem_region;

  cb_done->function   = free;
  cb_done->clientdata = (void *) malloc (sizeof (DCMF_Request_t));

  return cb_done->clientdata;
}

void dcmf_connection_initialize ()
{
  DCMF_Result result;

  /* Register a Get protocol to test */
  DCMF_Get_Configuration_t get_config;
  get_config.protocol = DCMF_DEFAULT_GET_PROTOCOL;
  result = DCMF_Get_register (&__dcmf_get_protocol, &get_config);

  /* Register a Put protocol to test */
  DCMF_Put_Configuration_t put_config;
  put_config.protocol = DCMF_DEFAULT_PUT_PROTOCOL;
  result = DCMF_Put_register (&__dcmf_put_protocol, &put_config);

  unsigned rank = DCMF_Messager_rank ();
  unsigned size = DCMF_Messager_size ();
  posix_memalign ((void **)&__connection, 16, sizeof(dcmf_connection_t) * size);
  memset ((void *)__connection, 0, sizeof(dcmf_connection_t) * size);

  void * base  = NULL;
  size_t bytes = (size_t) -1;

  unsigned i;
  for (i = 0; i < size; i++)
  {
    __connection[i].peer = i;
/*#warning fix memregion setup to handle non-global address space pinning.
    //DCMF_Result result =*/
      DCMF_Memregion_create (&__connection[i].local_mem_region,
                             &bytes, (size_t) -1, (void *) 0x00, 0);
  }

  DCMF_CriticalSection_enter(0);

  /* Register a send protocol to exchange memory regions
     Normally this shouldn't be on the stack, but since it will only be used
     once it is ok ... just this once. */
  DCMF_Protocol_t send_protocol;
  DCMF_Send_Configuration_t send_configuration = {
    DCMF_DEFAULT_SEND_PROTOCOL,
    dcmf_RecvMemregion1,
    __connection,
    dcmf_RecvMemregion2,
    __connection
  };
  DCMF_Send_register (&send_protocol, &send_configuration);

  DCMF_Request_t request;
  volatile unsigned active;
  DCMF_Callback_t cb_done = { (void (*)(void*)) dcmf_cb_decrement, (void *) &active };

  /* Exchange the memory regions */
  for (i = 0; i < size; i++)
  {
    unsigned peer = (rank+i)%size;
    active = 1;
    DCMF_Send (&send_protocol,
               &request,
               cb_done,
               DCMF_SEQUENTIAL_CONSISTENCY,
               peer,
               sizeof(DCMF_Memregion_t),
               (char *) &__connection[peer].local_mem_region,
               (DCQuad *) NULL,
               0);
    while (active) DCMF_Messager_advance();
  }

  DCMF_CriticalSection_exit(0);
}

#endif

int CHECK_RESULT=0;

int chunk[CHUNK_NUM] = {1,3,4,6,9,12,16,20,24,30,40,48,52,64,78,91,104,
                        128,142,171,210,256,300,353,400,440,476,512};

char check_type[15];
int nproc, me;
int warn_accuracy=0;

void fill_array(double *arr, int count, int which);
void check_result(double *src_buf, double *dst_buf, int *stride, int *count,
                  int stride_levels);
void acc_array(double scale, double *array1, double *array2, int *stride,
               int *count, int stride_levels);


static double _tt0=0.0;
/*\ quick fix for inacurate timer
\*/
double Timer()
{
#define DELTA 0.000001
  double t=MP_TIMER();
  if(t<=_tt0 + DELTA) _tt0 += DELTA;
  else _tt0 = t;
  return _tt0;
}

#define TIMER MP_TIMER


double time_get(double *src_buf, double *dst_buf, int chunk, int loop,
                int proc, int levels)
{
    int i, bal = 0;
    
    int stride[2];
    int count[2];
    int stride_levels = levels;
    double *tmp_buf, *tmp_buf_ptr;
    
    double start_time, stop_time, total_time = 0;

    stride[0] = SIZE * sizeof(double);
    count[0] = chunk * sizeof(double); count[1] = chunk;

    if(CHECK_RESULT) {
        tmp_buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(tmp_buf != NULL);

        fill_array(tmp_buf, SIZE*SIZE, proc);
        tmp_buf_ptr = tmp_buf;
    }
    
#ifdef TEST_DCMF
    volatile unsigned dcmf_get_active;
    DCMF_Request_t dcmf_request;
    DCMF_Callback_t cb_done = (DCMF_Callback_t){dcmf_cb_decrement, (void *)&dcmf_get_active};
    DCMF_Memregion_t * src_memregion = &__connection[proc].remote_mem_region;
    DCMF_Memregion_t * dst_memregion = &__connection[proc].local_mem_region;
    void * src_base;
    void * dst_base;
    size_t bytes;
    DCMF_Memregion_query (src_memregion, &bytes, &src_base);
    DCMF_Memregion_query (dst_memregion, &bytes, &dst_base);
    size_t src_offset;
    size_t dst_offset;

    /* enter cs outside of time loop */
    DCMF_CriticalSection_enter (0);
#endif
    
    
    start_time = TIMER();
    for(i=0; i<loop; i++) {
         
#ifdef FORCE_1D
        int j;
        if(levels>0)for(j=0; j< count[1]; j++){
           char *s = (char*) src_buf, *d= (char*)dst_buf;
           s += j*stride[0]; d += j*stride[0];
           ARMCI_Get(src_buf, dst_buf, count[0],proc);
        }
        else
#endif
        if(levels)
#ifdef TEST_DCMF
           assert(0);
#else
           ARMCI_GetS(src_buf, stride, dst_buf, stride, count, stride_levels,proc);
#endif
        else
#ifdef TEST_DCMF
        {
           src_offset = (size_t)src_buf - (size_t)src_base;
           dst_offset = (size_t)dst_buf - (size_t)dst_base;
           dcmf_get_active = 1;
/*
size_t tmp_src_bytes;
void * tmp_src_base;
DCMF_Memregion_query (src_memregion, &tmp_src_bytes, &tmp_src_base);
fprintf (stderr, "before DCMF_Get() - src_memregion = %p, src_memregion->base = %p, src_memregion->bytes = %d, src_offset = %d\n", src_memregion, tmp_src_base, tmp_src_bytes, 
size_t tmp_dst_bytes;
void * tmp_dst_base;
DCMF_Memregion_query (dst_memregion, &tmp_dst_bytes, &tmp_dst_base);
*/
           DCMF_Get (&__dcmf_get_protocol,
                     &dcmf_request,
                     cb_done,
                     DCMF_SEQUENTIAL_CONSISTENCY,
                     proc,
                     (size_t) count[0],
                     src_memregion,
                     dst_memregion,
                     src_offset,
                     dst_offset);
           while (dcmf_get_active) DCMF_Messager_advance ();
         }
#else
           ARMCI_Get(src_buf, dst_buf,count[0], proc);
#endif

        if(CHECK_RESULT) {
            sprintf(check_type, "ARMCI_GetS:");
            check_result(tmp_buf_ptr, dst_buf, stride, count, stride_levels);
        }
        
        /* prepare next src and dst ptrs: avoid cache locality */
        if(bal == 0) {
            src_buf += 128;
            dst_buf += 128;
            if(CHECK_RESULT) tmp_buf_ptr += 128;
            bal = 1;
        } else {
            src_buf -= 128;
            dst_buf -= 128;
            if(CHECK_RESULT) tmp_buf_ptr -= 128;
            bal = 0;
        }
    }
    stop_time = TIMER();
    total_time = (stop_time - start_time);

#ifdef TEST_DCMF
    /* exit cs outside of time loop */
    DCMF_CriticalSection_exit (0);
#endif

    if(CHECK_RESULT) free(tmp_buf);

    if(total_time == 0.0){
       total_time=0.000001; /* workaround for inaccurate timers */
       warn_accuracy++;
    }
    return(total_time/loop);
}

double time_put(double *src_buf, double *dst_buf, int chunk, int loop,
                int proc, int levels)
{
    int i, bal = 0;

    int stride[2];
    int count[2];
    int stride_levels = levels;
    double *tmp_buf;

    double start_time, stop_time, total_time = 0;

    stride[0] = SIZE * sizeof(double);
    count[0] = chunk * sizeof(double); count[1] = chunk;

    if(CHECK_RESULT) {
        tmp_buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(tmp_buf != NULL);
    }
    
#ifdef TEST_DCMF
    volatile unsigned dcmf_put_active;
    DCMF_Request_t dcmf_request;
    DCMF_Callback_t cb_done = (DCMF_Callback_t){dcmf_cb_decrement, (void *)&dcmf_put_active};
    DCMF_Memregion_t * src_memregion = &__connection[proc].local_mem_region;
    DCMF_Memregion_t * dst_memregion = &__connection[proc].remote_mem_region;
    void * src_base;
    void * dst_base;
    size_t bytes;
    DCMF_Memregion_query (src_memregion, &bytes, &src_base);
    DCMF_Memregion_query (dst_memregion, &bytes, &dst_base);
    size_t src_offset;
    size_t dst_offset;

    /* enter cs outside of time loop */
    DCMF_CriticalSection_enter (0);
#endif

    start_time = TIMER();
    for(i=0; i<loop; i++) {

#ifdef FORCE_1D
        int j;
        if(levels>0)for(j=0; j< count[1]; j++){
           char *s = (char*) src_buf, *d= (char*)dst_buf;
           s += j*stride[0]; d += j*stride[0];
           ARMCI_Put(src_buf, dst_buf, count[0],proc);
        }
        else
#endif
        if(levels)
#ifdef TEST_DCMF
           assert(0);
#else
           ARMCI_PutS(src_buf, stride, dst_buf, stride, count, stride_levels,proc);
#endif
        else
#ifdef TEST_DCMF
        {
           src_offset = (size_t)src_buf - (size_t)src_base;
           dst_offset = (size_t)dst_buf - (size_t)dst_base;
           dcmf_put_active = 1;

           DCMF_Put (&__dcmf_get_protocol,
                     &dcmf_request,
                     cb_done,
                     DCMF_SEQUENTIAL_CONSISTENCY,
                     proc,
                     (size_t) count[0],
                     src_memregion,
                     dst_memregion,
                     src_offset,
                     dst_offset);
           while (dcmf_put_active) DCMF_Messager_advance ();
        }
#else
           ARMCI_Put(src_buf, dst_buf,count[0], proc);
#endif
#ifndef TEST_DCMF
        if(CHECK_RESULT) {
            ARMCI_GetS(dst_buf, stride, tmp_buf, stride, count,
                       stride_levels, proc);

            sprintf(check_type, "ARMCI_PutS:");
            check_result(tmp_buf, src_buf, stride, count, stride_levels);
        }
#endif
        
        /* prepare next src and dst ptrs: avoid cache locality */
        if(bal == 0) {
            src_buf += 128;
            dst_buf += 128;
            bal = 1;
        } else {
            src_buf -= 128;
            dst_buf -= 128;
            bal = 0;
        }
    }
    stop_time = TIMER();
    total_time = (stop_time - start_time);

#ifdef TEST_DCMF
    /* exit cs outside of time loop */
    DCMF_CriticalSection_exit (0);
#endif

    if(CHECK_RESULT) free(tmp_buf);
    
    if(total_time == 0.0){ 
       total_time=0.000001; /* workaround for inaccurate timers */
       warn_accuracy++;
    }
    return(total_time/loop);
}

double time_acc(double *src_buf, double *dst_buf, int chunk, int loop,
                int proc, int levels)
{
    int i, bal = 0;

    int stride[2];
    int count[2];
    int stride_levels = levels;
    double *before_buf, *after_buf;
    
    double start_time, stop_time, total_time = 0;

    stride[0] = SIZE * sizeof(double);
    count[0] = chunk * sizeof(double); count[1] = chunk;

    if(CHECK_RESULT) {
        before_buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(before_buf != NULL);
        after_buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(after_buf != NULL);
    }
    
    start_time = TIMER();
    for(i=0; i<loop; i++) {
        double scale = (double)i;

        if(CHECK_RESULT) {
            ARMCI_GetS(dst_buf, stride, before_buf, stride, count,
                       stride_levels, proc);

            acc_array(scale, before_buf, src_buf, stride, count,stride_levels);
        }

        ARMCI_AccS(ARMCI_ACC_DBL, &scale, src_buf, stride, dst_buf, stride,
                   count, stride_levels, proc);

        if(CHECK_RESULT) {
            ARMCI_GetS(dst_buf, stride, after_buf, stride, count,
                       stride_levels, proc);
            
            sprintf(check_type, "ARMCI_AccS:");
            check_result(after_buf, before_buf, stride, count, stride_levels);
        }
        
        /* prepare next src and dst ptrs: avoid cache locality */
        if(bal == 0) {
            src_buf += 128;
            dst_buf += 128;
            bal = 1;
        } else {
            src_buf -= 128;
            dst_buf -= 128;
            bal = 0;
        }
    }
    stop_time = TIMER();
    total_time = (stop_time - start_time);

    if(CHECK_RESULT) { free(before_buf); free(after_buf); }
    
    if(total_time == 0.0){ 
       total_time=0.000001; /* workaround for inaccurate timers */
       warn_accuracy++;
    }
    return(total_time/loop);
}

void test_1D()
{
    int i;
    int src, dst;
    int ierr;
    double *buf;
    void *ptr[MAXPROC], *get_ptr[MAXPROC];

    /* find who I am and the dst process */
    src = me;
    
    /* memory allocation */
#ifdef MALLOC_LOC 
    if(me == 0) {
        buf = (double *)ARMCI_Malloc_local(SIZE * SIZE * sizeof(double));
        assert(buf != NULL);
    }
#else
    if(me == 0) {
        buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(buf != NULL);
    }
#endif
    
    ierr = ARMCI_Malloc(ptr, (SIZE * SIZE * sizeof(double)));
    assert(ierr == 0); assert(ptr[me]);
    ierr = ARMCI_Malloc(get_ptr, (SIZE * SIZE * sizeof(double)));
    assert(ierr == 0); assert(get_ptr[me]);

    /* ARMCI - initialize the data window */
    fill_array(ptr[me], SIZE*SIZE, me);
    fill_array(get_ptr[me], SIZE*SIZE, me);
    MP_BARRIER();
    
    /* only the proc 0 does the work */
    if(me == 0) {
        if(!CHECK_RESULT){
          printf("  section               get                 put");
          printf("                 acc\n");
          printf("bytes   loop       sec      MB/s       sec      MB/s");
          printf("       sec      MB/s\n");
          printf("------- ------  --------  --------  --------  --------");
          printf("  --------  --------\n");
          fflush(stdout);
        }
        
        for(i=0; i<CHUNK_NUM; i++) {
            int loop;
            int bytes = chunk[i] * chunk[i] * sizeof(double);
            
            double t_get = 0, t_put = 0, t_acc = 0;
            double latency_get, latency_put, latency_acc;
            double bandwidth_get, bandwidth_put, bandwidth_acc;
            
            loop = (SIZE * SIZE) / (chunk[i] * chunk[i]);
            loop = (int)sqrt((double)loop);
            if(loop<2)loop=2;
            
            for(dst=1; dst<nproc; dst++) {
                /* strided get */
                fill_array(buf, SIZE*SIZE, me*10);
                t_get += time_get((double *)(get_ptr[dst]), (double *)buf,
                                  chunk[i]*chunk[i], loop, dst, 0);
                
                /* strided put */
                fill_array(buf, SIZE*SIZE, me*10);
                t_put += time_put((double *)buf, (double *)(ptr[dst]),
                                  chunk[i]*chunk[i], loop, dst, 0);
                
#ifndef TEST_DCMF
                /* strided acc */
                fill_array(buf, SIZE*SIZE, me*10);
                t_acc += time_acc((double *)buf, (double *)(ptr[dst]),
                                  chunk[i]*chunk[i], loop, dst, 0);
#endif
            }
            
            latency_get = t_get/(nproc - 1);
            latency_put = t_put/(nproc - 1);
            latency_acc = t_acc/(nproc - 1);
            
            bandwidth_get = (bytes * (nproc - 1) * 1e-6)/t_get;
            bandwidth_put = (bytes * (nproc - 1) * 1e-6)/t_put;
            bandwidth_acc = (bytes * (nproc - 1) * 1e-6)/t_acc;

            /* print */
            if(!CHECK_RESULT)printf("%d\t%d\t%.2e  %.2e  %.2e  %.2e  %.2e  %.2e\n",
                   bytes, loop, latency_get, bandwidth_get,
                   latency_put, bandwidth_put, latency_acc, bandwidth_acc);
        }
    }
    else
#ifndef FORCE_DCMF_ADVANCE
      sleep(3);
#else
    {
      unsigned i = FORCE_DCMF_ADVANCE_LOOP;
      DCMF_CriticalSection_enter (0);
      while (i--) DCMF_Messager_advance ();
      DCMF_CriticalSection_exit (0);
    }
#endif
    
    ARMCI_AllFence();
    MP_BARRIER();
    
    /* cleanup */
    ARMCI_Free(get_ptr[me]);
    ARMCI_Free(ptr[me]);
    
#ifdef MALLOC_LOC 
    /*if(me == 0) ARMCI_Free_local(buf);*/
#else
    if(me == 0) free(buf);
#endif
}

void test_2D()
{
    int i;
    int src, dst;
    int ierr;
    double *buf;
    void *ptr[MAXPROC], *get_ptr[MAXPROC];

    /* find who I am and the dst process */
    src = me;
    
#ifdef MALLOC_LOC
    if(me == 0) {
        buf = (double *)ARMCI_Malloc_local(SIZE * SIZE * sizeof(double));
        assert(buf != NULL);
    }
#else
    if(me == 0) {
        buf = (double *)malloc(SIZE * SIZE * sizeof(double));
        assert(buf != NULL);
    }
#endif

    ierr = ARMCI_Malloc(ptr, (SIZE * SIZE * sizeof(double)));
    assert(ierr == 0); assert(ptr[me]);
    ierr = ARMCI_Malloc(get_ptr, (SIZE * SIZE * sizeof(double)));
    assert(ierr == 0); assert(get_ptr[me]);
    
    /* ARMCI - initialize the data window */
    fill_array(ptr[me], SIZE*SIZE, me);
    fill_array(get_ptr[me], SIZE*SIZE, me);

    MP_BARRIER();
    
    /* only the proc 0 doest the work */
    /* print the title */
    if(me == 0) {
        if(!CHECK_RESULT){
           printf("  section               get                 put");
           printf("                 acc\n");
           printf("bytes   loop       sec      MB/s       sec      MB/s");
           printf("       sec      MB/s\n");
           printf("------- ------  --------  --------  --------  --------");
           printf("  --------  --------\n");
           fflush(stdout);
        }
        
        for(i=0; i<CHUNK_NUM; i++) {
            int loop;
            int bytes = chunk[i] * chunk[i] * sizeof(double);

            double t_get = 0, t_put = 0, t_acc = 0;
            double latency_get, latency_put, latency_acc;
            double bandwidth_get, bandwidth_put, bandwidth_acc;
            
            loop = SIZE / chunk[i];
            if(loop<2)loop=2;

            for(dst=1; dst<nproc; dst++) {
                /* strided get */
                fill_array(buf, SIZE*SIZE, me*10);
                t_get += time_get((double *)(get_ptr[dst]), (double *)buf,
                                 chunk[i], loop, dst, 1);
 
                /* strided put */
                fill_array(buf, SIZE*SIZE, me*10);
                t_put += time_put((double *)buf, (double *)(ptr[dst]),
                                 chunk[i], loop, dst, 1);
                
                /* strided acc */
                fill_array(buf, SIZE*SIZE, me*10);
                t_acc += time_acc((double *)buf, (double *)(ptr[dst]),
                                 chunk[i], loop, dst, 1);
            }
            
            latency_get = t_get/(nproc - 1);
            latency_put = t_put/(nproc - 1);
            latency_acc = t_acc/(nproc - 1);
            
            bandwidth_get = (bytes * (nproc - 1) * 1e-6)/t_get;
            bandwidth_put = (bytes * (nproc - 1) * 1e-6)/t_put;
            bandwidth_acc = (bytes * (nproc - 1) * 1e-6)/t_acc;

            /* print */
            if(!CHECK_RESULT)printf("%d\t%d\t%.2e  %.2e  %.2e  %.2e  %.2e  %.2e\n",
                       bytes, loop, latency_get, bandwidth_get,
                       latency_put, bandwidth_put, latency_acc, bandwidth_acc);
        }
    }
    else
#ifndef FORCE_DCMF_ADVANCE
      sleep(3);
#else
    {
      unsigned i = FORCE_DCMF_ADVANCE_LOOP;
      DCMF_CriticalSection_enter (0);
      while (i--) DCMF_Messager_advance ();
      DCMF_CriticalSection_exit (0);
    }
#endif
    
    ARMCI_AllFence();
    MP_BARRIER();

    /* cleanup */
    ARMCI_Free(get_ptr[me]);
    ARMCI_Free(ptr[me]);

#ifdef MALLOC_LOC
    if(me == 0) ARMCI_Free_local(buf);
#else
    if(me == 0) free(buf);
#endif

}

    
int main(int argc, char **argv)
{

  MP_INIT(argc,argv);
  MP_MYID(&me);
  MP_PROCS(&nproc);

    if(nproc < 2 || nproc> MAXPROC) {
        if(me == 0)
            fprintf(stderr,
                    "USAGE: 2 <= processes < %d - got %d\n", MAXPROC, nproc);
        MP_BARRIER();
        MP_FINALIZE();
        exit(0);
    }
    
    /* initialize ARMCI */
    ARMCI_Init();

#ifdef TEST_DCMF
  dcmf_connection_initialize ();
#endif

    if(!me)printf("\n             Performance of Basic Blocking Communication Operations\n");
    MP_BARRIER();
    
    CHECK_RESULT=1; test_1D(); CHECK_RESULT=0; /* warmup run */

    /* test 1 dimension array */
    if(!me)printf("\n\t\t\tContiguous Data Transfer\n");
    test_1D();

#ifndef TEST_DCMF
    /* test 2 dimension array */
    if(!me)printf("\n\t\t\tStrided Data Transfer\n");
    test_2D();
#endif

    MP_BARRIER();
    if(me == 0){
       if(warn_accuracy) 
          printf("\nWARNING: Your timer does not have sufficient accuracy for this test (%d)\n",warn_accuracy);
       printf("\n\n------------ Now we test the same data transfer for correctness ----------\n");
       fflush(stdout);
    }

    MP_BARRIER();
#ifndef TEST_DCMF
    CHECK_RESULT=1;
    if(!me)printf("\n\t\t\tContiguous Data Transfer\n");
    test_1D();
    if(me == 0) printf("OK\n");
    MP_BARRIER();
#endif
#ifndef TEST_DCMF
    if(!me)printf("\n\t\t\tStrided Data Transfer\n");
    test_2D();
    if(me == 0) printf("OK\n\n\nTests Completed.\n");
    MP_BARRIER();
#endif
    /* done */
    ARMCI_Finalize();
    MP_FINALIZE();
    return(0);
}    

void fill_array(double *arr, int count, int which)
{
    int i;

    for(i=0; i<count; i++) arr[i] = i * 8.23 + which * 2.89;
}

void check_result(double *src_buf, double *dst_buf, int *stride, int *count,
                  int stride_levels)
{
    int i, j, size;
    long idx;
    int n1dim;  /* number of 1 dim block */
    int bvalue[ARMCI_MAX_STRIDE_LEVEL], bunit[ARMCI_MAX_STRIDE_LEVEL];

    /* number of n-element of the first dimension */
    n1dim = 1;
    for(i=1; i<=stride_levels; i++)
        n1dim *= count[i];

    /* calculate the destination indices */
    bvalue[0] = 0; bvalue[1] = 0; bunit[0] = 1; bunit[1] = 1;
    for(i=2; i<=stride_levels; i++) {
        bvalue[i] = 0;
        bunit[i] = bunit[i-1] * count[i-1];
    }

    for(i=0; i<n1dim; i++) {
        idx = 0;
        for(j=1; j<=stride_levels; j++) {
            idx += bvalue[j] * stride[j-1];
            if((i+1) % bunit[j] == 0) bvalue[j]++;
            if(bvalue[j] > (count[j]-1)) bvalue[j] = 0;
        }
        
        size = count[0] / sizeof(double);
        for(j=0; j<size; j++)
            if(ABS(((double *)((char *)src_buf+idx))[j] - 
               ((double *)((char *)dst_buf+idx))[j]) > 0.000001 ){
                fprintf(stdout,"Error:%s comparison failed: (%d) (%f :%f) %d\n",
                        check_type, j, ((double *)((char *)src_buf+idx))[j],
                        ((double *)((char *)dst_buf+idx))[j], count[0]);
                ARMCI_Error("failed",0);
            }
    }
}

/* array1 = array1 + array2 * scale */
void acc_array(double scale, double *array1, double *array2, int *stride,
               int *count, int stride_levels)
{
        int i, j, size;
    long idx;
    int n1dim;  /* number of 1 dim block */
    int bvalue[ARMCI_MAX_STRIDE_LEVEL], bunit[ARMCI_MAX_STRIDE_LEVEL];

    /* number of n-element of the first dimension */
    n1dim = 1;
    for(i=1; i<=stride_levels; i++)
        n1dim *= count[i];

    /* calculate the destination indices */
    bvalue[0] = 0; bvalue[1] = 0; bunit[0] = 1; bunit[1] = 1;
    for(i=2; i<=stride_levels; i++) {
        bvalue[i] = 0;
        bunit[i] = bunit[i-1] * count[i-1];
    }

    for(i=0; i<n1dim; i++) {
        idx = 0;
        for(j=1; j<=stride_levels; j++) {
            idx += bvalue[j] * stride[j-1];
            if((i+1) % bunit[j] == 0) bvalue[j]++;
            if(bvalue[j] > (count[j]-1)) bvalue[j] = 0;
        }

        size = count[0] / sizeof(double);
        for(j=0; j<size; j++)
            ((double *)((char *)array1+idx))[j] +=
                ((double *)((char *)array2+idx))[j] * scale;

    }
}
