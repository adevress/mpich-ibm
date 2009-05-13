/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2009 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/* 
 * Run concurrent sends to different target processes. Stresses an 
 * implementation that permits concurrent sends to different targets.
 *
 * By turning on verbose output, some simple performance data will be output.
 *
 * Use nonblocking sends, and have a single thread complete all I/O.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "mpitest.h"
#include "mpithreadtest.h"

/* This is the master test routine */
#define MAX_CNT 660000
/*#define MAX_LOOP 200 */
#define MAX_LOOP 10
#define MAX_NTHREAD 128

static int ownerWaits = 0;

void run_test_send(void *arg)
{
    int    cnt, j, *buf, wsize;
    int    thread_num = (int)arg;
    double t;
    static MPI_Request r[MAX_NTHREAD];

    MPI_Comm_size( MPI_COMM_WORLD, &wsize );
    if (wsize >= MAX_NTHREAD) wsize = MAX_NTHREAD;
    
    for (cnt=1; cnt < MAX_CNT; cnt = 2*cnt) {
	buf = (int *)malloc( cnt * sizeof(int) );
	/* printf( "%d My buf is at %p (%d)\n", 
	   pthread_self(), buf, cnt * sizeof(int) );fflush(stdout); */

	/* Wait for all senders to be ready */
	for (j=0; j<wsize; j++) r[j] = MPI_REQUEST_NULL;
	MTest_thread_barrier(-1);

	t = MPI_Wtime();
	for (j=0; j<MAX_LOOP; j++) {
	    MTest_thread_barrier(-1);
	    MPI_Isend( buf, cnt, MPI_INT, thread_num, cnt, MPI_COMM_WORLD,
		       &r[thread_num-1]);
	    if (ownerWaits) {
		MPI_Wait( &r[thread_num-1], MPI_STATUS_IGNORE );
	    }
	    else {
		/* Wait for all threads to start the sends */
		MTest_thread_barrier(-1);
		if (thread_num == 1) 
		    MPI_Waitall( wsize-1, r, MPI_STATUSES_IGNORE );
	    }
	}
	t = MPI_Wtime() - t;
	free( buf );
	if (thread_num == 1) 
	    MTestPrintfMsg( 1, "buf size %d: time %f\n", cnt, t / MAX_LOOP );
    }
}
void run_test_recv( void )
{
    int        cnt, j, *buf;
    MPI_Status status;
    double     t;
    
    for (cnt=1; cnt < MAX_CNT; cnt = 2*cnt) {
	buf = (int *)malloc( cnt * sizeof(int) );
	t = MPI_Wtime();
	for (j=0; j<MAX_LOOP; j++)
	    MPI_Recv( buf, cnt, MPI_INT, 0, cnt, MPI_COMM_WORLD, &status );
	t = MPI_Wtime() - t;
	free( buf );
    }
}

int main(int argc, char ** argv)
{
    int i, pmode, nprocs, rank;
    int errs = 0, err;

    MTest_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &pmode);
    if (pmode != MPI_THREAD_MULTIPLE) {
	fprintf(stderr, "Thread Multiple not supported by the MPI implementation\n");
	MPI_Abort(MPI_COMM_WORLD, -1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (nprocs < 2) {
	fprintf(stderr, "Need at least two processes\n");
	MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (nprocs > MAX_NTHREAD) nprocs = MAX_NTHREAD;

    err = MTest_thread_barrier_init();
    if (err) {
	fprintf( stderr, "Could not create thread barrier\n" );
	MPI_Abort( MPI_COMM_WORLD, 1 );
    }
    MPI_Barrier( MPI_COMM_WORLD );
    if (rank == 0) {
	for (i=1; i<nprocs; i++) 
	    MTest_Start_thread( run_test_send,  (void *)i );

	MTest_Join_threads( );
    }
    else if (rank < MAX_NTHREAD) {
	run_test_recv();
    }
    MTest_thread_barrier_free();

    MTest_Finalize( errs );

    MPI_Finalize();

    return 0;
}
