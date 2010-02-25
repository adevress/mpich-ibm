/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_isend_self.c
 * \brief Handle the case where the sends are posted to self
 */
#include "mpidimpl.h"

/**
 * \brief Handle the case where the sends are posted to self
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[in]  type           The type of send requested
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
int MPIDI_Isend_self(const void    * buf,
                     int             count,
                     MPI_Datatype    datatype,
                     int             rank,
                     int             tag,
                     MPID_Comm     * comm,
                     int             context_offset,
                     int             type,
                     MPID_Request ** request)
{
  MPIDI_Message_match match;
  MPID_Request * sreq;
  MPID_Request * rreq;
  int found;

  /* --------------------- */
  /* create a send request */
  /* --------------------- */

  if (!(sreq = MPIDI_Request_create()))
    {
      *request = NULL;
      int mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                           MPIR_ERR_FATAL,
                                           "mpid_send",
                                           __LINE__,
                                           MPI_ERR_OTHER,
                                           "**nomem", 0);
      return mpi_errno;
    }
  MPIDI_Request_setType (sreq, type);
  sreq->mpid.userbuf       = (char *)buf;
  sreq->mpid.userbufcount  = count;
  sreq->mpid.datatype      = datatype;
  sreq->status.count      = count;
  MPIDI_Request_setSelf (sreq, 1);

  /* ------------------------------------------ */
  /* attempt to find a matching receive request */
  /* ------------------------------------------ */

  match.rank              = rank;
  match.tag               = tag;
  match.context_id        = comm->context_id + context_offset;
  rreq = MPIDI_Recvq_FDP_or_AEU(match.rank, match.tag, match.context_id, &found);
  if (rreq == NULL)
    {
      int mpi_errno;
      MPID_Request_release(sreq);
      *request = NULL;
      mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                       MPIR_ERR_RECOVERABLE,
                                       "MPID_Isend",
                                       __LINE__,
                                       MPI_ERR_OTHER,
                                       "**nomem",
                                  0);
      return mpi_errno;
    }

  /* ------------------------------------------ */
  /* set rreq status.                           */
  /* ------------------------------------------ */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
   MPID_Datatype *dataptr;
   int size;
   if(HANDLE_GET_KIND(datatype) == HANDLE_KIND_BUILTIN)
   {
      size = MPID_Datatype_get_basic_size(datatype);
   }
   else
   {
      MPID_Datatype_get_ptr(datatype, dataptr);
      size = dataptr->size;
   }
   rreq->status.count      = count * size;

  if (found)
    {
      /* ------------------------------------------ */
      /* we found the posted receive                */
      /* ------------------------------------------ */
      MPIDI_msg_sz_t data_sz;

      MPIDI_Buffer_copy(buf,                      /* source buffer */
                            count,
                            datatype,
                            &sreq->status.MPI_ERROR,
                            rreq->mpid.userbuf,         /* dest buffer */
                            rreq->mpid.userbufcount,
                            rreq->mpid.datatype,
                            &data_sz,
                            &rreq->status.MPI_ERROR);

      rreq->status.count = data_sz;
      MPIDI_Request_complete(rreq);

      /* sreq has never been seen by the user or outside this thread,
         so it is safe to reset ref_count and cc */
      sreq->cc                   = 0;
      *request                   = sreq;
      sreq->comm                 = comm;
      sreq->kind                 = MPID_REQUEST_SEND;
      MPIDI_Request_setMatch(sreq, match.tag, match.rank, match.context_id);
      MPIR_Comm_add_ref(comm);
      sreq->status.count = data_sz;
      return MPI_SUCCESS;
    }

  else
    {
      /* ---------------------------------------------- */
      /* no corresponding posted receive has been found */
      /* we have added the new *unexpected* receive req */
      /* to the queue, and are attaching sreq to it.    */
      /* ---------------------------------------------- */
      if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
        {
          MPID_Datatype_get_ptr(datatype, sreq->mpid.datatype_ptr);
          MPID_Datatype_add_ref(sreq->mpid.datatype_ptr);
        }
      rreq->partner_request       = sreq;
      *request                    = sreq;
      sreq->comm                  = comm;
      sreq->kind                  = MPID_REQUEST_SEND;
      MPIDI_Request_setMatch(sreq,match.tag, match.rank, match.context_id);
      MPIR_Comm_add_ref(comm);
      MPIDI_Request_setSelf (rreq, 1); /* it's a self request */
      MPIDI_Progress_signal();         /* Signal any waiter.  */
      return MPI_SUCCESS;
    }
}
