/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_control.c
 * \brief Interface to the control protocols used by MPID pt2pt
 */
#include "mpidimpl.h"


/**
 * \brief Send a high-priority msginfo struct (control data)
 *
 * \param[in] control  The pointer to the msginfo structure
 * \param[in] peerrank The node to whom the control message is to be sent
 */
static inline void
MPIDI_CtrlSend(pami_context_t context,
               MPIDI_MsgInfo * control,
               size_t peerrank)
{
  pami_task_t old_peer = control->msginfo.peerrank;
  control->msginfo.peerrank = MPIDI_Process.global.rank;


  pami_endpoint_t dest;
  PAMI_Endpoint_create(MPIDI_Client, peerrank, 0, &dest);

  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols.Control,
  dest     : dest,
  header   : {
    iov_base : control,
    iov_len  : sizeof(MPIDI_MsgInfo),
    },
  data     : {
    iov_base : NULL,
    iov_len  : 0,
  },
  };

  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);


  control->msginfo.peerrank = old_peer;
}


/**
 * \brief Message layer callback which is invoked on the target node
 * of a flow-control rendezvous operation.
 *
 * This callback is invoked when the data buffer on the origin node
 * has been completely transfered to the target node. The target node
 * must acknowledge the completion of the transfer to the origin node
 * with a control message and then complete the receive by releasing
 * the request object.
 *
 * \param[in,out] rreq MPI receive request object
 */
void MPIDI_RecvRzvDoneCB(pami_context_t   context,
                         void          * cookie,
                         pami_result_t    result)
{
  MPID_Request * rreq = (MPID_Request *)cookie;
  MPID_assert(rreq != NULL);

  /* Is it neccesary to save the original value of the 'type' field ?? */
  unsigned original_value = MPIDI_Request_getType(rreq);
  MPIDI_Request_setType(rreq, MPIDI_REQUEST_TYPE_RENDEZVOUS_ACKNOWLEDGE);
  MPIDI_CtrlSend(context,
                 &rreq->mpid.envelope.envelope.msginfo,
                 MPIDI_Request_getPeerRank(rreq));
  MPIDI_Request_setType(rreq, original_value);

#ifdef USE_PAMI_RDMA
  pami_result_t rc;
  rc = PAMI_Memregion_destroy(context, &rreq->mpid.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
#endif

  MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
}


/**
 * \brief Acknowledge an MPI_Ssend()
 *
 * \param[in] req The request element to acknowledge
 *
 * \return The same as MPIDI_CtrlSend()
 */
void
MPIDI_postSyncAck(pami_context_t context, MPID_Request * req)
{
  MPIDI_Request_setType(req, MPIDI_REQUEST_TYPE_SSEND_ACKNOWLEDGE);

  MPIDI_MsgInfo * info = &req->mpid.envelope.envelope.msginfo;
  unsigned        peer =  MPIDI_Request_getPeerRank(req);

  MPIDI_CtrlSend(context, info, peer);
}

/**
 * \brief Process an incoming MPI_Ssend() acknowledgment
 *
 * \param[in] info The contents of the control message as a MPIDI_MsgInfo struct
 * \param[in] peer The rank of the node sending the data
 */
static inline void
MPIDI_procSyncAck(pami_context_t context, const MPIDI_MsgInfo *info, unsigned peer)
{
  MPID_assert(info != NULL);
  MPID_Request *req = MPIDI_Msginfo_getPeerRequest(info);
  MPID_assert(req != NULL);

  if(req->mpid.state ==  MPIDI_SEND_COMPLETE)
    MPIDI_Request_complete(req);
  else
    req->mpid.state = MPIDI_ACKNOWLEGED;
}


/**
 * \brief Process an incoming MPI_Send() cancelation
 *
 * \param[in] info The contents of the control message as a MPIDI_MsgInfo struct
 * \param[in] peer The rank of the node sending the data
 */
void
MPIDI_procCancelReq(pami_context_t context, const MPIDI_MsgInfo *info, size_t peer)
{
  MPIDI_REQUEST_TYPE  type;
  MPIDI_MsgInfo       ackinfo;
  MPID_Request      * sreq = NULL;

  MPID_assert(info != NULL);
  MPID_assert(MPIDI_Msginfo_getPeerRequest(info) != NULL);

  sreq=MPIDI_Recvq_FDUR(MPIDI_Msginfo_getPeerRequest(info),
                        info->msginfo.MPIrank,
                        info->msginfo.MPItag,
                        info->msginfo.MPIctxt);
  if(sreq)
    {
      if (sreq->mpid.uebuf)
        {
          MPIU_Free(sreq->mpid.uebuf);
          sreq->mpid.uebuf = NULL;
        }
      MPID_Request_release(sreq);
      type = MPIDI_REQUEST_TYPE_CANCEL_ACKNOWLEDGE;
    }
  else
    {
      type = MPIDI_REQUEST_TYPE_CANCEL_NOT_ACKNOWLEDGE;
    }

  ackinfo.msginfo.type = type;
  MPIDI_Msginfo_cpyPeerRequest(&ackinfo, info);
  MPIDI_CtrlSend(context, &ackinfo, peer);
}


/**
 * \brief Process an incoming MPI_Send() cancelation result
 *
 * \param[in] info The contents of the control message as a MPIDI_MsgInfo struct
 * \param[in] peer The rank of the node sending the data
 */
static inline void
MPIDI_procCancelAck(pami_context_t context, const MPIDI_MsgInfo *info, size_t peer)
{
  MPID_assert(info != NULL);
  MPID_Request *req = MPIDI_Msginfo_getPeerRequest(info);
  MPID_assert(req != NULL);

  if(info->msginfo.type == MPIDI_REQUEST_TYPE_CANCEL_NOT_ACKNOWLEDGE)
    {
      req->mpid.cancel_pending = FALSE;
      MPIDI_Request_complete(req);
      return;
    }

  MPID_assert(info->msginfo.type == MPIDI_REQUEST_TYPE_CANCEL_ACKNOWLEDGE);
  MPID_assert(req->mpid.cancel_pending == TRUE);

  req->status.cancelled = TRUE;

  /*
   * Rendezvous Sends wait until a rzv ack is received to complete the
   * send. Since this request was canceled, no rzv ack will be sent
   * from the target node, and the send done callback must be
   * explicitly called here.
   */
  if (MPIDI_Request_isRzv(req))
    MPIDI_SendDoneCB(context, req, PAMI_SUCCESS);
  /*
   * This checks for a Sync-Send that hasn't been ACKed (and now will
   * never be acked), but has transfered the data.  When
   * MPIDI_SendDoneCB() was called for this one, it wouldn't have
   * called MPIDI_Request_complete() to decrement the CC.  Therefore,
   * we call it now to simulate an ACKed message.
   */
  if ( (MPIDI_Request_getType(req) == MPIDI_REQUEST_TYPE_SSEND) &&
       (req->mpid.state           == MPIDI_SEND_COMPLETE) )
    MPIDI_Request_complete(req);

  /*
   * Finally, this request has been faux-Sync-ACKed and
   * faux-RZV-ACKed.  We just set the state and do a normal
   * completion.  This will set the completion to 0, so that the user
   * can call MPI_Wait()/MPI_Test() to finish it off (unless the
   * message is still in the MPIDI_INITIALIZED state, in which
   * case the done callback will finish it off).
   */
  req->mpid.state=MPIDI_REQUEST_DONE_CANCELLED;
  MPIDI_Request_complete(req);
}


/**
 * \brief Process an incoming rendezvous acknowledgment from the
 * target (remote) node and complete the MPI_Send() on the origin
 * (local) node.
 *
 * \param[in] info The contents of the control message as a MPIDI_MsgInfo struct
 * \param[in] peer The rank of the node sending the data
 */
static inline void MPIDI_procRzvAck(pami_context_t context, const MPIDI_MsgInfo *info, size_t peer)
{
  MPID_assert(info != NULL);
  MPID_Request *req = MPIDI_Msginfo_getPeerRequest(info);
  MPID_assert(req != NULL);

#ifdef USE_PAMI_RDMA
  pami_result_t rc;
  rc = PAMI_Memregion_destroy(context, &req->mpid.envelope.envelope.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
#endif

  MPIDI_SendDoneCB(context, req, PAMI_SUCCESS);
}


/**
 * \brief This is the general PT2PT control message call-back
 */
void MPIDI_ControlCB(pami_context_t   context,
                     void          * _contextid,
                     void          * _msginfo,
                     size_t          msginfo_size,
                     void          * sndbuf,
                     size_t          sndlen,
                     pami_recv_t    * recv)
{
  MPID_assert(recv == NULL); /**< Uncomment when ticket #50 is fixed */
  MPID_assert(sndlen == 0);
  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));
  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  size_t peer = msginfo->msginfo.peerrank;
  /* size_t               contextid = (size_t)_contextid; */

  switch (msginfo->msginfo.type)
    {
    case MPIDI_REQUEST_TYPE_SSEND_ACKNOWLEDGE:
      MPIDI_procSyncAck(context, msginfo, peer);
      break;
    case MPIDI_REQUEST_TYPE_CANCEL_REQUEST:
      MPIDI_procCancelReq(context, msginfo, peer);
      break;
    case MPIDI_REQUEST_TYPE_CANCEL_ACKNOWLEDGE:
    case MPIDI_REQUEST_TYPE_CANCEL_NOT_ACKNOWLEDGE:
      MPIDI_procCancelAck(context, msginfo, peer);
      break;
    case MPIDI_REQUEST_TYPE_RENDEZVOUS_ACKNOWLEDGE:
      MPIDI_procRzvAck(context, msginfo, peer);
      break;
    default:
      fprintf(stderr, "Bad msginfo type: 0x%08x  %d\n",
              msginfo->msginfo.type,
              msginfo->msginfo.type);
      MPID_abort();
    }
  MPIDI_Progress_signal();
}
