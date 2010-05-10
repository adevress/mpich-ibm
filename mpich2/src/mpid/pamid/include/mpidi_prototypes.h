/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpid_prototypes.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpid_prototypes_h__
#define __include_mpid_prototypes_h__


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
/** \} */

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
 * \param[in] COND This is not a true parameter.  It is *specifically*
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
/** \} */

/**
 * \defgroup MPID_CALLBACKS MPID callbacks for communication
 *
 * These calls are used to manage message asynchronous start and completion
 *
 * \addtogroup MPID_CALLBACKS
 * \{
 */
pami_result_t MPIDI_SendMsg_handoff(pami_context_t context, void * sreq);
int MPIDI_RecvMsg(void          * buf,
                  int             count,
                  MPI_Datatype    datatype,
                  int             rank,
                  int             tag,
                  MPID_Comm     * comm,
                  int             context_offset,
                  MPI_Status    * status,
                  MPID_Request ** request);

void MPIDI_SendDoneCB   (pami_context_t   context,
                         void          * clientdata,
                         pami_result_t    result);

void MPIDI_RecvCB       (pami_context_t   context,
                         void          * _contextid,
                         void          * _msginfo,
                         size_t          msginfo_size,
                         void          * sndbuf,
                         size_t          sndlen,
                         pami_recv_t    * recv);
void MPIDI_RecvRzvCB    (pami_context_t   context,
                         void          * _contextid,
                         void          * _msginfo,
                         size_t          msginfo_size,
                         void          * sndbuf,
                         size_t          sndlen,
                         pami_recv_t    * recv);
void MPIDI_RecvDoneCB   (pami_context_t   context,
                         void          * clientdata,
                         pami_result_t    result);
void MPIDI_RecvRzvDoneCB(pami_context_t   context,
                         void          * cookie,
                         pami_result_t    result);
/** \} */


/** \brief Acknowledge an MPI_Ssend() */
void MPIDI_postSyncAck(pami_context_t context, MPID_Request * req);
/** \brief Cancel an MPI_Send(). */
void MPIDI_procCancelReq(pami_context_t context, const MPIDI_MsgInfo *info, pami_task_t peer);
/** \brief This is the general PT2PT control message call-back */
void MPIDI_ControlCB(pami_context_t   context,
                     void          * _contextid,
                     void          * _msginfo,
                     size_t          msginfo_size,
                     void          * sndbuf,
                     size_t          sndlen,
                     pami_recv_t    * recv);


/** \brief Helper function when sending to self  */
int MPIDI_Isend_self(const void    * buf,
                     int             count,
                     MPI_Datatype    datatype,
                     int             rank,
                     int             tag,
                     MPID_Comm     * comm,
                     int             context_offset,
                     MPID_Request ** request);

/** \brief Helper function to complete a rendevous transfer */
void MPIDI_RendezvousTransfer(pami_context_t context, MPID_Request * rreq);


void MPIDI_Comm_create      (MPID_Comm *comm);
void MPIDI_Comm_destroy     (MPID_Comm *comm);
void MPIDI_Coll_comm_create (MPID_Comm *comm);
void MPIDI_Coll_comm_destroy (MPID_Comm *comm);
void MPIDI_Env_setup        ();
void MPIDI_Comm_world_setup ();

void MPIDI_Topo_Comm_create (MPID_Comm *comm);
void MPIDI_Topo_Comm_destroy(MPID_Comm *comm);
int  MPID_Dims_create       (int nnodes, int ndims, int *dims);

void MPIDI_Coll_Comm_create (MPID_Comm *comm);
void MPIDI_Coll_Comm_destroy(MPID_Comm *comm);
void MPIDI_Coll_register    (void);

int MPIDO_Bcast(void *buffer, int count, MPI_Datatype dt, int root, MPID_Comm *comm_ptr);
int MPIDO_Barrier(MPID_Comm *comm_ptr);
int MPIDO_Allreduce(void *sbuffer, void *rbuffer, int count, MPI_Datatype datatype, MPI_Op op, MPID_Comm *comm_ptr);

#endif
