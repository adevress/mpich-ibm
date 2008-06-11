/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/alltoallw/alltoall_algorithms.c
 * \brief ???
 */

#include "mpido_coll.h"

/**
 * **************************************************************************
 * \brief "Done" callback for collective alltoall message.
 * **************************************************************************
 */

void
alltoallw_cb_done(void *clientdata)
{
  volatile unsigned * work_left = (unsigned *) clientdata;
  * work_left = 0;
  MPID_Progress_signal();
  return;
}

int MPIDO_Alltoallw_torus(void *sendbuf,
			  int * sendcounts,
			  int * senddispls,
			  MPI_Datatype * sendtypes,
			  void * recvbuf,
			  int * recvcounts,
			  int * recvdispls,
			  MPI_Datatype *recvtypes,
			  MPID_Comm * comm)
{
   int rc;
   DCMF_CollectiveRequest_t request;
   volatile unsigned active = 1;
   DCMF_Callback_t callback = { alltoallw_cb_done, (void *) &active };
   DCMF_Geometry_t * geometry = &(comm->dcmf.geometry);   

   /* ignore some of the args passed in, used the one setup in comm ptr */
   unsigned * sndlen = comm -> dcmf.sndlen;
   unsigned * sdispls = comm -> dcmf.sdispls;
   unsigned * rcvlen = comm -> dcmf.rcvlen;
   unsigned * rdispls = comm -> dcmf.rdispls;
   unsigned * sndcounters = comm -> dcmf.sndcounters;
   unsigned * rcvcounters = comm -> dcmf.rcvcounters;

   rc = DCMF_Alltoallv(&MPIDI_CollectiveProtocols.alltoallv.torus,
                       &request,
                       callback,
                       DCMF_MATCH_CONSISTENCY,
                       geometry,
                       sendbuf,
                       sndlen,
                       sdispls,
                       recvbuf,
                       rcvlen,
                       rdispls,
                       sndcounters,
                       rcvcounters);

   MPID_PROGRESS_WAIT_WHILE(active);
   return rc;
}


