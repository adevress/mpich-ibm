/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_prototypes.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_prototypes_h__
#define __include_mpidi_prototypes_h__


/**
 * \addtogroup MPID_RECVQ
 * \{
 */
void MPIDI_Recvq_init();
void MPIDI_Recvq_finalize();
int            MPIDI_Recvq_FU        (int s, int t, int c, MPI_Status * status);
MPID_Request * MPIDI_Recvq_FDUR      (MPID_Request * req, int source, int tag, int context_id);
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

pami_result_t MPIDI_SendMsg_handoff(pami_context_t context, void * sreq);
pami_result_t MPIDI_Isend_handoff(pami_context_t context, void * sreq);

void MPIDI_RecvMsg_Unexp(MPID_Request * rreq, void * buf, int count, MPI_Datatype datatype);

/**
 * \defgroup MPID_CALLBACKS MPID callbacks for communication
 *
 * These calls are used to manage message asynchronous start and completion
 *
 * \addtogroup MPID_CALLBACKS
 * \{
 */
void MPIDI_SendDoneCB   (pami_context_t    context,
                         void            * clientdata,
                         pami_result_t     result);
void MPIDI_RecvShortCB  (pami_context_t    context,
                         void            * cookie,
                         const void      * _msginfo,
                         size_t            msginfo_size,
                         const void      * sndbuf,
                         size_t            sndlen,
                         pami_endpoint_t   sender,
                         pami_recv_t     * recv);
void MPIDI_RecvCB       (pami_context_t    context,
                         void            * cookie,
                         const void      * _msginfo,
                         size_t            msginfo_size,
                         const void      * sndbuf,
                         size_t            sndlen,
                         pami_endpoint_t   sender,
                         pami_recv_t     * recv);
void MPIDI_RecvRzvCB    (pami_context_t    context,
                         void            * cookie,
                         const void      * _msginfo,
                         size_t            msginfo_size,
                         const void      * sndbuf,
                         size_t            sndlen,
                         pami_endpoint_t   sender,
                         pami_recv_t     * recv);
void MPIDI_RecvDoneCB   (pami_context_t    context,
                         void            * clientdata,
                         pami_result_t     result);
void MPIDI_RecvRzvDoneCB(pami_context_t    context,
                         void            * cookie,
                         pami_result_t     result);
/** \} */


/** \brief Acknowledge an MPI_Ssend() */
void MPIDI_SyncAck_post(pami_context_t context, MPID_Request * req, unsigned rank);
/** \brief This is the general PT2PT control message call-back */
void MPIDI_ControlCB(pami_context_t    context,
                     void            * cookie,
                     const void      * _msginfo,
                     size_t            msginfo_size,
                     const void      * sndbuf,
                     size_t            sndlen,
                     pami_endpoint_t   sender,
                     pami_recv_t     * recv);
void
MPIDI_WinControlCB(pami_context_t    context,
                   void            * cookie,
                   const void      * _control,
                   size_t            size,
                   const void      * sndbuf,
                   size_t            sndlen,
                   pami_endpoint_t   sender,
                   pami_recv_t     * recv);

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
void MPIDI_Comm_coll_query  (MPID_Comm *comm);
void MPIDI_Comm_coll_envvars(MPID_Comm *comm);
void MPIDI_Coll_register    (void);

int MPIDO_Bcast(void *buffer, int count, MPI_Datatype dt, int root, MPID_Comm *comm_ptr);
int MPIDO_Barrier(MPID_Comm *comm_ptr);
int MPIDO_Allreduce(void *sbuffer, void *rbuffer, int count,
                    MPI_Datatype datatype, MPI_Op op, MPID_Comm *comm_ptr);
int MPIDO_Allgather(void *sendbuf, int sendcount, MPI_Datatype sendtype,
                    void *recvbuf, int recvcount, MPI_Datatype recvtype,
                    MPID_Comm *comm_ptr);

int MPIDO_Allgatherv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
                     void *recvbuf, int *recvcounts, int *displs,
                     MPI_Datatype recvtype, MPID_Comm * comm_ptr);

int MPIDO_Gather(void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int root, MPID_Comm * comm_ptr);

int MPIDO_Scatter(void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  int root, MPID_Comm * comm_ptr);

int MPIDO_Scatterv(void *sendbuf, int *sendcounts, int *displs,
                   MPI_Datatype sendtype,
                   void *recvbuf, int recvcount, MPI_Datatype recvtype,
                   int root, MPID_Comm * comm_ptr);


int MPItoPAMI(MPI_Datatype dt, pami_dt *pdt, MPI_Op op, pami_op *pop, int *musupport);
void MPIopString(MPI_Op op, char *string);
pami_result_t MPIDI_Pami_post_wrapper(pami_context_t context, void *cookie);

#endif
