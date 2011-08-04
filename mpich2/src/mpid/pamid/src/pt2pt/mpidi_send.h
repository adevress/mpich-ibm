/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_send.h
 *
 * This is a generic inline send operator.  It groups code previous
 * found in mpid_{,i}{,s}send.c.  The hope is that the common code
 * will be easier to maintain, and that the inline nature of the flags
 * will ensure high performance.
 */
#include <mpidimpl.h>

#ifndef __src_pt2pt_mpidi_send_h__
#define __src_pt2pt_mpidi_send_h__


static inline void
MPIDI_SendDoneCB_inline(pami_context_t   context,
                        void           * clientdata,
                        pami_result_t    result)
{
  MPID_Request * sreq = (MPID_Request*)clientdata;
  MPID_assert(sreq != NULL);
  MPIU_TestFree(&sreq->mpid.uebuf);
  MPIDI_Request_complete(sreq);
}


#define MPIDI_Send_post(__func, __req)                          \
({                                                              \
  if (likely(MPIDI_Process.context_post > 0))                   \
    {                                                           \
      pami_context_t context = MPIDI_Context_local(__req);      \
                                                                \
      pami_result_t rc;                                         \
      rc = PAMI_Context_post(context,                           \
                             &(__req)->mpid.post_request,       \
                             __func,                            \
                             __req);                            \
      MPID_assert(rc == PAMI_SUCCESS);                          \
    }                                                           \
  else                                                          \
    {                                                           \
      __func(MPIDI_Context[0], __req);                          \
    }                                                           \
})


/**
 * \brief This is a generic inline verion of the various send functions.
 *
 * This is the same function prototype as MPID_{,I}{,S}send.c, but it
 * inludes two inline flags: is_blocking and is_sync.  These are not
 * meant to be truely variable, but are expected to be known at
 * compile time.  That *should* allow the compiler to remove any extra
 * code from this function that won't be used in the specific inlined
 * version.
 *
 * The PAMI_Context_post function may queue this send operation for
 * later handling and then return to the user.  Therefore, this
 * function has only two goals:
 *
 *   + Fill in the request oject with all relevant information, and
 *   + Do any reference counting that must be done before the function returns.
 *
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[in]  is_blocking    Is this a blocking request? (i.e. one of MPI_Send or MPI_Ssend)
 * \param[in]  is_sync        Is this a synchronous request? (i.e. of of MPI_Ssend or MPI_Issend)
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
static inline int
MPIDI_Send(const void    * buf,
           int             count,
           MPI_Datatype    datatype,
           int             rank,
           int             tag,
           MPID_Comm     * comm,
           int             context_offset,
           unsigned        is_blocking,
           unsigned        is_sync,
           MPID_Request ** request)
{
  MPID_Request * sreq = NULL;

  /* --------------------- */
  /* create a send request */
  /* --------------------- */

  sreq = MPIDI_Request_create2();
  *request = sreq;

  /* match info */
  MPIDI_Request_setMatch(sreq, tag, comm->rank, comm->context_id+context_offset);

  /* data buffer info */
  sreq->mpid.userbuf      = (void*)buf;
  sreq->mpid.userbufcount = count;
  sreq->mpid.datatype     = datatype;

  /* communicator & destination info */
  sreq->comm              = comm;  MPIR_Comm_add_ref(comm);
  MPIDI_Request_setPeerRank_comm(sreq, rank);

  /* message type info */
  sreq->kind = MPID_REQUEST_SEND;

  /* ----------------------------------------- */
  /*      start the message                    */
  /* ----------------------------------------- */
  if (is_sync)
    {
      MPIDI_Request_uncomplete(sreq);
      MPIDI_Send_post(MPIDI_Ssend_handoff, sreq);
    }
  else
    {
      MPIDI_Send_post(MPIDI_Send_handoff, sreq);
    }

  return MPI_SUCCESS;
}


#undef MPIDI_Send_post


#endif
