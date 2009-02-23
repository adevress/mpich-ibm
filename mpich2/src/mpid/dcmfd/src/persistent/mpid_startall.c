/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/persistent/mpid_startall.c
 * \brief ???
 */
#include "mpidimpl.h"
#include "../../../mpi/pt2pt/bsendutil.h"

int MPID_Startall(int count, MPID_Request * requests[])
{
  int rc=MPI_SUCCESS, i;
  for (i = 0; i < count; i++)
    {
      MPID_Request * const preq = requests[i];
      switch(MPID_Request_getType(preq))
        {
        case MPIDI_DCMF_REQUEST_TYPE_RECV:
          {
            rc = MPID_Irecv(preq->dcmf.userbuf,
                            preq->dcmf.userbufcount,
                            preq->dcmf.datatype,
                            MPID_Request_getMatchRank(preq),
                            MPID_Request_getMatchTag(preq),
                            preq->comm,
                            MPID_Request_getMatchCtxt(preq) - preq->comm->recvcontext_id,
                            &preq->partner_request);
            break;
          }
        case MPIDI_DCMF_REQUEST_TYPE_SEND:
          {
            rc = MPID_Isend(preq->dcmf.userbuf,
                            preq->dcmf.userbufcount,
                            preq->dcmf.datatype,
                            MPID_Request_getMatchRank(preq),
                            MPID_Request_getMatchTag(preq),
                            preq->comm,
                            MPID_Request_getMatchCtxt(preq) - preq->comm->context_id,
                            &preq->partner_request);
            break;
          }
        case MPIDI_DCMF_REQUEST_TYPE_RSEND:
          {
            rc = MPID_Irsend(preq->dcmf.userbuf,
                             preq->dcmf.userbufcount,
                             preq->dcmf.datatype,
                             MPID_Request_getMatchRank(preq),
                             MPID_Request_getMatchTag(preq),
                             preq->comm,
                             MPID_Request_getMatchCtxt(preq) - preq->comm->context_id,
                             &preq->partner_request);
            break;
          }
        case MPIDI_DCMF_REQUEST_TYPE_SSEND:
          {
            rc = MPID_Issend(preq->dcmf.userbuf,
                             preq->dcmf.userbufcount,
                             preq->dcmf.datatype,
                             MPID_Request_getMatchRank(preq),
                             MPID_Request_getMatchTag(preq),
                             preq->comm,
                             MPID_Request_getMatchCtxt(preq) - preq->comm->context_id,
                             &preq->partner_request);
            break;
          }
        case MPIDI_DCMF_REQUEST_TYPE_BSEND:
          {
            rc = MPIR_Bsend_isend(preq->dcmf.userbuf,
                                  preq->dcmf.userbufcount,
                                  preq->dcmf.datatype,
                                  MPID_Request_getMatchRank(preq),
                                  MPID_Request_getMatchTag(preq),
                                  preq->comm,
                                  BSEND_INIT,
                                  &preq->partner_request);
            break;
          }

        default:
          {
            rc = MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_FATAL, "MPID_Startall", __LINE__, MPI_ERR_INTERN,"**ch3|badreqtype","**ch3|badreqtype %d",MPID_Request_getType(preq));
          }

        } /* switch should end here, bug fixed. */

      if (rc == MPI_SUCCESS)
      {
        preq->status.MPI_ERROR = MPI_SUCCESS;
        if (MPID_Request_getType(preq) == MPIDI_DCMF_REQUEST_TYPE_BSEND)
          {
            preq->cc = 0;
            preq->cc_ptr = &preq->cc;  /* Complete BSENDs immediately */
          }
        else
          preq->cc_ptr = &preq->partner_request->cc;
      }
      else
      {
        /* If a failure occurs attempting to start the request,
          then we assume that partner request was not created,
          and stuff the error code in the persistent request.
          The wait and test routines will look at the error code
          in the persistent request if a partner request is not present. */
        preq->partner_request = NULL;
        preq->status.MPI_ERROR = rc;
        preq->cc_ptr = &preq->cc;
        preq->cc = 0;
      }
  } /* for */
  return rc;
}
