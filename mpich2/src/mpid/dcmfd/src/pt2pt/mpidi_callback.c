/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_callback.c
 * \brief The standard callback for a new message
 */
#include "mpidimpl.h"

/**
 * \brief The standard callback for a new message
 * \param[in]  clientdata Unused
 * \param[in]  msginfo    The 16-byte msginfo struct
 * \param[in]  count      The number of msginfo quads (1)
 * \param[in]  senderrank The sender's rank
 * \param[in]  sndlen     The length of the incoming data
 * \param[out] rcvlen     The amount we are willing to receive
 * \param[out] rcvbuf     Where we want to put the data
 * \param[out] cb_info    Callback information for message completion
 * \returns    Storage for the DCMF to use for managing the message
 */
DCMF_Request_t * MPIDI_BG2S_RecvCB(void                     * clientdata,
                                   const MPIDI_DCMF_MsgInfo * msginfo,
                                   unsigned                   count,
                                   unsigned                   senderrank,
                                   const unsigned             sndlen,
                                   unsigned                 * rcvlen,
                                   char                    ** rcvbuf,
                                   DCMF_Callback_t    * const cb_info)
{
  MPID_Request * rreq = NULL;
  int found;
  *rcvlen = sndlen;

  /* -------------------------- */
  /*      match request         */
  /* -------------------------- */
  MPIDI_Message_match match;
  match.rank              = msginfo->msginfo.MPIrank;
  match.tag               = msginfo->msginfo.MPItag;
  match.context_id        = msginfo->msginfo.MPIctxt;

  rreq = MPIDI_Recvq_FDP_or_AEU(match.rank, match.tag, match.context_id, &found);

  if (rreq == NULL)
    {
      /* ------------------------------------------------- */
      /* we have failed to match the request.              */
      /* allocate and initialize a request object instead. */
      /* ------------------------------------------------- */

      int mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                           MPIR_ERR_FATAL,
                                           "mpid_recv",
                                           __LINE__,
                                           MPI_ERR_OTHER,
                                           "**nomem", 0);
      rreq->status.MPI_ERROR = mpi_errno;
      rreq->status.count     = 0;
      MPID_Abort(NULL, mpi_errno, -1, "Cannot allocate message");
    }

  /* -------------------------------------- */
  /* Signal that the recv has been started. */
  /* -------------------------------------- */
  MPID_Progress_signal ();

  /* ------------------------ */
  /* copy in information      */
  /* ------------------------ */
  rreq->status.MPI_SOURCE = match.rank;
  rreq->status.MPI_TAG    = match.tag;
  MPID_Request_setPeerRank(rreq,senderrank);
  MPID_Request_setPeerRequest(rreq,msginfo->msginfo.req);
  MPID_Request_setSync(rreq, msginfo->msginfo.isSync);
  MPID_Request_setRzv(rreq, 0);

  /* -------------------------------------------------------- */
  /* we have enough information to fill in the callback info. */
  /* -------------------------------------------------------- */
  cb_info->function = MPIDI_DCMF_RecvDoneCB;
  cb_info->clientdata = (void *)rreq;

  /* ----------------------------------------- */
  /* figure out target buffer for request data */
  /* ----------------------------------------- */
  MPID_Request_setCA(rreq, MPIDI_DCMF_CA_COMPLETE);
  rreq->status.count = *rcvlen;
  if (found)
    {
      /* --------------------------- */
      /* request was already posted. */
      /* if synchronized, post ack.  */
      /* --------------------------- */
      if (msginfo->msginfo.isSync)
        MPIDI_DCMF_postSyncAck(rreq);

      /* -------------------------------------- */
      /* calculate message length for reception */
      /* calculate receive message "count"      */
      /* -------------------------------------- */
      unsigned dt_contig, dt_size;
      MPID_Datatype *dt_ptr;
      MPI_Aint dt_true_lb;
      MPIDI_Datatype_get_info (rreq->dcmf.userbufcount,
                               rreq->dcmf.datatype,
                               dt_contig,
                               dt_size,
                               dt_ptr,
                               dt_true_lb);

      /* -------------------------------------- */
      /* test for truncated message.            */
      /* -------------------------------------- */
      if (*rcvlen > dt_size)
        {
          *rcvlen = dt_size;
          rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;
          rreq->status.count = *rcvlen;
        }

      /* -------------------------------------- */
      /* if buffer is contiguous, we are done.  */
      /* -------------------------------------- */
      if (dt_contig)
        {

          rreq->dcmf.uebuf = NULL;
          rreq->dcmf.uebuflen = 0;
          *rcvbuf = (char *)rreq->dcmf.userbuf + dt_true_lb;

          /* Whitespace to sync lines of code with mpidi_callback_short.c  */
          /* ------------------------------------------------------------- */

          return &rreq->dcmf.msg;
        }

      /* --------------------------------------------- */
      /* buffer is non-contiguous. we need to allocate */
      /* a temporary buffer, and unpack later.         */
      /* --------------------------------------------- */
      else
        {
          MPID_Request_setCA(rreq, MPIDI_DCMF_CA_UNPACK_UEBUF_AND_COMPLETE);

          /*
           *
           * Whitespace to sync lines of code with mpidi_callback_short.c
           *
           */
        }
    }

  /* ------------------------------------------------------------- */
  /* fallback position: request was not posted or not contiguous   */
  /* We must allocate enough space to hold the message temporarily */
  /* the temporary buffer will be unpacked later.                  */
  /* ------------------------------------------------------------- */
  rreq->dcmf.uebuflen   = *rcvlen;
  if ((rreq->dcmf.uebuf = MPIU_Malloc (*rcvlen)) == NULL)
    {
      /* ------------------------------------ */
      /* creation of temporary buffer failed. */
      /* we are in trouble and must bail out. */
      /* ------------------------------------ */

      int mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                           MPIR_ERR_FATAL,
                                           "mpid_recv",
                                           __LINE__,
                                           MPI_ERR_OTHER,
                                           "**nomem", 0);
      rreq->status.MPI_ERROR = mpi_errno;
      rreq->status.count     = 0;
      MPID_Abort(NULL, mpi_errno, -1, "Cannot allocate unexpected buffer");
    }

  /* ------------------------------------------------ */
  /*         set up outgoing variables                */
  /* ------------------------------------------------ */
  *rcvbuf = rreq->dcmf.uebuf;

  return &rreq->dcmf.msg;
}
