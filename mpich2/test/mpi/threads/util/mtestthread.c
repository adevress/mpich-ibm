/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2006 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include "mpitest.h"
#include "mpithreadtest.h"

/* This file provides a portability layer for using threads.  Currently, 
   it supports POSIX threads (pthreads) and Windows threads.  Testing has 
   been performed for pthreads.
 */

/* We remember all of the threads we create; this similifies terminating 
   (joining) them. */
#ifndef MTEST_MAX_THREADS
#define MTEST_MAX_THREADS 16
#endif

static MTEST_THREAD_HANDLE threads[MTEST_MAX_THREADS];
static int nthreads = 0;

#ifdef HAVE_WINDOWS_H
int MTest_Start_thread(MTEST_THREAD_RETURN_TYPE (*fn)(void *p),void *arg)
{
    if (nthreads >= MTEST_MAX_THREADS) {
	fprintf( stderr, "Too many threads already created: max is %d\n",
		 MTEST_MAX_THREADS );
	return 1;
    }
    threads[nthreads] = CreateThread(NULL, 0, 
		     (LPTHREAD_START_ROUTINE)fn, (LPVOID)arg, 0, NULL);
    if (threads[nthreads] == NULL)
    {
	return GetLastError();
    }
    else 
	nthreads++;
    
    return 0;
}
int MTest_Join_threads( void ){
    int i, err = 0;
    for (i=0; i<nthreads; i++) {
	if(WaitForSingleObject(threads[i], INFINITE) == WAIT_FAILED){
	    DEBUG(printf("Error WaitForSingleObject() \n"));
	    err = GetLastError();
	}
	else 
	    CloseHandle(threads[i]);
    }
    nthreads = 0;
    return err;
}
#else
int MTest_Start_thread(MTEST_THREAD_RETURN_TYPE (*fn)(void *p),void *arg)
{
    int err;
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (nthreads >= MTEST_MAX_THREADS) {
	fprintf( stderr, "Too many threads already created: max is %d\n",
		 MTEST_MAX_THREADS );
	return 1;
    }
    err = pthread_create(threads+nthreads, &attr, fn, arg);
    if (!err) nthreads++;
    pthread_attr_destroy(&attr);
    return err;
}
int MTest_Join_threads( void )
{
    int i, rc, err = 0;
    for (i=0; i<nthreads; i++) {
	rc = pthread_join(threads[i], 0);
	if (rc) err = rc;
    }
    nthreads = 0;
    return err;
}
int MTest_thread_lock_create( MTEST_THREAD_LOCK_TYPE *lock )
{
    int err;
    err = pthread_mutex_init( lock, NULL );
    if (err) {
	perror( "Failed to initialize lock:" );
    }
    return err;
}
int MTest_thread_lock( MTEST_THREAD_LOCK_TYPE *lock )
{
    int err;
    err = pthread_mutex_lock( lock );
    if (err) {
	perror( "Failed to acquire lock:" );
    }
    return err;
}
int MTest_thread_unlock( MTEST_THREAD_LOCK_TYPE *lock )
{
    int err;
    err = pthread_mutex_unlock( lock );
    if (err) {
	perror( "Failed to release lock:" );
    }
    return err;
}
int MTest_thread_lock_free( MTEST_THREAD_LOCK_TYPE *lock )
{
    int err;
    err = pthread_mutex_destroy( lock );
    if (err) {
	perror( "Failed to free lock:" );
    }
    return err;
}
#endif

static MTEST_THREAD_LOCK_TYPE barrierLock;
static int   phase=0;
static int   c[2] = {-1,-1};
int MTest_thread_barrier_init( void )
{
    return MTest_thread_lock_create( &barrierLock );
}
int MTest_thread_barrier_free( void )
{
    return MTest_thread_lock_free( &barrierLock );
}
int MTest_thread_barrier( int nt )
{
    volatile int *cntP;
    int          err = 0;

    if (nt < 0) nt = nthreads;
    cntP = &c[phase];

    err = MTest_thread_lock( &barrierLock );
    if (*cntP < 0) *cntP = nt;
    /* printf( "phase = %d, cnt = %d\n", phase, *cntP ); */
    if (*cntP == 1) { phase = !phase; c[phase] = nt; }
    /* Really need a write barrier here */
    *cntP = *cntP - 1;
    err = MTest_thread_unlock( &barrierLock );
    /* printf( "About to wait: %d\n", *cntP ); fflush(stdout); */
    while (*cntP) ;
    
    return err;
}

