/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_recvq.h
 * \brief Accessors and actors for MPID Requests
 */
#ifndef  __src_pt2pt_mpidi_recvq_h__
#define  __src_pt2pt_mpidi_recvq_h__

#include <mpidimpl.h>


struct MPIDID_Recvq_t
{
  struct MPID_Request * posted_head;      /**< \brief The Head of the Posted queue */
  struct MPID_Request * posted_tail;      /**< \brief The Tail of the Posted queue */
  struct MPID_Request * unexpected_head;  /**< \brief The Head of the Unexpected queue */
  struct MPID_Request * unexpected_tail;  /**< \brief The Tail of the Unexpected queue */
};
extern struct MPIDID_Recvq_t recvq;

MPID_Request * MPIDI_Recvq_FDU_outofline(int source, int tag, int context_id, int * foundp);
MPID_Request * MPIDI_Recvq_AEU(int sndlen, const MPIDI_MsgInfo *msginfo);


/**
 * \brief Find a request in the unexpected queue and dequeue it, or allocate a new request and enqueue it in the posted queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \param[out] foundp    TRUE iff the request was found
 * \return     The matching UE request or the new posted request
 */
static inline MPID_Request *
MPIDI_Recvq_FDU_or_AEP(int source, int tag, int context_id, int * foundp)
{
    MPID_Request * rreq = NULL;
    *foundp = FALSE;
    //We have unexpected messages, so search unexpected queue
    if (unlikely(recvq.unexpected_head != NULL)) {
      rreq = MPIDI_Recvq_FDU_outofline(source, tag, context_id, foundp);
      if (*foundp == TRUE)
        return rreq;
    }

    /* A matching request was not found in the unexpected queue,
       so we need to allocate a new request and add it to the
       posted queue */
    rreq = MPIDI_Request_create2_fast();
    rreq->status.count      = 0;
    rreq->status.cancelled  = FALSE;
    rreq->status.MPI_ERROR  = MPI_SUCCESS;

    struct MPIDI_Request* mpid = &rreq->mpid;
    mpid->cancel_pending   = FALSE;
    mpid->next             = NULL;
    mpid->datatype_ptr     = NULL;
    mpid->uebuf            = NULL;
    mpid->uebuflen         = 0;
    MPIDI_Request_setRzv(rreq, 0);
    rreq->kind = MPID_REQUEST_RECV;
    MPIDI_Request_setMatch(rreq, tag, source, context_id);

    if (recvq.posted_tail != NULL)
      {
          recvq.posted_tail->mpid.next = rreq;
      }
    else
      {
        recvq.posted_head = rreq;
      }
    recvq.posted_tail = rreq;

    return rreq;
}


/**
 * \brief Find a request in the posted queue and dequeue it, or allocate a new request and enqueue it in the unexpected queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \return     The matching posted request or the new UE request
 */
static inline MPID_Request *
MPIDI_Recvq_FDP(int source, int tag, int context_id)
{
    MPID_Request * rreq;
    MPID_Request * prev_rreq = NULL;
#ifdef USE_STATISTICS
    unsigned search_length = 0;
#endif

    rreq = recvq.posted_head;
    while (rreq != NULL)
      {
#ifdef USE_STATISTICS
        ++search_length;
#endif

        if ((MPIDI_Request_getMatchRank(rreq)==source || MPIDI_Request_getMatchRank(rreq)==MPI_ANY_SOURCE) &&
            (MPIDI_Request_getMatchCtxt(rreq)==context_id) &&
            (MPIDI_Request_getMatchTag(rreq)  == tag  || MPIDI_Request_getMatchTag(rreq)  == MPI_ANY_TAG)
            )
          {
            if (prev_rreq != NULL)
              {
                prev_rreq->mpid.next = rreq->mpid.next;
              }
            else
              {
                recvq.posted_head = rreq->mpid.next;
              }
            if (rreq->mpid.next == NULL)
              {
                recvq.posted_tail = prev_rreq;
              }
#ifdef USE_STATISTICS
            MPIDI_Statistics_time(MPIDI_Statistics.recvq.unexpected_search, search_length);
#endif
            return rreq;
          }

        prev_rreq = rreq;
        rreq = rreq->mpid.next;
      }

#ifdef USE_STATISTICS
    MPIDI_Statistics_time(MPIDI_Statistics.recvq.unexpected_search, search_length);
#endif
    return NULL;
}


#endif
