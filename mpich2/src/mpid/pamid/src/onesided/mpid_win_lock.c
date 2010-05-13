/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_lock.c
 * \brief ???
 */
#include "mpidi_onesided.h"


static inline void
MPIDI_WinLockAck_post(pami_context_t   context,
                      unsigned         peer,
                      MPID_Win       * win);


static inline void
MPIDI_WinLockAdvance(pami_context_t   context,
                     MPID_Win       * win)
{
  struct MPIDI_Win_sync_lock* slock = &win->mpid.sync.lock;
  struct MPIDI_Win_queue*     q     = &slock->local.requested;

  if (
      (q->head != NULL ) &&
      ( (slock->local.count == 0) ||
        (
         (slock->local.type == MPI_LOCK_SHARED) &&
         (q->head->type     == MPI_LOCK_SHARED)
         )
        )
      )
    {
      struct MPIDI_Win_lock* lock = q->head;
      q->head = lock->next;
      if (q->head == NULL)
        q->tail = NULL;

      ++slock->local.count;
      slock->local.type = lock->type;

      MPIDI_WinLockAck_post(context, lock->rank, win);
      MPIU_Free(lock);
      MPIDI_WinLockAdvance(context, win);
    }
}


static inline void
MPIDI_WinLockReq_post(pami_context_t   context,
                      unsigned         peer,
                      int              lock_type,
                      MPID_Win       * win)
{
  MPIDI_Win_control_t info = {
  type       : MPIDI_WIN_MSGTYPE_LOCKREQ,
  data       : {
    lock       : {
      type : lock_type,
    },
    },
  };

  MPIDI_WinCtrlSend(context, &info, peer, win);
}


void
MPIDI_WinLockReq_proc(pami_context_t              context,
                      const MPIDI_Win_control_t * info,
                      unsigned                    peer)
{
  MPID_Win * win = info->win;
  struct MPIDI_Win_lock* lock = MPIU_Calloc0(1, struct MPIDI_Win_lock);
  lock->rank = peer;
  lock->type = info->data.lock.type;

  struct MPIDI_Win_queue* q = &win->mpid.sync.lock.local.requested;
  MPID_assert( (q->head != NULL) ^ (q->tail == NULL) );
  if (q->tail == NULL)
    q->head = lock;
  else
    q->tail->next = lock;
  q->tail = lock;

  MPIDI_WinLockAdvance(context, win);
}


static inline void
MPIDI_WinLockAck_post(pami_context_t   context,
                      unsigned         peer,
                      MPID_Win       * win)
{
  MPIDI_Win_control_t info = {
  type       : MPIDI_WIN_MSGTYPE_LOCKACK,
  };
  MPIDI_WinCtrlSend(context, &info, peer, win);
}


void
MPIDI_WinLockAck_proc(pami_context_t              context,
                      const MPIDI_Win_control_t * info,
                      unsigned                    peer)
{
  info->win->mpid.sync.lock.remote.locked = 1;
}


static inline void
MPIDI_WinUnlock_post(pami_context_t   context,
                     unsigned         peer,
                     MPID_Win       * win)
{
  MPIDI_Win_control_t info = {
  type       : MPIDI_WIN_MSGTYPE_UNLOCK,
  };
  MPIDI_WinCtrlSend(context, &info, peer, win);
}


void
MPIDI_WinUnlock_proc(pami_context_t              context,
                     const MPIDI_Win_control_t * info,
                     unsigned                    peer)
{
  MPID_Win *win = info->win;
  --win->mpid.sync.lock.local.count;
  MPID_assert((int)win->mpid.sync.lock.local.count >= 0);
  MPIDI_WinLockAdvance(context, win);
}


int
MPID_Win_lock(int       lock_type,
              int       rank,
              int       assert,
              MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;
  struct MPIDI_Win_sync_lock* slock = &win->mpid.sync.lock;

  MPIDI_WinLockReq_post(MPIDI_Context[0], rank, lock_type, win);
  MPID_PROGRESS_WAIT_WHILE(!slock->remote.locked);

  return mpi_errno;
}


int
MPID_Win_unlock(int       rank,
                MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_PROGRESS_WAIT_WHILE(sync->started != sync->complete);
  sync->started  = 0;
  sync->complete = 0;

  MPIDI_WinUnlock_post(MPIDI_Context[0], rank, win);
  sync->lock.remote.locked   = 0;
  return mpi_errno;
}
