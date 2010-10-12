/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_mutex.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "opa_primitives.h"

#ifndef __include_mpidi_mutex_h__
#define __include_mpidi_mutex_h__


#define  MPIDI_MAX_MUTEXES        16
extern OPA_int_t    mutex_vec[MPIDI_MAX_MUTEXES];
extern __thread int mutex_recursion_counter[MPIDI_MAX_MUTEXES];


/**
 *  \brief Initialize a mutex.  In this API, mutexes are acessed via
 *  indices from 0..MPIDI_MAX_MUTEXES. The mutexes are recursive
 *  \param [IN] Index of the mutex
 */
static inline int
MPIDI_Mutex_initialize(int  m)
{
  MPID_assert(m < MPIDI_MAX_MUTEXES);

  OPA_store_int(&mutex_vec[m], 0);
  /*Ensure all threads set this to zero*/
  mutex_recursion_counter[m] = 0;

  return 0;
}


/**
 *  \brief Try to acquire a mutex identified by an index.
 *  \param [IN] Index of the mutex
 *  \return 0   Lock successfully acquired
 *  \return 1   Lock was not acquired
 */
static inline int
MPIDI_Mutex_try_acquire(int m)
{
  register int old_val;

  MPID_assert(m < MPIDI_MAX_MUTEXES);

  if (mutex_recursion_counter[m] >= 1) {
    mutex_recursion_counter[m] ++;
    return 0;
  }

  old_val = OPA_LL_int( &mutex_vec[m] );
  if (old_val != 0)
    return 1;  //Lock failed

  int rc = OPA_SC_int(&mutex_vec[m], 1);  //returns 0 when SC fails

  if (rc == 0)
    return 1; //Lock failed

  mutex_recursion_counter[m] =  1;
  return 0;   //Lock succeeded
}


/**
 *  \brief Acquire a mutex identified by an index.
 *  \param [IN] Index of the mutex
 *  \return 0   Lock successfully acquired
 *  \return 1   Fail
 */
static inline int
MPIDI_Mutex_acquire(int m)
{
  register int old_val;

  MPID_assert(m < MPIDI_MAX_MUTEXES);

  if (mutex_recursion_counter[m] >= 1) {
    mutex_recursion_counter[m] ++;
    return 0;
  }

  do {
    old_val = OPA_LL_int( &mutex_vec[m] );
    if (old_val != 0)
      continue;

  } while (!OPA_SC_int(&mutex_vec[m], 1));

  mutex_recursion_counter[m] =  1;
  return 0;
}


/**
 *  \brief Release a mutex identified by an index.
 *  \param [IN] Index of the mutex
 *  \return 0   Lock successfully acquired
 *  \return 1   Fail
 */
static inline int
MPIDI_Mutex_release(int m)
{
  MPID_assert(m < MPIDI_MAX_MUTEXES);
  //Verify this thread is the owner of this lock
  MPID_assert( mutex_recursion_counter[m] > 0 );

  mutex_recursion_counter[m] --;
  if (mutex_recursion_counter[m] > 0)
    return 0;    /* Future calls will release the lock to other threads */

  /* Wait till all the writes in the critical sections from this
     thread have completed and invalidates have been delivered*/
  OPA_read_write_barrier();

  /* Release the lock */
  OPA_store_int(&mutex_vec[m], 0);

  return 0;
}


#endif
