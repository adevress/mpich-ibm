/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpid_irecv.c
 * \brief ADI level implemenation of MPI_Irecv()
 */
#include <mpidimpl.h>
#include "mpidi_recv.h"

/**
 * \brief ADI level implemenation of MPI_Irecv()
 *
 * \param[in]  buf            The buffer to receive into
 * \param[in]  count          Number of expected elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The sending rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 *
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
int MPID_Irecv(void          * buf,
               int             count,
               MPI_Datatype    datatype,
               int             rank,
               int             tag,
               MPID_Comm     * comm,
               int             context_offset,
               MPID_Request ** request)
{
  return MPIDI_Recv(buf,
                    count,
                    datatype,
                    rank,
                    tag,
                    comm,
                    context_offset,
                    0,
                    MPI_STATUS_IGNORE,
                    request);
}
