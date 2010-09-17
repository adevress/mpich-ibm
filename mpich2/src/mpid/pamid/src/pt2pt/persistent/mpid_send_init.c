/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/persistent/mpid_send_init.c
 * \brief ???
 */
/* This creates and initializes a persistent send request */

#include "mpidimpl.h"

/**
 * ***************************************************************************
 *              create a persistent send template
 * ***************************************************************************
 */

static inline int
MPID_PSendRequest(const void    * buf,
                  int             count,
                  MPI_Datatype    datatype,
                  int             rank,
                  int             tag,
                  MPID_Comm     * comm,
                  int             context_offset,
                  MPID_Request ** request)
{
  (*request) = MPID_Request_create();

  (*request)->kind              = MPID_PREQUEST_SEND;
  (*request)->comm              = comm;
  MPIR_Comm_add_ref(comm);
  MPIDI_Request_setMatch((*request), tag, rank, comm->context_id+context_offset);
  (*request)->mpid.userbuf      = (void*)buf;
  (*request)->mpid.userbufcount = count;
  (*request)->mpid.datatype     = datatype;
  (*request)->partner_request   = NULL;
  //  (*request)->cc		= 0;
  MPID_cc_set (&(*request)->cc, 0);  

  if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
    {
      MPID_Datatype_get_ptr(datatype, (*request)->mpid.datatype_ptr);
      MPID_Datatype_add_ref((*request)->mpid.datatype_ptr);
    }

  return MPI_SUCCESS;
}

/**
 * ***************************************************************************
 *              simple persistent send
 * ***************************************************************************
 */

int MPID_Send_init(const void * buf,
                   int count,
                   MPI_Datatype datatype,
                   int rank,
                   int tag,
                   MPID_Comm * comm,
                   int context_offset,
                   MPID_Request ** request)
{
  int mpi_errno = MPID_PSendRequest(buf,
                                    count,
                                    datatype,
                                    rank,
                                    tag,
                                    comm,
                                    context_offset,
                                    request);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;
  MPIDI_Request_setPType((*request), MPIDI_REQUEST_PTYPE_SEND);
  return MPI_SUCCESS;
}

/**
 * ***************************************************************************
 *              persistent synchronous send
 * ***************************************************************************
 */

int MPID_Ssend_init(const void * buf,
                   int count,
                   MPI_Datatype datatype,
                   int rank,
                   int tag,
                   MPID_Comm * comm,
                   int context_offset,
                   MPID_Request ** request)
{
  int mpi_errno = MPID_PSendRequest(buf,
                                    count,
                                    datatype,
                                    rank,
                                    tag,
                                    comm,
                                    context_offset,
                                    request);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;
  MPIDI_Request_setPType((*request), MPIDI_REQUEST_PTYPE_SSEND);
  return MPI_SUCCESS;
}

/**
 * ***************************************************************************
 *              persistent buffered send
 * ***************************************************************************
 */

int MPID_Bsend_init(const void * buf,
                    int count,
                    MPI_Datatype datatype,
                    int rank,
                    int tag,
                    MPID_Comm * comm,
                    int context_offset,
                    MPID_Request ** request)
{
  int mpi_errno = MPID_PSendRequest(buf,
                                    count,
                                    datatype,
                                    rank,
                                    tag,
                                    comm,
                                    context_offset,
                                    request);
  if (mpi_errno != MPI_SUCCESS)
    return mpi_errno;
  MPIDI_Request_setPType((*request), MPIDI_REQUEST_PTYPE_BSEND);
  return MPI_SUCCESS;
}
