/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/alltoall/mpido_alltoall.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Alltoall = MPIDO_Alltoall

int
MPIDO_Alltoall(void *sendbuf,
               int sendcount,
               MPI_Datatype sendtype,
               void *recvbuf,
               int recvcount,
               MPI_Datatype recvtype,
               MPID_Comm * comm)
{
  int i, numprocs = comm->local_size;
  int tsndlen, trcvlen, snd_contig, rcv_contig, rc;
  DCMF_Embedded_Info_Set * properties;

  MPI_Aint sdt_true_lb, rdt_true_lb;
  MPID_Datatype *dt_null = NULL;

  if (sendcount == 0 || recvcount == 0)
    return MPI_SUCCESS;

  MPIDI_Datatype_get_info(sendcount, sendtype, snd_contig,
			  tsndlen, dt_null, sdt_true_lb);
  MPIDI_Datatype_get_info(recvcount, recvtype, rcv_contig,
			  trcvlen, dt_null, rdt_true_lb);

  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				   sdt_true_lb);
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   rdt_true_lb);

  properties = &(comm -> dcmf.properties);

  if (!snd_contig ||
      !rcv_contig ||
      tsndlen != trcvlen ||
      numprocs < 2 ||
      comm -> comm_kind != MPID_INTRACOMM)
    return MPIR_Alltoall(sendbuf, sendcount, sendtype,
			 recvbuf, recvcount, recvtype,
			 comm);

  if (!DCMF_AllocateAlltoallBuffers(comm))
    return MPIR_Err_create_code(MPI_SUCCESS,
				MPIR_ERR_RECOVERABLE,
				"MPI_Alltoall",
				__LINE__, MPI_ERR_OTHER, "**nomem", 0);
      
  /* ---------------------------------------------- */
  /* Initialize the send buffers and lengths        */
  /* pktInject is the number of packets to inject   */
  /* per advance loop.  The best performance is 2   */
  /* ---------------------------------------------- */
  for (i = 0; i < numprocs; i++)
    {
      comm->dcmf.sndlen [i] =     tsndlen;
      comm->dcmf.sdispls[i] = i * tsndlen;
      comm->dcmf.rcvlen [i] =     trcvlen;
      comm->dcmf.rdispls[i] = i * trcvlen;
    }

  /*
    DEFAULT code path or if coming here from within another collective.
    if the torus alltoall is available, use it, otherwise, MPICH.
   */

  if (!DCMF_INFO_ISSET(properties, DCMF_TORUS_ALLTOALL))
    return MPIR_Alltoall(sendbuf, sendcount, sendtype,
			 recvbuf, recvcount, recvtype,
			 comm);
  
  
  /* Create a message layer collective message      */
  /* ---------------------------------------------- */
  
  rc = MPIDO_Alltoall_torus((char *) sendbuf + sdt_true_lb,
			    sendcount,
			    sendtype,
			    (char *) recvbuf + rdt_true_lb,
			    recvcount,
			    recvtype,
			    comm);
  return rc;
}
