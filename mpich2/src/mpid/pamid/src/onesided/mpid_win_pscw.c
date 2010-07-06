/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_pscw.c
 * \brief ???
 */
#include "mpidi_onesided.h"


static inline void
MPIDI_WinPost_post(pami_context_t   context,
                   unsigned         peer,
                   MPID_Win       * win)
{
  MPIDI_Win_control_t info = {
  type : MPIDI_WIN_MSGTYPE_POST,
  };

  MPIDI_WinCtrlSend(context, &info, peer, win);
}


void
MPIDI_WinPost_proc(pami_context_t              context,
                   const MPIDI_Win_control_t * info,
                   unsigned                    peer)
{
  ++info->win->mpid.sync.pw.count;
}


static inline void
MPIDI_WinComplete_post(pami_context_t   context,
                       unsigned         peer,
                       MPID_Win       * win)
{
  MPIDI_Win_control_t info = {
  type : MPIDI_WIN_MSGTYPE_COMPLETE,
  };

  MPIDI_WinCtrlSend(context, &info, peer, win);
}


void
MPIDI_WinComplete_proc(pami_context_t              context,
                       const MPIDI_Win_control_t * info,
                       unsigned                    peer)
{
  ++info->win->mpid.sync.sc.count;
}


int
MPID_Win_start(MPID_Group *group,
               int         assert,
               MPID_Win   *win)
{
  int mpi_errno = MPI_SUCCESS;
  MPIR_Group_add_ref(group);

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_PROGRESS_WAIT_WHILE(group->size != sync->pw.count);
  sync->pw.count = 0;

  MPID_assert(win->mpid.sync.sc.group == NULL);
  win->mpid.sync.sc.group = group;

  return mpi_errno;
}


int
MPID_Win_complete(MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_PROGRESS_WAIT_WHILE(sync->started != sync->complete);
  sync->started  = 0;
  sync->complete = 0;

  unsigned rank, index;
  MPID_Group *group = sync->sc.group;
  sync->sc.group = NULL;
  for (index=0; index < group->size; ++index) {
    rank = group->lrank_to_lpid[index].lpid;
    MPIDI_WinComplete_post(MPIDI_Context[0], rank, win);
  }

  MPIR_Group_release(group);
  return mpi_errno;
}


int
MPID_Win_post(MPID_Group *group,
              int         assert,
              MPID_Win   *win)
{
  int mpi_errno = MPI_SUCCESS;
  MPIR_Group_add_ref(group);

  MPID_assert(win->mpid.sync.pw.group == NULL);
  win->mpid.sync.pw.group = group;

  unsigned rank, index;
  for (index=0; index < group->size; ++index) {
    rank = group->lrank_to_lpid[index].lpid;
    MPIDI_WinPost_post(MPIDI_Context[0], rank, win);
  }

  return mpi_errno;
}


int
MPID_Win_wait(MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_Group *group = sync->pw.group;
  MPID_PROGRESS_WAIT_WHILE(group->size != sync->sc.count);
  sync->sc.count = 0;
  sync->pw.group = NULL;

  MPIR_Group_release(group);
  return mpi_errno;
}


int
MPID_Win_test (MPID_Win *win,
               int      *flag)
{
  int mpi_errno = MPI_SUCCESS;

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  MPID_Group *group = sync->pw.group;
  if (group->size == sync->sc.count)
    {
      sync->sc.count = 0;
      sync->pw.group = NULL;
      *flag = 1;
      MPIR_Group_release(group);
    }
  else
    {
      MPID_Progress_poke();
    }

  return mpi_errno;
}
