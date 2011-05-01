/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_recvq.h
 * \brief Accessors and actors for MPID Requests
 */
#ifndef  __src_pt2pt_mpidi_recvq_h__
#define  __src_pt2pt_mpidi_recvq_h__

#include <mpidimpl.h>


struct MPIDI_Recvq_t
{
  struct MPID_Request * posted_head;      /**< \brief The Head of the Posted queue */
  struct MPID_Request * posted_tail;      /**< \brief The Tail of the Posted queue */
  struct MPID_Request * unexpected_head;  /**< \brief The Head of the Unexpected queue */
  struct MPID_Request * unexpected_tail;  /**< \brief The Tail of the Unexpected queue */
};
extern struct MPIDI_Recvq_t MPIDI_Recvq;


#define MPIDI_Recvq_append(__Q, __req)                  \
({                                                      \
  /* ---------------------------------------------- */  \
  /*  The tail request should point to the new one  */  \
  /* ---------------------------------------------- */  \
  if (__Q ## _tail != NULL)                             \
      __Q ## _tail->mpid.next = __req;                  \
  else                                                  \
      __Q ## _head = __req;                             \
  /* ------------------------------------------ */      \
  /*  The tail should point to the new request  */      \
  /* ------------------------------------------ */      \
  __Q ## _tail = __req;                                 \
})


#define MPIDI_Recvq_remove(__Q, __req, __prev)          \
({                                                      \
  /* --------------------------------------------- */   \
  /*  Patch the next pointers to skip the request  */   \
  /* --------------------------------------------- */   \
  if (__prev != NULL)                                   \
    __prev->mpid.next = __req->mpid.next;               \
  else                                                  \
    __Q ## _head = __req->mpid.next;                    \
  /* ------------------------------------------- */     \
  /*  Set tail pointer if removing the last one  */     \
  /* ------------------------------------------- */     \
  if (__req->mpid.next == NULL)                         \
    __Q ## _tail = __prev;                              \
})


/**
 * \brief Find a request in the unexpected queue and dequeue it, or allocate a new request and enqueue it in the posted queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \param[out] foundp     TRUE iff the request was found
 * \return     The matching UE request or the new posted request
 */
static inline MPID_Request *
MPIDI_Recvq_FDU_or_AEP(int source, int tag, int context_id, int * foundp)
{
  MPID_Request * rreq = NULL;
  /* We have unexpected messages, so search unexpected queue */
  if (unlikely(MPIDI_Recvq.unexpected_head != NULL)) {
    rreq = MPIDI_Recvq_FDU(source, tag, context_id, foundp);
    if (*foundp == TRUE)
      return rreq;
  }

  /* A matching request was not found in the unexpected queue,
     so we need to allocate a new request and add it to the
     posted queue */
  rreq = MPIDI_Request_create2();
  rreq->kind = MPID_REQUEST_RECV;
  MPIDI_Request_setMatch(rreq, tag, source, context_id);
  MPIDI_Recvq_append(MPIDI_Recvq.posted, rreq);
  *foundp = FALSE;

  return rreq;
}


/**
 * \brief Find a request in the posted queue and dequeue it
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \return     The matching posted request or the new UE request
 */
static inline MPID_Request *
MPIDI_Recvq_FDP(size_t source, size_t tag, size_t context_id)
{
  MPID_Request * rreq;
  MPID_Request * prev_rreq = NULL;
#ifdef USE_STATISTICS
  unsigned search_length = 0;
#endif

  rreq = MPIDI_Recvq.posted_head;
  MPIDI_Mutex_sync(); //We may be retriving data stored by another thread
  while (rreq != NULL) {
#ifdef USE_STATISTICS
    ++search_length;
#endif

    int match_src = MPIDI_Request_getMatchRank(rreq);
    int match_tag = MPIDI_Request_getMatchTag(rreq);
    int match_ctx = MPIDI_Request_getMatchCtxt(rreq);

    int flag0  = (source == match_src);
    flag0     |= (match_src == MPI_ANY_SOURCE);
    int flag1  = (context_id == match_ctx);
    int flag2  = (tag == match_tag);
    flag2     |= (match_tag == MPI_ANY_TAG);
    int flag   = flag0 & flag1 & flag2;

#if 0
  if ((MPIDI_Request_getMatchRank(rreq)==source || MPIDI_Request_getMatchRank(rreq)==MPI_ANY_SOURCE) &&
      (MPIDI_Request_getMatchCtxt(rreq)==context_id) &&
      (MPIDI_Request_getMatchTag(rreq)  == tag  || MPIDI_Request_getMatchTag(rreq)  == MPI_ANY_TAG)
      )
#else
    if (flag)
#endif
      {
        MPIDI_Recvq_remove(MPIDI_Recvq.posted, rreq, prev_rreq);
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
