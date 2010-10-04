/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_done.c
 * \brief "Done" call-backs provided to the message layer for signaling completion
 */
#include "mpidimpl.h"


/**
 * \brief Message layer callback which is invoked on the origin node
 * when the send of the message is done
 *
 * \param[in,out] sreq MPI receive request object
 */
void MPIDI_SendDoneCB(pami_context_t   context,
                      void           * clientdata,
                      pami_result_t    result)
{
  MPID_Request * sreq = (MPID_Request*)clientdata;
  MPID_assert(sreq != NULL);

  if (sreq->mpid.uebuf != NULL)
    {
      MPIU_Free(sreq->mpid.uebuf);
      sreq->mpid.uebuf = NULL;
    }

    MPIDI_Request_complete(sreq);
}


static inline void
MPIDI_RecvDoneCB_copy(MPID_Request * rreq)
{
  int smpi_errno;
  MPID_assert(rreq->mpid.uebuf != NULL);
  // It is unsafe to check the user buffer against NULL.
  // Believe it or not, an IRECV can legally be posted with a NULL buffer.
  // MPID_assert(rreq->mpid.userbuf != NULL);
  MPIDI_msg_sz_t _count=0;
  MPIDI_Buffer_copy(rreq->mpid.uebuf,        /* source buffer */
                    rreq->mpid.uebuflen,
                    MPI_CHAR,
                    &smpi_errno,
                    rreq->mpid.userbuf,      /* dest buffer */
                    rreq->mpid.userbufcount, /* dest count */
                    rreq->mpid.datatype,     /* dest type */
                    &_count,
                    &rreq->status.MPI_ERROR);
  rreq->status.count = _count;
}


/**
 * \brief Message layer callback which is invoked on the target node
 * when the incoming message is complete.
 *
 * \param[in,out] rreq MPI receive request object
 */
void MPIDI_RecvDoneCB(pami_context_t   context,
                      void           * clientdata,
                      pami_result_t    result)
{
  MPID_Request * rreq = (MPID_Request*)clientdata;
  MPID_assert(rreq != NULL);
  switch (MPIDI_Request_getCA(rreq))
    {
    case MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE:
      {
        MPIDI_RecvDoneCB_copy(rreq);
        /* free the unexpected data buffer */
        MPIU_Free(rreq->mpid.uebuf);
        rreq->mpid.uebuf = NULL;
        break;
      }
    case MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE:
      {
        MPIDI_RecvDoneCB_copy(rreq);
        break;
      }
    case MPIDI_CA_COMPLETE:
      {
        break;
      }
    default:
      {
        MPID_Abort(NULL, MPI_ERR_OTHER, -1, "Internal: unknown CA");
        break;
      }
    }
  MPIDI_Request_complete(rreq);
}
