/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidimpl.h
 * \brief API MPID additions to MPI functions and structures
 */

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef MPICH_MPIDIMPL_H_INCLUDED
#define MPICH_MPIDIMPL_H_INCLUDED

#include <xmix.h>
#include <mpiimpl.h>
#include <mpidpre.h>
#include <mpidpost.h>

/**
 * \addtogroup MPID_RECVQ
 * \{
 */
void MPIDI_Recvq_init();
void MPIDI_Recvq_finalize();
int            MPIDI_Recvq_FU        (int s, int t, int c, MPI_Status * status);
MPID_Request * MPIDI_Recvq_FDUR      (MPID_Request * req, int source, int tag, int context_id);
MPID_Request * MPIDI_Recvq_FDU_or_AEP(int s, int t, int c, int * foundp);
int            MPIDI_Recvq_FDPR      (MPID_Request * req);
MPID_Request * MPIDI_Recvq_FDP_or_AEU(int s, int t, int c, int * foundp);
void MPIDI_Recvq_DumpQueues          (int verbose);
/**\}*/

void MPIDI_Buffer_copy(const void     * const sbuf,
                            int                    scount,
                            MPI_Datatype           sdt,
                            int            *       smpi_errno,
                            void           * const rbuf,
                            int                    rcount,
                            MPI_Datatype           rdt,
                            MPIDI_msg_sz_t *       rsz,
                            int            *       rmpi_errno);

/**
 * \addtogroup MPID_PROGRESS
 * \{
 */
void MPID_Progress_start  (MPID_Progress_state * state);
void MPID_Progress_end    (MPID_Progress_state * state);
int  MPID_Progress_wait   (MPID_Progress_state * state);
int  MPID_Progress_poke   ();
int  MPID_Progress_test   ();
void MPIDI_Progress_signal();
/**
 * \brief A macro to easily implement advancing until a specific
 * condition becomes false.
 *
 * \param COND This is not a true parameter.  It is *specifically*
 * designed to be evaluated several times, allowing for the result to
 * change.  The condition would generally look something like
 * "(cb.client == 0)".  This would be used as the condition on a while
 * loop.
 *
 * \returns MPI_SUCCESS
 *
 * This correctly checks the condition before attempting to loop,
 * since the call to MPID_Progress_wait() may not return if the event
 * is already complete.  Any ssytem *not* using this macro *must* use
 * a similar check before waiting.
 */
#define MPID_PROGRESS_WAIT_WHILE(COND)          \
({                                              \
   if (COND)                                    \
     {                                          \
       MPID_Progress_state dummy;               \
                                                \
       MPID_Progress_start(&dummy);             \
       while (COND)                             \
         MPID_Progress_wait(&dummy);            \
       MPID_Progress_end(&dummy);               \
     }                                          \
    MPI_SUCCESS;                                \
})
/**\}*/


/**
 * \brief Gets significant info regarding the datatype
 * Used in mpid_send, mpidi_send.  Stolen from CH3 channel implementation.
 */
#define MPIDI_Datatype_get_info(_count, _datatype,              \
_dt_contig_out, _data_sz_out, _dt_ptr, _dt_true_lb)             \
({                                                              \
  if (HANDLE_GET_KIND(_datatype) == HANDLE_KIND_BUILTIN)        \
    {                                                           \
        (_dt_ptr)        = NULL;                                \
        (_dt_contig_out) = TRUE;                                \
        (_dt_true_lb)    = 0;                                   \
        (_data_sz_out)   = (_count) *                           \
        MPID_Datatype_get_basic_size(_datatype);                \
    }                                                           \
  else                                                          \
    {                                                           \
        MPID_Datatype_get_ptr((_datatype), (_dt_ptr));          \
        (_dt_contig_out) = (_dt_ptr)->is_contig;                \
        (_dt_true_lb)    = (_dt_ptr)->true_lb;                  \
        (_data_sz_out)   = (_count) * (_dt_ptr)->size;          \
    }                                                           \
})

/**
 * \addtogroup MPID_REQUEST
 * \{
 */

void    MPIDI_Request_complete(MPID_Request *req);
#define MPIDI_Request_add_ref(_req)                                     \
({                                                                      \
  MPID_assert(HANDLE_GET_MPI_KIND((_req)->handle) == MPID_REQUEST);     \
  MPIU_Object_add_ref(_req);                                            \
})

#define MPIDI_Request_decrement_cc(_req, _inuse) ({ *(_inuse) = --(*(_req)->cc_ptr)  ;                               })
#define MPIDI_Request_increment_cc(_req)         ({               (*(_req)->cc_ptr)++;                               })
#define MPIDI_Request_get_cc(_req)               ({                *(_req)->cc_ptr;                                  })

#define MPIDI_Request_getCA(_req)                ({ (_req)->mpid.ca;                                                 })
#define MPIDI_Request_isSelf(_req)               ({ (_req)->mpid.isSelf;                                             })
#define MPIDI_Request_getPeerRank(_req)          ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.peerrank;         })
#define MPIDI_Request_getPeerRequest(_req)       ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req;              })
#define MPIDI_Request_getType(_req)              ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.type;             })
#define MPIDI_Request_isSync(_req)               ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync;           })
#define MPIDI_Request_isRzv(_req)                ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv;            })
#define MPIDI_Request_getMatchTag(_req)          ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag;           })
#define MPIDI_Request_getMatchRank(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank;          })
#define MPIDI_Request_getMatchCtxt(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt;          })

#define MPIDI_Request_setCA(_req, _ca)           ({ (_req)->mpid.ca                                         = (_ca); })
#define MPIDI_Request_setSelf(_req,_t)           ({ (_req)->mpid.isSelf                                     = (_t);  })
#define MPIDI_Request_setPeerRank(_req,_r)       ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.peerrank = (_r);  })
#define MPIDI_Request_setPeerRequest(_req,_r)    ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req      = (_r);  })
#define MPIDI_Request_setType(_req,_t)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.type     = (_t);  })
#define MPIDI_Request_setSync(_req,_t)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync   = (_t);  })
#define MPIDI_Request_setRzv(_req,_t)            ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv    = (_t);  })
#define MPIDI_Request_setMatch(_req,_tag,_rank,_ctxtid)                 \
({                                                                      \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag=(_tag);         \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank=(_rank);       \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt=(_ctxtid);     \
})
/**\}*/


/**
 * \defgroup MPID_CALLBACKS MPID callbacks for communication
 *
 * These calls are used to manage message asynchronous start and completion
 */
/**
 * \addtogroup MPID_CALLBACKS
 * \{
 */
void MPIDI_StartMsg(MPID_Request * sreq);
void MPIDI_SendDoneCB(xmi_context_t   context,
                      void          * clientdata,
                      xmi_result_t    result);

int MPIDI_Irecv(void          * buf,
                int             count,
                MPI_Datatype    datatype,
                int             rank,
                int             tag,
                MPID_Comm     * comm,
                int             context_offset,
                MPI_Status    * status,
                MPID_Request ** request,
                char          * func);
void MPIDI_RecvCB(xmi_context_t   context,
                  void          * _contextid,
                  void          * _msginfo,
                  size_t          msginfo_size,
                  void          * sndbuf,
                  size_t          sndlen,
                  xmi_recv_t    * recv);
void MPIDI_RecvRzvCB(xmi_context_t   context,
                     void          * _contextid,
                     void          * _msginfo,
                     size_t          msginfo_size,
                     void          * sndbuf,
                     size_t          sndlen,
                     xmi_recv_t    * recv);
void MPIDI_RecvDoneCB(xmi_context_t   context,
                      void          * clientdata,
                      xmi_result_t    result);
void MPIDI_RecvRzvDoneCB(xmi_context_t   context,
                         void          * cookie,
                         xmi_result_t    result);
/** \} */


/** \brief Acknowledge an MPI_Ssend() */
int  MPIDI_postSyncAck  (xmi_context_t context, MPID_Request * req);
/** \brief Cancel an MPI_Send(). */
void MPIDI_procCancelReq(xmi_context_t context, const MPIDI_MsgInfo *info, size_t peer);
/** \brief This is the general PT2PT control message call-back */
void MPIDI_ControlCB(xmi_context_t   context,
                     void          * _contextid,
                     void          * _msginfo,
                     size_t          msginfo_size,
                     void          * sndbuf,
                     size_t          sndlen,
                     xmi_recv_t    * recv);
/**
 * \brief Mark a request as cancel-pending
 * \param[in]  _req  The request to cancel
 * \param[out] _flag The previous state
 */
#define MPIDI_Request_cancel_pending(_req, _flag)       \
({                                                      \
  *(_flag) = (_req)->mpid.cancel_pending;               \
  (_req)->mpid.cancel_pending = TRUE;                   \
})

#define MPIDI_VerifyBuffer(_src_buff, _dst_buff, _data_lb)              \
({                                                                      \
  if (_src_buff == MPI_IN_PLACE)                                        \
    _dst_buff = _src_buff;                                              \
  else                                                                  \
    {                                                                   \
      MPID_Ensure_Aint_fits_in_pointer(MPI_VOID_PTR_CAST_TO_MPI_AINT    \
                                       _src_buff + _data_lb);           \
      _dst_buff = (char *) _src_buff + _data_lb;                        \
    }                                                                   \
})

/** \brief Helper function when sending to self  */
int MPIDI_Isend_self(const void    * buf,
                     int             count,
                     MPI_Datatype    datatype,
                     int             rank,
                     int             tag,
                     MPID_Comm     * comm,
                     int             context_offset,
                     int             type,
                     MPID_Request ** request);

/** \brief Helper function to complete a rendevous transfer */
void MPIDI_RendezvousTransfer (xmi_context_t context,MPID_Request * rreq);


void MPIDI_Comm_create       (MPID_Comm *comm);
void MPIDI_Comm_destroy      (MPID_Comm *comm);
void MPIDI_Comm_setup_properties(MPID_Comm *comm, int initial_setup);
void MPIDI_Env_setup         ();

void MPIDI_Topo_Comm_create  (MPID_Comm *comm);
void MPIDI_Topo_Comm_destroy (MPID_Comm *comm);
int  MPID_Dims_create        (int nnodes, int ndims, int *dims);

void MPIDI_Coll_Comm_create  (MPID_Comm *comm);
void MPIDI_Coll_Comm_destroy (MPID_Comm *comm);
void MPIDI_Coll_register     (void);

#endif
