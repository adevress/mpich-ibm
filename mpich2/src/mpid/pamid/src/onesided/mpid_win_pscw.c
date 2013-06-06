/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_pscw.c
 * \brief ???
 */
#include "mpidi_onesided.h"


typedef struct
{
  MPID_Win          * win;

  volatile unsigned   done;
  pami_work_t         work;
} MPIDI_WinPSCW_info;


static pami_result_t
MPIDI_WinPost_post(pami_context_t   context,
                   void           * _info)
{
  /* Send to remote peers to unblock their start. */
  MPIDI_WinPSCW_info * info = (MPIDI_WinPSCW_info*)_info;
  unsigned peer, index;
  MPID_Group *group = info->win->mpid.sync.pw.group;
  MPID_assert(group != NULL);
  MPIDI_Win_control_t msg = {
    .type = MPIDI_WIN_MSGTYPE_POST,
  };

  for (index=0; index < group->size; ++index) {
    peer = group->lrank_to_lpid[index].lpid;
    TRACE_ERR("<%s>PW count %u, peer %u, group %p\n",__FUNCTION__,info->win->mpid.sync.pw.count, peer, info->win->mpid.sync.pw.group);
    MPIDI_WinCtrlSend(context, &msg, peer, info->win);
  }

  info->done = 1;
  return PAMI_SUCCESS;
}


void
MPIDI_WinPost_proc(pami_context_t              context,
                   const MPIDI_Win_control_t * info,
                   unsigned                    peer)
{
  /* Counter intuitively, an incoming post is used on the local start/complete.  It satisfies the local blocking start.
     So we use the 'sc' structure.  */
  if(info->win->mpid.sync.sc.group != NULL && MPIDI_valid_group_rank(peer, info->win->mpid.sync.sc.group))
  {
    ++info->win->mpid.sync.sc.count;
    TRACE_ERR("<%s>SC count %u, peer %u, SC group %p\n",__FUNCTION__,info->win->mpid.sync.sc.count, peer, info->win->mpid.sync.sc.group);
  }
  else
  { /* add to early arrival list */
    TRACE_ERR("<%s>SC early %u, peer %u, group %p\n",__FUNCTION__,info->win->mpid.sync.sc.count, peer, info->win->mpid.sync.sc.group);
    struct MPIDI_Win_sync_early * early =  MPIU_Malloc(sizeof(struct MPIDI_Win_sync_early));
    MPID_assert(early != NULL);
    early->next = info->win->mpid.sync.sc.early;
    early->peer = peer;
    info->win->mpid.sync.sc.early = early;
  }
}


static pami_result_t
MPIDI_WinComplete_post(pami_context_t   context,
                       void           * _info)
{
  /* Send to remote peers to unblock their wait. */
  MPIDI_WinPSCW_info * info = (MPIDI_WinPSCW_info*)_info;
  unsigned peer, index;
  MPID_Group *group = info->win->mpid.sync.sc.group;
  MPID_assert(group != NULL);
  MPIDI_Win_control_t msg = {
    .type = MPIDI_WIN_MSGTYPE_COMPLETE,
  };

  for (index=0; index < group->size; ++index) {
    peer = group->lrank_to_lpid[index].lpid;
    TRACE_ERR("<%s>SC count %u, peer %u, group %p\n",__FUNCTION__,info->win->mpid.sync.sc.count, peer, info->win->mpid.sync.sc.group);
    MPIDI_WinCtrlSend(context, &msg, peer, info->win);
  }

  info->done = 1;
  return PAMI_SUCCESS;
}


void
MPIDI_WinComplete_proc(pami_context_t              context,
                       const MPIDI_Win_control_t * info,
                       unsigned                    peer)
{
  /* Counter intuitively, an incoming complete is used on the local post/wait.  It satisfies the blocking wait.
     So we use the 'pw' structure.  */
  if(info->win->mpid.sync.pw.group != NULL && MPIDI_valid_group_rank(peer, info->win->mpid.sync.pw.group))
  {
    ++info->win->mpid.sync.pw.count;
    TRACE_ERR("<%s>PW count %u, peer %u, PW group %p\n",__FUNCTION__,info->win->mpid.sync.pw.count, peer, info->win->mpid.sync.pw.group);
  }
  else
  { /* add to early arrival list */
    TRACE_ERR("<%s>PW early %u, peer %u, group %p\n",__FUNCTION__,info->win->mpid.sync.pw.count, peer, info->win->mpid.sync.pw.group);
    struct MPIDI_Win_sync_early * early =  MPIU_Malloc(sizeof(struct MPIDI_Win_sync_early));
    MPID_assert(early != NULL);
    early->next = info->win->mpid.sync.pw.early;
    early->peer = peer;
    info->win->mpid.sync.pw.early = early;
  }
}


int
MPID_Win_start(MPID_Group *group,
               int         assert,
               MPID_Win   *win)
{
  unsigned index;
  int mpi_errno = MPI_SUCCESS;
  static char FCNAME[] = "MPID_Win_start";
  if(win->mpid.sync.origin_epoch_type != MPID_EPOTYPE_NONE &&
    win->mpid.sync.origin_epoch_type != MPID_EPOTYPE_REFENCE)
  {
    MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                        return mpi_errno, "**rmasync");
  }

  MPIR_Group_add_ref(group);

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  /* Check early arrival count for this new group */
  TRACE_ERR("<%s>Check SC count %u, SC group %p\n",__FUNCTION__,sync->sc.count, sync->sc.group);
  for (index=0; index < group->size; ++index) 
  {
    unsigned peer = group->lrank_to_lpid[index].lpid;
    TRACE_ERR("<%s>Check SC index %u/%u, SC count %u, peer %u, SC group %p\n",__FUNCTION__,index,group->size, sync->sc.count, peer, sync->sc.group);
    struct MPIDI_Win_sync_early * prev =  NULL;
    struct MPIDI_Win_sync_early * next =  sync->sc.early;
    while (next != NULL && next->peer != peer)
    {
      TRACE_ERR("<%s>Check SC count %u, peer %u/%u, group %p\n",__FUNCTION__,sync->sc.count, peer, next->peer, sync->sc.group);
      if(next->next != NULL && next->next->peer == peer) /* look ahead and save prev if next is match */
        prev = next;
      next = next->next; /* check early arrival list */
    }
    if(next != NULL && next->peer == peer) /* found it: count it and free it */
    {
      sync->sc.count++;
      TRACE_ERR("<%s>Found SC count %u, peer %u/%u, group %p\n",__FUNCTION__,sync->sc.count, peer, next->peer, sync->sc.group);
      if(prev != NULL) prev->next = next->next;
      else sync->sc.early = next->next;
      MPIU_Free(next);
    }
  }
  MPIU_ERR_CHKORASSERT(win->mpid.sync.sc.group == NULL,
                       mpi_errno, MPI_ERR_GROUP, return mpi_errno, "**group");

  win->mpid.sync.sc.group = group;

  TRACE_ERR("<%s>wait while SC %u != %u, group %p, SC group %p, SC group %p\n",__FUNCTION__,group->size,sync->sc.count,group, sync->sc.group, sync->sc.group);
  MPID_PROGRESS_WAIT_WHILE(group->size != sync->sc.count);
  TRACE_ERR("<%s> wait done\n",__FUNCTION__);
  sync->sc.count = 0;


  win->mpid.sync.origin_epoch_type = MPID_EPOTYPE_START;

  return mpi_errno;
}


int
MPID_Win_complete(MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;
  static char FCNAME[] = "MPID_Win_complete";
  if(win->mpid.sync.origin_epoch_type != MPID_EPOTYPE_START){
     MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                        return mpi_errno, "**rmasync");
  }

  struct MPIDI_Win_sync* sync = &win->mpid.sync;
  TRACE_ERR("<%s> wait while sync total %u != %u\n",__FUNCTION__, sync->total, sync->complete);
  MPID_PROGRESS_WAIT_WHILE(sync->total != sync->complete);
  TRACE_ERR("<%s> wait done\n",__FUNCTION__);
  sync->total    = 0;
  sync->started  = 0;
  sync->complete = 0;

  MPIDI_WinPSCW_info info = {
    .done = 0,
    .win  = win,
  };
  MPIDI_Context_post(MPIDI_Context[0], &info.work, MPIDI_WinComplete_post, &info);
  TRACE_ERR("<%s>wait while info done !%u\n",__FUNCTION__,info.done);
  MPID_PROGRESS_WAIT_WHILE(!info.done);
  TRACE_ERR("<%s> wait done\n",__FUNCTION__);

  if(win->mpid.sync.target_epoch_type == MPID_EPOTYPE_REFENCE)
  {
    win->mpid.sync.origin_epoch_type = MPID_EPOTYPE_REFENCE;
  }else{
    win->mpid.sync.origin_epoch_type = MPID_EPOTYPE_NONE;
  }
  TRACE_ERR("<%s>Free SC group %p\n",__FUNCTION__,sync->sc.group);
  MPIR_Group_release(sync->sc.group);
  sync->sc.group = NULL;
  return mpi_errno;
}


int
MPID_Win_post(MPID_Group *group,
              int         assert,
              MPID_Win   *win)
{
  unsigned index; 
  int mpi_errno = MPI_SUCCESS;
  static char FCNAME[] = "MPID_Win_post";
  if(win->mpid.sync.target_epoch_type != MPID_EPOTYPE_NONE &&
     win->mpid.sync.target_epoch_type != MPID_EPOTYPE_REFENCE){
       MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                        return mpi_errno, "**rmasync");
  }

  MPIR_Group_add_ref(group);

  MPIU_ERR_CHKORASSERT(win->mpid.sync.pw.group == NULL,
                       mpi_errno, MPI_ERR_GROUP, return mpi_errno,"**group");

  /* Check early arrival count for this new group */
  TRACE_ERR("<%s>Check PW count %u, group %p, group size %u\n",__FUNCTION__,win->mpid.sync.pw.count, group, group->size);
  for (index=0; index < group->size; ++index) 
  {
    unsigned peer = group->lrank_to_lpid[index].lpid;
    TRACE_ERR("<%s>Check PW index %u/%u, count %u, peer %u, group %p\n",__FUNCTION__,index,group->size, win->mpid.sync.pw.count, peer, win->mpid.sync.pw.group);
    struct MPIDI_Win_sync_early * prev =  NULL;
    struct MPIDI_Win_sync_early * next =  win->mpid.sync.pw.early;
    while (next != NULL && next->peer != peer)
    {
      TRACE_ERR("<%s>Check PW count %u, peer %u/%u, group %p\n",__FUNCTION__,win->mpid.sync.pw.count, peer, next->peer, win->mpid.sync.pw.group);
      if(next->next != NULL && next->next->peer == peer) /* look ahead and save prev if next is match */
        prev = next;
      next = next->next; /* check early arrival list */
    }
    if(next != NULL && next->peer == peer) /* found it: count it and free it */
    {
      win->mpid.sync.pw.count++;
      TRACE_ERR("<%s>Found PW count %u, peer %u/%u, group %p\n",__FUNCTION__,win->mpid.sync.pw.count, peer, next->peer, win->mpid.sync.pw.group);
      if(prev != NULL) prev->next = next->next;
      else win->mpid.sync.pw.early = next->next;
      MPIU_Free(next);
    }
  }
  win->mpid.sync.pw.group = group;

  MPIDI_WinPSCW_info info = {
    .done = 0,
    .win  = win,
  };
  MPIDI_Context_post(MPIDI_Context[0], &info.work, MPIDI_WinPost_post, &info);
  TRACE_ERR("<%s>wait while info done !%u\n",__FUNCTION__,info.done);
  MPID_PROGRESS_WAIT_WHILE(!info.done);
  TRACE_ERR("<%s> wait done\n",__FUNCTION__);

  win->mpid.sync.target_epoch_type = MPID_EPOTYPE_POST;

  return mpi_errno;
}


int
MPID_Win_wait(MPID_Win *win)
{
  int mpi_errno = MPI_SUCCESS;
  static char FCNAME[] = "MPID_Win_wait";
  struct MPIDI_Win_sync* sync = &win->mpid.sync;

  if(win->mpid.sync.target_epoch_type != MPID_EPOTYPE_POST){
    MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                        return mpi_errno, "**rmasync");
  }

  MPID_Group *group = sync->pw.group;
  TRACE_ERR("<%s>wait while PW %u != %u, group %p, PW group %p, PW group %p\n",__FUNCTION__,group->size,sync->pw.count, group, sync->pw.group, sync->pw.group);
  MPID_PROGRESS_WAIT_WHILE(group->size != sync->pw.count);
  TRACE_ERR("<%s> wait done\n",__FUNCTION__);
  sync->pw.count = 0;
  TRACE_ERR("<%s>Free PW group %p\n",__FUNCTION__,sync->pw.group);
  sync->pw.group = NULL;

  MPIR_Group_release(group);

  if(win->mpid.sync.origin_epoch_type == MPID_EPOTYPE_REFENCE){
    win->mpid.sync.target_epoch_type = MPID_EPOTYPE_REFENCE;
  }else{
    win->mpid.sync.target_epoch_type = MPID_EPOTYPE_NONE;
  }
  return mpi_errno;
}


int
MPID_Win_test(MPID_Win *win,
              int      *flag)
{
  int mpi_errno = MPI_SUCCESS;
  static char FCNAME[] = "MPID_Win_test";
  struct MPIDI_Win_sync* sync = &win->mpid.sync;

  if(win->mpid.sync.target_epoch_type != MPID_EPOTYPE_POST){
    MPIU_ERR_SETANDSTMT(mpi_errno, MPI_ERR_RMA_SYNC,
                        return mpi_errno, "**rmasync");
  }

  MPID_Group *group = sync->pw.group;
  if (group->size == sync->pw.count)
    {
      sync->pw.count = 0;
      TRACE_ERR("<%s>PW count 0 and Free PW group %p\n",__FUNCTION__,sync->pw.group);
      sync->pw.group = NULL;
      *flag = 1;
      MPIR_Group_release(group);
      if(win->mpid.sync.origin_epoch_type == MPID_EPOTYPE_REFENCE){
        win->mpid.sync.target_epoch_type = MPID_EPOTYPE_REFENCE;
      }else{
        win->mpid.sync.target_epoch_type = MPID_EPOTYPE_NONE;
      }
    }
  else
    {
      MPID_Progress_poke();
    }

  return mpi_errno;
}
