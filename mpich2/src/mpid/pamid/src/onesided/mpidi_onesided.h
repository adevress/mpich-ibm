/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpidi_onesided.h
 * \brief ???
 */
#include <mpidimpl.h>

#ifndef __src_onesided_mpidi_onesided_h__
#define __src_onesided_mpidi_onesided_h__


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

typedef enum
  {
    MPIDI_WIN_REQUEST_ACCUMULATE,
    MPIDI_WIN_REQUEST_GET,
    MPIDI_WIN_REQUEST_PUT,
  } MPIDI_Win_requesttype_t;


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


typedef struct
{
  MPI_Datatype    type;
  MPID_Datatype * pointer;
  int             contig;
  MPI_Aint        true_lb;
  MPIDI_msg_sz_t  size;

  int             num_contig;
  DLOOP_VECTOR  * map;
  DLOOP_VECTOR    __map;
} MPIDI_Datatype;


/** \todo make sure that the extra fields are removed */
typedef struct
{
  MPID_Win *win;
  MPIDI_Win_requesttype_t type;
  pami_endpoint_t dest;

  struct
  {
    void * addr;
    int    count;
    MPI_Datatype datatype;
  } origin;

  pami_memregion_t memregion;
  /* uint32_t         memregion_used; */

  uint32_t ops_started;
  uint32_t ops_complete;

  MPIDI_Datatype origin_dt;
  MPIDI_Datatype target_dt;

  void * pack_buffer;
  /* size_t pack_length; */
  uint32_t pack_free;
} MPIDI_Win_request;



void
MPIDI_Win_datatype_basic(int              count,
                         MPI_Datatype     datatype,
                         MPIDI_Datatype * dt);
void
MPIDI_Win_datatype_map  (MPIDI_Datatype * dt);
void
MPIDI_Win_datatype_unmap(MPIDI_Datatype * dt);

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

void
MPIDI_DoneCB(pami_context_t  context,
             void          * cookie,
             pami_result_t   result);


#endif
