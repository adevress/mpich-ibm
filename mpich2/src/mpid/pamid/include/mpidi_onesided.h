/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_onesided.h
 * \brief ???
 */

#ifndef __include_mpidi_onesided_h__
#define __include_mpidi_onesided_h__

#include "mpidimpl.h"


/**
 * \brief One-sided Message Types
 */
typedef enum
  {
    MPIDI_WIN_MSGTYPE_LOCKACK,  /**< Lock acknowledge  */
    MPIDI_WIN_MSGTYPE_LOCKREQ,  /**< Lock window       */
    MPIDI_WIN_MSGTYPE_UNLOCK,   /**< Unlock window     */

    MPIDI_WIN_MSGTYPE_COMPLETE, /**< End a START epoch */
    MPIDI_WIN_MSGTYPE_POST,     /**< Begin POST epoch  */

#if JRATT_OLD_1S
    MPIDI_WIN_MSGTYPE_ACC,      /**< ACCUMULATE RMA operation */
    MPIDI_WIN_MSGTYPE_DT_IOV,   /**< Datatype iov payload */
    MPIDI_WIN_MSGTYPE_DT_MAP,   /**< Datatype map payload */
    MPIDI_WIN_MSGTYPE_FENCE,    /**< (not used) */
    MPIDI_WIN_MSGTYPE_GET,      /**< GET RMA operation */
    MPIDI_WIN_MSGTYPE_PUT,      /**< PUT RMA operation */
    MPIDI_WIN_MSGTYPE_START,    /**< (not used) */
    MPIDI_WIN_MSGTYPE_UNFENCE,  /**< (not used) */
    MPIDI_WIN_MSGTYPE_UNLOCKACK,/**< unlock acknowledge, with status */
    MPIDI_WIN_MSGTYPE_WAIT,     /**< (not used) */
#endif
  } MPIDI_Win_msgtype_t;

typedef struct
{
  MPIDI_Win_msgtype_t type;
  MPID_Win *win;
  union
  {
    struct
    {
      int type;
    } lock;
  } data;
} MPIDI_Win_control_t;



void
MPIDI_WinCtrlSend(pami_context_t       context,
                  MPIDI_Win_control_t *control,
                  pami_task_t          peer,
                  MPID_Win            *win);

void
MPIDI_WinLockReq_proc(pami_context_t              context,
                      const MPIDI_Win_control_t * info,
                      unsigned                    peer);
void
MPIDI_WinLockAck_proc(pami_context_t              context,
                      const MPIDI_Win_control_t * info,
                      unsigned                    peer);
void
MPIDI_WinUnlock_proc(pami_context_t              context,
                     const MPIDI_Win_control_t * info,
                     unsigned                    peer);

void
MPIDI_WinComplete_proc(pami_context_t              context,
                       const MPIDI_Win_control_t * info,
                       unsigned                    peer);
void
MPIDI_WinPost_proc(pami_context_t              context,
                   const MPIDI_Win_control_t * info,
                   unsigned                    peer);


#endif
