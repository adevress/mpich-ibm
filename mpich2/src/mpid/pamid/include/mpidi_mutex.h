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


#undef MPIDI_USE_OPA


#ifdef MPIDI_USE_OPA

#define  MPIDI_MAX_MUTEXES        16
extern OPA_int_t    MPIDI_Mutex_vector [MPIDI_MAX_MUTEXES];
extern __thread int MPIDI_Mutex_counter[MPIDI_MAX_MUTEXES];


/**
 *  \brief Initialize a mutex.
 *
 *  In this API, mutexes are acessed via indices from
 *  0..MPIDI_MAX_MUTEXES. The mutexes are recursive
 */
static inline int
MPIDI_Mutex_initialize()
{
  register size_t i;
  for (i=0; i<MPIDI_MAX_MUTEXES; ++i)
    OPA_store_int(&MPIDI_Mutex_vector[i], 0);
  return 0;
}


/**
 *  \brief Try to acquire a mutex identified by an index.
 *  \param[in] m Index of the mutex
 *  \return 0    Lock successfully acquired
 *  \return 1    Lock was not acquired
 */
static inline int
MPIDI_Mutex_try_acquire(unsigned m)
{
  register int old_val;

  MPID_assert(m < MPIDI_MAX_MUTEXES);

  if (MPIDI_Mutex_counter[m] >= 1) {
    MPIDI_Mutex_counter[m]++;
    return 0;
  }

  old_val = OPA_LL_int(&MPIDI_Mutex_vector[m]);
  if (old_val != 0)
    return 1;  //Lock failed

  int rc = OPA_SC_int(&MPIDI_Mutex_vector[m], 1);  //returns 0 when SC fails

  if (rc == 0)
    return 1; //Lock failed

  MPIDI_Mutex_counter[m] =  1;
  return 0;   //Lock succeeded
}


/**
 *  \brief Acquire a mutex identified by an index.
 *  \param[in] m Index of the mutex
 *  \return 0    Lock successfully acquired
 *  \return 1    Fail
 */
static inline int
MPIDI_Mutex_acquire(unsigned m)
{
  register int old_val;

  MPID_assert(m < MPIDI_MAX_MUTEXES);

  if (MPIDI_Mutex_counter[m] >= 1) {
    MPIDI_Mutex_counter[m]++;
    return 0;
  }

  do {
    old_val = OPA_LL_int(&MPIDI_Mutex_vector[m]);
    if (old_val != 0)
      continue;

  } while (!OPA_SC_int(&MPIDI_Mutex_vector[m], 1));

  MPIDI_Mutex_counter[m] =  1;
  return 0;
}


/**
 *  \brief Release a mutex identified by an index.
 *  \param[in] m Index of the mutex
 *  \return 0    Lock successfully released
 *  \return 1    Fail
 */
static inline int
MPIDI_Mutex_release(unsigned m)
{
  MPID_assert(m < MPIDI_MAX_MUTEXES);
  //Verify this thread is the owner of this lock
  MPID_assert(MPIDI_Mutex_counter[m] > 0);

  MPIDI_Mutex_counter[m]--;
  MPID_assert(MPIDI_Mutex_counter[m] >= 0);
  if (MPIDI_Mutex_counter[m] > 0)
    return 0;    /* Future calls will release the lock to other threads */

  /* Wait till all the writes in the critical sections from this
     thread have completed and invalidates have been delivered*/
  OPA_read_write_barrier();

  /* Release the lock */
  OPA_store_int(&MPIDI_Mutex_vector[m], 0);

  return 0;
}

#else

extern pthread_mutex_t MPIDI_Mutex_lock;

static inline int
MPIDI_Mutex_initialize()
{
  int rc;

  pthread_mutexattr_t attr;
  rc = pthread_mutexattr_init(&attr);
  MPID_assert(rc == 0);
  extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) __THROW __nonnull ((1));
  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
  MPID_assert(rc == 0);

  rc = pthread_mutex_init(&MPIDI_Mutex_lock, &attr);
  MPID_assert(rc == 0);

  return 0;
}


static inline int
MPIDI_Mutex_try_acquire(unsigned m)
{
  return pthread_mutex_trylock(&MPIDI_Mutex_lock);
}


static inline int
MPIDI_Mutex_acquire(unsigned m)
{
  return pthread_mutex_lock(&MPIDI_Mutex_lock);
}


static inline int
MPIDI_Mutex_release(unsigned m)
{
  return pthread_mutex_unlock(&MPIDI_Mutex_lock);
}

#endif


#endif
