/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_done.c
 * \brief "Done" call-backs provided to the message layer for signaling completion
 */
#include <mpidimpl.h>


/**
 * \brief Message layer callback which is invoked on the origin node
 * when the send of the message is done
 *
 * \param[in,out] sreq MPI receive request object
 */
void
MPIDI_SendDoneCB(pami_context_t   context,
                 void           * clientdata,
                 pami_result_t    result)
{
  MPIDI_SendDoneCB_inline(context,
                          clientdata,
                          result);
}


static inline void
MPIDI_RecvDoneCB_copy(MPID_Request * rreq)
{
  int smpi_errno;
  MPID_assert(rreq->mpid.uebuf != NULL);
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
void
MPIDI_RecvDoneCB(pami_context_t   context,
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
#ifdef OUT_OF_ORDER_HANDLING
  int source;
  source = MPIDI_Request_getPeerRank_pami(rreq);
  if (MPIDI_In_cntr[source].n_OutOfOrderMsgs > 0) {
     MPIDI_Recvq_process_out_of_order_msgs(source, context);
  }
#endif
  MPIDI_Request_complete(rreq);
}


/**
 * \brief Thread-safe message layer callback which is invoked on the
 * target node when the incoming message is complete.
 *
 * \param[in,out] rreq MPI receive request object
 */
void
MPIDI_RecvDoneCB_mutexed(pami_context_t   context,
                         void           * clientdata,
                         pami_result_t    result)
{
  MPIU_THREAD_CS_ENTER(MSGQUEUE, 0);

  MPIDI_RecvDoneCB(context, clientdata, result);

  MPIU_THREAD_CS_EXIT(MSGQUEUE, 0);
}


#ifdef OUT_OF_ORDER_HANDLING
/**
 * \brief Checks if any of the messages in the out of order list is ready
 * to be processed, if so, process it
 */
void MPIDI_Recvq_process_out_of_order_msgs(pami_task_t src, pami_context_t context)
{
   /*******************************************************/
   /* If the next message to be processed in the          */
   /* a recv is posted, then process the message.         */
   /*******************************************************/
   MPIDI_In_cntr_t *in_cntr;
   MPID_Request *ooreq, *rreq, *prev_rreq;
   pami_get_simple_t xferP;
   MPIDI_msg_sz_t _count=0;
   int matched;

   in_cntr  = &MPIDI_In_cntr[src];
   prev_rreq = NULL;
   ooreq = in_cntr->OutOfOrderList;
   while((in_cntr->n_OutOfOrderMsgs !=0) && ((MPIDI_Request_getMatchSeq(ooreq) == (in_cntr->nMsgs+1)) || (MPIDI_Request_getMatchSeq(ooreq) == in_cntr->nMsgs)))
   {
      matched=0;
      matched=MPIDI_Search_recv_posting_queue(MPIDI_Request_getMatchRank(ooreq),MPIDI_Request_getMatchTag(ooreq),MPIDI_Request_getMatchCtxt(ooreq),&rreq);

      if (matched)  {
        /* process a completed message i.e. data is in EA   */
        if (MPIDI_Request_getMatchSeq(ooreq) == (in_cntr->nMsgs+ 1))
          in_cntr->nMsgs++;

        if (ooreq->mpid.nextR != NULL)  { /* recv is in the out of order list */
          MPIDI_Recvq_remove_req_from_ool(ooreq,in_cntr);
        }

        /* ----------------------------------------- */
        /*  Calculate message length for reception.  */
        /* ----------------------------------------- */
        unsigned dt_contig, dt_size;
        MPID_Datatype *dt_ptr;
        MPI_Aint dt_true_lb;
        MPIDI_Datatype_get_info(rreq->mpid.userbufcount,
                                rreq->mpid.datatype,
                                dt_contig,
                                dt_size,
                                dt_ptr,
                                dt_true_lb);
        if (unlikely(ooreq->mpid.uebuflen > dt_size))
          {
            rreq->status.count = dt_size;
            rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;
          }

        MPIDI_RecvMsg_Unexp(ooreq, rreq->mpid.userbuf, rreq->mpid.userbufcount, rreq->mpid.datatype);
        if(ooreq->mpid.uebuf == NULL) {
          MPIDI_Request_complete(rreq);
          rreq->status.count = ooreq->status.count;
        } else {
          MPIDI_Buffer_copy(ooreq->mpid.uebuf,
                            ooreq->mpid.uebuflen,
                            MPI_CHAR,
                            &ooreq->status.MPI_ERROR,
                            rreq->mpid.userbuf,
                            rreq->mpid.userbufcount,
                            rreq->mpid.datatype,
                            &_count,
                            &rreq->status.MPI_ERROR);

          MPIDI_Request_complete(rreq);
          rreq->status.count = _count;
        }
        MPIDI_Recvq_remove(MPIDI_Recvq.unexpected, ooreq, ooreq->mpid.prev);

      } else {
        if (MPIDI_Request_getMatchSeq(ooreq) == (in_cntr->nMsgs+ 1))
          in_cntr->nMsgs++;
        if (ooreq->mpid.nextR != NULL)  { /* recv is in the out of order list */
            MPIDI_Recvq_remove_req_from_ool(ooreq,in_cntr);
        }
      }
      if (in_cntr->n_OutOfOrderMsgs > 0)
        ooreq = in_cntr->OutOfOrderList;
   }  /* while */
}


/**
 * \brief  search the posted recv queue and if found eliminate the
 * element from the queue and return in *request; else return NULL
 */
int MPIDI_Search_recv_posting_queue(int src, int tag, int context_id,
                                   MPID_Request **request )
{
    MPID_Request * rreq;
    MPID_Request * prev_rreq = NULL;

    *request = NULL;
    rreq = MPIDI_Recvq.posted_head;
    while (rreq != NULL)
    {
        /* The communicator test is not yet correct */
        if ((MPIDI_Request_getMatchRank(rreq)==src || MPIDI_Request_getMatchRank(rreq)==MPI_ANY_SOURCE) &&
        (MPIDI_Request_getMatchCtxt(rreq)==context_id) &&
        (MPIDI_Request_getMatchTag(rreq)  == tag  || MPIDI_Request_getMatchTag(rreq)  == MPI_ANY_TAG)
        ) {
            MPIDI_Recvq_remove(MPIDI_Recvq.posted, rreq, prev_rreq);
            *request = rreq;
            return 1;
        }
        prev_rreq = rreq;
        rreq = rreq->mpid.next;

    }
    return 0;
}
#endif
