/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_startmessage.c
 * \brief Funnel point for starting all MPI messages
 */
#include "mpidimpl.h"

/* ----------------------------------------------------------------------- */
/*          Helper function: gets the message underway once the request    */
/* and the buffers have been allocated.                                    */
/* ----------------------------------------------------------------------- */

static inline void
MPIDI_Send_eager(MPID_Request  * sreq,
                 xmi_endpoint_t  dest,
                 char          * sndbuf,
                 unsigned        sndlen)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.envelope.msginfo;

  xmi_send_t params = {
  send   : {
    dispatch : MPIDI_Protocols.Send,
    dest     : dest,
    header   : {
      iov_base : msginfo,
      iov_len  : sizeof(MPIDI_MsgInfo),
      },
    data     : {
      iov_base : sndbuf,
      iov_len  : sndlen,
    },
  },
  events : {
    cookie   : sreq,
    local_fn : MPIDI_SendDoneCB,
  },
  };

  xmi_result_t rc = XMI_ERROR;
  rc = XMI_Send(MPIDI_Context[0], &params);
  MPID_assert(rc == XMI_SUCCESS);
}


static inline void
MPIDI_Send_rzv(MPID_Request  * sreq,
               xmi_endpoint_t  dest,
               char          * sndbuf,
               unsigned        sndlen)
{
  /* Set the isRzv bit in the SEND request. This is important for
   * canceling requests.
   */
  MPIDI_Request_setRzv(sreq, 1);

  /* The rendezvous information, such as the origin/local/sender
   * node's send buffer and the number of bytes the origin node wishes
   * to send, is sent as the payload of the request-to-send (RTS)
   * message.
   */
  sreq->mpid.envelope.envelope.data   = sndbuf;
  sreq->mpid.envelope.envelope.length = sndlen;

  /* Do not specify a callback function to be invoked when the RTS
   * message has been sent. The MPI_Send is completed only when the
   * target/remote/receiver node has completed an XMI_Get from the
   * origin node and has then sent a rendezvous acknowledgement (ACK)
   * to the origin node to signify the end of the transfer.  When the
   * ACK message is received by the origin node the same callback
   * function is used to complete the MPI_Send as the non-rendezvous
   * case.
   */
  xmi_send_immediate_t params = {
  dispatch : MPIDI_Protocols.RTS,
  dest     : dest,
  header   : {
    iov_base : &sreq->mpid.envelope,
    iov_len  : sizeof(MPIDI_MsgEnvelope),
    },
  data     : {
    iov_base : NULL,
    iov_len  : 0,
  },
  };

  xmi_result_t rc = XMI_ERROR;
  rc = XMI_Send_immediate(MPIDI_Context[0], &params);
  MPID_assert(rc == XMI_SUCCESS);
}


static inline void
MPIDI_Send(MPID_Request  * sreq,
           char          * sndbuf,
           unsigned        sndlen)
{
  xmi_endpoint_t dest = XMI_Client_endpoint(MPIDI_Client, MPIDI_Request_getPeerRank(sreq), 0);


  xmi_task_t old_peer = MPIDI_Request_getPeerRank(sreq);
  MPIDI_Request_setPeerRank(sreq, MPIR_Process.comm_world->rank);


  /* Always use the short protocol when sndlen is zero.
   * Use the short/eager protocol when sndlen is less than the eager limit.
   */
  if (sndlen==0 || sndlen<MPIDI_Process.eager_limit)
    {
      MPIDI_Send_eager(sreq,
                       dest,
                       sndbuf,
                       sndlen);
    }
  /* Otherwise, use the default rendezvous protocol (glue implementation
   * that guarantees no unexpected data.
   */
  else
    {
      MPIDI_Send_rzv(sreq,
                     dest,
                     sndbuf,
                     sndlen);
    }


  MPIDI_Request_setPeerRank(sreq, old_peer);
}


/*
 * \brief Central function for all sends.
 * \param [in,out] sreq Structure containing all relevant info about the message.
 */
void
MPIDI_StartMsg(MPID_Request  * sreq)
{
  int data_sz, dt_contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_true_lb;

  /* ----------------------------------------- */
  /* prerequisites: not sending to a NULL rank */
  /* request already allocated                 */
  /* not sending to self                       */
  /* ----------------------------------------- */
  MPID_assert(sreq != NULL);

  /* ----------------------------------------- */
  /*   get the datatype info                   */
  /* ----------------------------------------- */
  MPIDI_Datatype_get_info(sreq->mpid.userbufcount,
                          sreq->mpid.datatype,
                          dt_contig,
                          data_sz,
                          dt_ptr,
                          dt_true_lb);

  /* ----------------------------------------- */
  /* contiguous data type                      */
  /* ----------------------------------------- */
  if (dt_contig)
    {
      MPID_assert(sreq->mpid.uebuf == NULL);
      MPIDI_Send(sreq, (char *)sreq->mpid.userbuf + dt_true_lb, data_sz);
      return;
    }

  /* ------------------------------------------- */
  /* allocate and populate temporary send buffer */
  /* ------------------------------------------- */
  if (sreq->mpid.uebuf == NULL)
    {
      MPID_Segment              segment;

      sreq->mpid.uebuf = MPIU_Malloc(data_sz);
      if (sreq->mpid.uebuf == NULL)
        {
          sreq->status.MPI_ERROR = MPI_ERR_NO_SPACE;
          sreq->status.count = 0;
          MPID_Abort(NULL, MPI_ERR_NO_SPACE, -1,
                     "Unable to allocate non-contiguous buffer");
        }

      DLOOP_Offset last = data_sz;
      MPID_Segment_init(sreq->mpid.userbuf,
                        sreq->mpid.userbufcount,
                        sreq->mpid.datatype,
                        &segment,0);
      MPID_Segment_pack(&segment, 0, &last, sreq->mpid.uebuf);
      MPID_assert(last == data_sz);
    }

  MPIDI_Send(sreq, sreq->mpid.uebuf, data_sz);
}
