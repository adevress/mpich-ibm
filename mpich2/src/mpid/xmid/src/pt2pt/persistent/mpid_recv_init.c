/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/persistent/mpid_recv_init.c
 * \brief ???
 */

/* This creates and initializes a persistent recv request */

#include "mpidimpl.h"

int MPID_Recv_init(void * buf,
                   int count,
                   MPI_Datatype datatype,
                   int rank,
                   int tag,
                   MPID_Comm * comm,
                   int context_offset,
                   MPID_Request ** request)
{
  MPID_Request * rreq = MPID_Request_create();

  rreq->kind = MPID_PREQUEST_RECV;
  rreq->comm = comm;
  MPIR_Comm_add_ref(comm);
  MPIDI_Request_setMatch(rreq,tag,rank,comm->recvcontext_id + context_offset);
  rreq->mpid.userbuf = (char *) buf;
  rreq->mpid.userbufcount = count;
  rreq->mpid.datatype = datatype;
  rreq->partner_request = NULL;
  rreq->cc = 0;

  MPIDI_Request_setType(rreq, MPIDI_REQUEST_TYPE_RECV);
  if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
    {
      MPID_Datatype_get_ptr(datatype, rreq->mpid.datatype_ptr);
      MPID_Datatype_add_ref(rreq->mpid.datatype_ptr);
    }

  *request = rreq;
  return MPI_SUCCESS;
}
