/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*  
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "opa_config.h"

/* FIXME For now we rely on pthreads for our IPC locks.  This is fairly
   portable, although it is obviously not 100% portable.  We need to
   figure out how to support other threading packages and lock
   implementations, such as the BG/P lockbox. */
#if defined(OPA_HAVE_PTHREAD_H)
#include <pthread.h>
#include <opa_primitives.h>

pthread_mutex_t *OPA_emulation_lock = NULL;

int OPA_Interprocess_lock_init(pthread_mutex_t *shm_lock, int isLeader)
{
    int mpi_errno = 0; /*MPI_SUCCESS*/
    OPA_emulation_lock = shm_lock;

    if (isLeader) {
        pthread_mutex_init(OPA_emulation_lock, NULL);
    }

    return mpi_errno;
}
#endif /* defined(OPA_HAVE_PTHREAD_H) */

