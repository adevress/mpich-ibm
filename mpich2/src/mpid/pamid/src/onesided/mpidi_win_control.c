/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpidi_win_control.c
 * \brief ???
 */
#include "mpidi_onesided.h"


void
MPIDI_WinCtrlSend(pami_context_t       context,
                  MPIDI_Win_control_t *control,
                  pami_task_t          peer,
                  MPID_Win            *win)
{
  control->win = win->mpid.info[peer].win;

  pami_endpoint_t dest;
  pami_result_t rc;
  rc = PAMI_Endpoint_create(MPIDI_Client, peer, 0, &dest);
  MPID_assert(rc == PAMI_SUCCESS);

  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols.WinCtrl,
  dest     : dest,
  header   : {
    iov_base : control,
    iov_len  : sizeof(MPIDI_Win_control_t),
    },
  data     : {
    iov_base : NULL,
    iov_len  : 0,
  },
  };

  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
}


void
MPIDI_WinControlCB(pami_context_t    context,
                   void            * _contextid,
                   const void      * _control,
                   size_t            size,
                   const void      * sndbuf,
                   size_t            sndlen,
                   pami_endpoint_t   sender,
                   pami_recv_t     * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(sndlen == 0);
  MPID_assert(_control != NULL);
  MPID_assert(size == sizeof(MPIDI_Win_control_t));
  const MPIDI_Win_control_t *control = (const MPIDI_Win_control_t *)_control;
  pami_task_t senderrank;
  size_t      sendercontext;
  pami_result_t rc;
  rc = PAMI_Endpoint_query(sender, &senderrank, &sendercontext);
  MPID_assert(rc == PAMI_SUCCESS);
  /* size_t               contextid = (size_t)_contextid; */

  switch (control->type)
    {
    case MPIDI_WIN_MSGTYPE_LOCKREQ:
      MPIDI_WinLockReq_proc(context, control, senderrank);
      break;
    case MPIDI_WIN_MSGTYPE_LOCKACK:
      MPIDI_WinLockAck_proc(context, control, senderrank);
      break;
    case MPIDI_WIN_MSGTYPE_UNLOCK:
      MPIDI_WinUnlock_proc(context, control, senderrank);
      break;

    case MPIDI_WIN_MSGTYPE_COMPLETE:
      MPIDI_WinComplete_proc(context, control, senderrank);
      break;
    case MPIDI_WIN_MSGTYPE_POST:
      MPIDI_WinPost_proc(context, control, senderrank);
      break;

    default:
      fprintf(stderr, "Bad win-control type: 0x%08x  %d\n",
              control->type,
              control->type);
      MPID_abort();
    }

  MPIDI_Progress_signal();
}
