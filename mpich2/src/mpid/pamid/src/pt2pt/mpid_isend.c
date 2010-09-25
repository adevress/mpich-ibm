/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpid_isend.c
 * \brief ADI level implemenation of MPI_Isend()
 */
#include "mpidimpl.h"
#include "mpidi_send.h"

#if 0
/**
 * \brief ADI level implemenation of MPI_Isend()
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 *
 * This is a slight variation on mpid_send.c - basically, we *always*
 * want to return a send request even if the request is already
 * complete (as is in the case of sending to a NULL rank).
 */
int MPID_Isend(const void    * buf,
               int             count,
               MPI_Datatype    datatype,
               int             rank,
               int             tag,
               MPID_Comm     * comm,
               int             context_offset,
               MPID_Request ** request)
{
  return MPIDI_Send(buf,
                    count,
                    datatype,
                    rank,
                    tag,
                    comm,
                    context_offset,
                    0,
                    0,
                    request);
}

#endif
