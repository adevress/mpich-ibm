/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_recvq.c
 * \brief Functions to manage the Receive Queues
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <mpidimpl.h>
#include "mpid_recvq.h"

/**
 * \defgroup MPID_RECVQ MPID Receive Queue management
 *
 * Functions to manage the Receive Queues
 */



/** \brief Structure to group the common recvq pointers */
struct MPIDI_Recvq_t MPIDI_Recvq;

#ifdef HAVE_DEBUGGER_SUPPORT
struct MPID_Request ** const MPID_Recvq_posted_head_ptr =
  &MPIDI_Recvq.posted_head;
struct MPID_Request ** const MPID_Recvq_unexpected_head_ptr =
  &MPIDI_Recvq.unexpected_head;
#endif /* HAVE_DEBUGGER_SUPPORT */


/**
 * \brief Set up the request queues
 */
void
MPIDI_Recvq_init()
{
  MPIDI_Recvq.posted_head = NULL;
  MPIDI_Recvq.posted_tail = NULL;
  MPIDI_Recvq.unexpected_head = NULL;
  MPIDI_Recvq.unexpected_tail = NULL;
}


/**
 * \brief Tear down the request queues
 */
void
MPIDI_Recvq_finalize()
{
  MPIDI_Recvq_DumpQueues(MPIDI_Process.verbose);
}


/**
 * \brief Find a request in the unexpected queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \return     The matching UE request or NULL
 */
int
MPIDI_Recvq_FU(int source, int tag, int context_id, MPI_Status * status)
{
  MPID_Request * rreq;
  int found = FALSE;
#ifdef USE_STATISTICS
  unsigned search_length = 0;
#endif

  if (tag != MPI_ANY_TAG && source != MPI_ANY_SOURCE)
    {
      rreq = MPIDI_Recvq.unexpected_head;
      while (rreq != NULL) {
#ifdef USE_STATISTICS
        ++search_length;
#endif
        if ( (MPIDI_Request_getMatchCtxt(rreq) == context_id) &&
             (MPIDI_Request_getMatchRank(rreq) == source    ) &&
             (MPIDI_Request_getMatchTag(rreq)  == tag       )
             )
          {
            found = TRUE;
            if(status != MPI_STATUS_IGNORE)
              *status = (rreq->status);
            break;
          }

        rreq = rreq->mpid.next;
      }
    }
  else
    {
      MPIDI_Message_match match;
      MPIDI_Message_match mask;

      match.context_id = context_id;
      mask.context_id = ~0;
      if (tag == MPI_ANY_TAG)
        {
          match.tag = 0;
          mask.tag = 0;
        }
      else
        {
          match.tag = tag;
          mask.tag = ~0;
        }
      if (source == MPI_ANY_SOURCE)
        {
          match.rank = 0;
          mask.rank = 0;
        }
      else
        {
          match.rank = source;
          mask.rank = ~0;
        }

      rreq = MPIDI_Recvq.unexpected_head;
      while (rreq != NULL) {
#ifdef USE_STATISTICS
        ++search_length;
#endif
        if ( (  MPIDI_Request_getMatchCtxt(rreq)              == match.context_id) &&
             ( (MPIDI_Request_getMatchRank(rreq) & mask.rank) == match.rank      ) &&
             ( (MPIDI_Request_getMatchTag(rreq)  & mask.tag ) == match.tag       )
             )
          {
            found = TRUE;
            if(status != MPI_STATUS_IGNORE)
              *status = (rreq->status);
            break;
          }

        rreq = rreq->mpid.next;
      }
    }

#ifdef USE_STATISTICS
  MPIDI_Statistics_time(MPIDI_Statistics.recvq.unexpected_search, search_length);
#endif

  return found;
}


/**
 * \brief Find a request in the unexpected queue and dequeue it
 * \param[in]  req        Find by address of request object on sender
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \return     The matching UE request or NULL
 */
MPID_Request *
MPIDI_Recvq_FDUR(MPID_Request * req, int source, int tag, int context_id)
{
  MPID_Request * prev_rreq          = NULL; /* previous request in queue */
  MPID_Request * cur_rreq           = NULL; /* current request in queue */
  MPID_Request * matching_cur_rreq  = NULL; /* matching request in queue */
  MPID_Request * matching_prev_rreq = NULL; /* previous in queue to match */
#ifdef USE_STATISTICS
  unsigned search_length = 0;
#endif

  /* ----------------------- */
  /* first we do the finding */
  /* ----------------------- */
  cur_rreq = MPIDI_Recvq.unexpected_head;
  while (cur_rreq != NULL) {
#ifdef USE_STATISTICS
    ++search_length;
#endif
    if (MPIDI_Request_getPeerRequest(cur_rreq) == req        &&
        MPIDI_Request_getMatchCtxt(cur_rreq)   == context_id &&
        MPIDI_Request_getMatchRank(cur_rreq)   == source     &&
        MPIDI_Request_getMatchTag(cur_rreq)    == tag)
      {
        matching_prev_rreq = prev_rreq;
        matching_cur_rreq  = cur_rreq;
        break;
      }
    prev_rreq = cur_rreq;
    cur_rreq  = cur_rreq->mpid.next;
  }

  /* ----------------------- */
  /* found nothing; return   */
  /* ----------------------- */
  if (matching_cur_rreq == NULL)
    goto fn_exit;

  MPIDI_Recvq_remove(MPIDI_Recvq.unexpected, matching_cur_rreq, matching_prev_rreq);

 fn_exit:
#ifdef USE_STATISTICS
  MPIDI_Statistics_time(MPIDI_Statistics.recvq.unexpected_search, search_length);
#endif

  return matching_cur_rreq;
}


/**
 * \brief Out of band part of find a request in the unexpected queue and dequeue it, or allocate a new request and enqueue it in the posted queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \param[out] foundp    TRUE iff the request was found
 * \return     The matching UE request or the new posted request
 */
MPID_Request *
MPIDI_Recvq_FDU(int source, int tag, int context_id, int * foundp)
{
  int found = FALSE;
  MPID_Request * rreq = NULL;
  MPID_Request * prev_rreq;
#ifdef USE_STATISTICS
  unsigned search_length = 0;
#endif

  if (tag != MPI_ANY_TAG && source != MPI_ANY_SOURCE)
    {
      prev_rreq = NULL;
      rreq = MPIDI_Recvq.unexpected_head;
      while (rreq != NULL) {
#ifdef USE_STATISTICS
        ++search_length;
#endif
        if ( (MPIDI_Request_getMatchCtxt(rreq) == context_id) &&
             (MPIDI_Request_getMatchRank(rreq) == source    ) &&
             (MPIDI_Request_getMatchTag(rreq)  == tag       )
             )
          {
            MPIDI_Recvq_remove(MPIDI_Recvq.unexpected, rreq, prev_rreq);
            found = TRUE;
            goto fn_exit;
          }

        prev_rreq = rreq;
        rreq = rreq->mpid.next;
      }
    }
  else
    {
      MPIDI_Message_match match;
      MPIDI_Message_match mask;

      match.context_id = context_id;
      mask.context_id = ~0;
      if (tag == MPI_ANY_TAG)
        {
          match.tag = 0;
          mask.tag = 0;
        }
      else
        {
          match.tag = tag;
          mask.tag = ~0;
        }
      if (source == MPI_ANY_SOURCE)
        {
          match.rank = 0;
          mask.rank = 0;
        }
      else
        {
          match.rank = source;
          mask.rank = ~0;
        }

      prev_rreq = NULL;
      rreq = MPIDI_Recvq.unexpected_head;
      while (rreq != NULL) {
#ifdef USE_STATISTICS
        ++search_length;
#endif
        if ( (  MPIDI_Request_getMatchCtxt(rreq)              == match.context_id) &&
             ( (MPIDI_Request_getMatchRank(rreq) & mask.rank) == match.rank      ) &&
             ( (MPIDI_Request_getMatchTag(rreq)  & mask.tag ) == match.tag       )
             )
          {
            MPIDI_Recvq_remove(MPIDI_Recvq.unexpected, rreq, prev_rreq);
            found = TRUE;
            goto fn_exit;
          }

        prev_rreq = rreq;
        rreq = rreq->mpid.next;
      }
    }

 fn_exit:
#ifdef USE_STATISTICS
  MPIDI_Statistics_time(MPIDI_Statistics.recvq.unexpected_search, search_length);
#endif

  *foundp = found;
  return rreq;
}


/**
 * \brief Find a request in the posted queue and dequeue it
 * \param[in]  req        Find by address of request object on sender
 * \return     The matching posted request or NULL
 */
int
MPIDI_Recvq_FDPR(MPID_Request * req)
{
  MPID_Request * cur_rreq  = NULL;
  MPID_Request * prev_rreq = NULL;
  int found = FALSE;
#ifdef USE_STATISTICS
  unsigned search_length = 0;
#endif

  cur_rreq = MPIDI_Recvq.posted_head;
  while (cur_rreq != NULL) {
#ifdef USE_STATISTICS
    ++search_length;
#endif
    if (cur_rreq == req)
      {
        MPIDI_Recvq_remove(MPIDI_Recvq.posted, cur_rreq, prev_rreq);
        found = TRUE;
        break;
      }

    prev_rreq = cur_rreq;
    cur_rreq = cur_rreq->mpid.next;
  }

#ifdef USE_STATISTICS
  MPIDI_Statistics_time(MPIDI_Statistics.recvq.posted_search, search_length);
#endif

  return found;
}


/**
 * \brief Find a request in the posted queue and dequeue it, or allocate a new request and enqueue it in the unexpected queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \param[out] foundp    TRUE iff the request was found
 * \return     The matching posted request or the new UE request
 */
MPID_Request *
MPIDI_Recvq_FDP_or_AEU(int source, int tag, int context_id, int * foundp)
{
  MPID_Request * rreq;
  int found = FALSE;

  rreq = MPIDI_Recvq_FDP(source, tag, context_id);
  if (rreq != NULL)
      found = TRUE;
  else
      rreq = MPIDI_Recvq_AEU(source, tag, context_id);

  *foundp = found;
  return rreq;
}


/**
 * \brief Allocate a new request and enqueue it in the unexpected queue
 * \param[in]  source     Find by Sender
 * \param[in]  tag        Find by Tag
 * \param[in]  context_id Find by Context ID (communicator)
 * \return     The matching posted request or the new UE request
 */
MPID_Request *
MPIDI_Recvq_AEU(int source, int tag, int context_id)
{
  /* A matching request was not found in the posted queue, so we
     need to allocate a new request and add it to the unexpected
     queue */
  MPID_Request *rreq;
  rreq = MPIDI_Request_create2();
  rreq->kind = MPID_REQUEST_RECV;
  MPIDI_Request_setMatch(rreq, tag, source, context_id);
  MPIDI_Recvq_append(MPIDI_Recvq.unexpected, rreq);

  return rreq;
}

/**
 * \brief Dump the queues
 */
void
MPIDI_Recvq_DumpQueues(int verbose)
{
  if(verbose == 0)
    return;

  MPID_Request * rreq = MPIDI_Recvq.posted_head;
  MPID_Request * prev_rreq = NULL;
  unsigned i=0, numposted=0, numue=0;
  unsigned postedbytes=0, uebytes=0;

  fprintf(stderr,"Posted Queue:\n");
  fprintf(stderr,"-------------\n");
  while (rreq != NULL) {
    if(verbose >= 2)
      fprintf(stderr, "P %d: MPItag=%d MPIrank=%d ctxt=%d count=%d\n",
              i++,
              MPIDI_Request_getMatchTag(rreq),
              MPIDI_Request_getMatchRank(rreq),
              MPIDI_Request_getMatchCtxt(rreq),
              rreq->mpid.userbufcount
              );
    numposted++;
    postedbytes+=rreq->mpid.userbufcount;
    prev_rreq = rreq;
    rreq = rreq->mpid.next;
  }
  fprintf(stderr, "Posted Requests %d, Total Mem: %d bytes\n",
          numposted, postedbytes);


  i=0;
  rreq = MPIDI_Recvq.unexpected_head;
  fprintf(stderr, "Unexpected Queue:\n");
  fprintf(stderr, "-----------------\n");
  while (rreq != NULL) {
    if(verbose >= 2)
      fprintf(stderr, "UE %d: MPItag=%d MPIrank=%d ctxt=%d uebuf=%p uebuflen=%u\n",
              i++,
              MPIDI_Request_getMatchTag(rreq),
              MPIDI_Request_getMatchRank(rreq),
              MPIDI_Request_getMatchCtxt(rreq),
              rreq->mpid.uebuf,
              rreq->mpid.uebuflen);
    numue++;
    uebytes+=rreq->mpid.uebuflen;
    prev_rreq = rreq;
    rreq = rreq->mpid.next;
  }
  fprintf(stderr, "Unexpected Requests %d, Total Mem: %d bytes\n",
          numue, uebytes);
}
