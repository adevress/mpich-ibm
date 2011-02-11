/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_mutex.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef __include_mpidi_mutex_h__
#define __include_mpidi_mutex_h__

#include <opa_primitives.h>


#ifdef MPIDI_USE_OPA

#include <kernel/location.h>

#define MPIDI_MUTEX_THREAD_ID()                                         \
({                                                                      \
  assert(MPIDI_Mutex_threadID[Kernel_ProcessorID()] != (uint8_t)-1);    \
  MPIDI_Mutex_threadID[Kernel_ProcessorID()];                           \
})

#define  MPIDI_MAX_MUTEXES 16
#define  MPIDI_MAX_THREADS 64
extern OPA_int_t  MPIDI_Mutex_vector  [MPIDI_MAX_MUTEXES];
extern int        MPIDI_Mutex_counter [MPIDI_MAX_THREADS][MPIDI_MAX_MUTEXES];
extern uint8_t    MPIDI_Mutex_threadID[MPIDI_MAX_THREADS];

/**
 *  \brief Initialize a mutex.
 *
 *  In this API, mutexes are acessed via indices from
 *  0..MPIDI_MAX_MUTEXES. The mutexes are recursive
 */
static inline int
MPIDI_Mutex_initialize()
{
  size_t i;

  uint64_t ThreadMask = Kernel_ThreadMask(Kernel_MyTcoord());
  uint8_t  ThreadCount = 0;
#if 0
    fprintf(stderr,
            ">>> 1 I am process (Kernel_MyTcoord=%u) out of (Kernel_ProcessCount=%u) processes.\n"
            ">>> 2 I am Running on (HWThread Kernel_ProcessorID=%u).\n"
            ">>> 3 (Kernel_ThreadMask=%016"PRIx64")\n",
            Kernel_MyTcoord(),
            Kernel_ProcessCount(),
            Kernel_ProcessorID(),
            ThreadMask);
#endif
  i=MPIDI_MAX_THREADS;
  do {
    --i;
    MPIDI_Mutex_threadID[i] = (ThreadMask & 1) ? ThreadCount++ : (uint8_t)-1;
    ThreadMask >>= 1;
  } while (i != 0);
  for (i=0; i<MPIDI_MAX_THREADS; ++i)
    if (MPIDI_Mutex_threadID[i] != (uint8_t)-1)
      MPIDI_Mutex_threadID[i] = ThreadCount-MPIDI_Mutex_threadID[i]-1;
#if 0
  {
    char buf[6*MPIDI_MAX_THREADS] = {0};
    for (i=0; i<MPIDI_MAX_THREADS; ++i)
      snprintf(buf+3*i, 4, "|%2d", (int)(int8_t)MPIDI_Mutex_threadID[i]);
    buf[0] = '[';
    fprintf(stderr, ">>> 4 %s]\n", buf);
    fprintf(stderr, ">>> 5 MPIDI_MUTEX_THREAD_ID=%"PRIu8"\n", MPIDI_MUTEX_THREAD_ID());
  }
#endif
  MPID_assert(MPIDI_MUTEX_THREAD_ID() == 0);

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

  if (MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] >= 1) {
    MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m]++;
    return 0;
  }

  old_val = OPA_LL_int(&MPIDI_Mutex_vector[m]);
  if (old_val != 0)
    return 1;  /* Lock failed */

  int rc = OPA_SC_int(&MPIDI_Mutex_vector[m], 1);  /* returns 0 when SC fails */

  if (rc == 0)
    return 1; /* Lock failed */

  MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] =  1;
  return 0;   /* Lock succeeded */
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

  if (MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] >= 1) {
    MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m]++;
    return 0;
  }

  do {
    old_val = OPA_LL_int(&MPIDI_Mutex_vector[m]);
    if (old_val != 0)
      continue;

  } while (!OPA_SC_int(&MPIDI_Mutex_vector[m], 1));

  MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] =  1;
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
  /* Verify this thread is the owner of this lock */
  MPID_assert(MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] > 0);

  MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m]--;
  MPID_assert(MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] >= 0);
  if (MPIDI_Mutex_counter[MPIDI_MUTEX_THREAD_ID()][m] > 0)
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
  extern int pthread_mutexattr_settype(pthread_mutexattr_t *__attr, int __kind) __THROW __nonnull ((1));
#ifdef USE_PAMI_COMM_THREADS
  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#endif
  MPID_assert(rc == 0);

  rc = pthread_mutex_init(&MPIDI_Mutex_lock, &attr);
  MPID_assert(rc == 0);

  return 0;
}


static inline int
MPIDI_Mutex_try_acquire(unsigned m)
{
  int rc;
  rc = pthread_mutex_trylock(&MPIDI_Mutex_lock);
  assert( (rc == 0) || (rc == EBUSY) );
  /* fprintf(stderr, "%s:%u (rc=%d)\n", __FUNCTION__, __LINE__, rc); */
  return rc;
}


static inline int
MPIDI_Mutex_acquire(unsigned m)
{
  int rc;
  /* fprintf(stderr, "%s:%u\n", __FUNCTION__, __LINE__); */
  rc = pthread_mutex_lock(&MPIDI_Mutex_lock);
  /* fprintf(stderr, "%s:%u (rc=%d)\n", __FUNCTION__, __LINE__, rc); */
  assert(rc == 0);
  return rc;
}


static inline int
MPIDI_Mutex_release(unsigned m)
{
  int rc;
  rc = pthread_mutex_unlock(&MPIDI_Mutex_lock);
  /* fprintf(stderr, "%s:%u (rc=%d)\n", __FUNCTION__, __LINE__, rc); */
  assert(rc == 0);
  return rc;
}

#endif


#endif
