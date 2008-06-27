/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/alltoallv/mpido_alltoallv.c
 * \brief ???
 */

#include "mpido_coll.h"

#pragma weak PMPIDO_Alltoallv = MPIDO_Alltoallv

int
MPIDO_Alltoallv(void *sendbuf,
                int *sendcounts,
                int *senddispls,
                MPI_Datatype sendtype,
                void *recvbuf,
                int *recvcounts,
                int *recvdispls,
                MPI_Datatype recvtype,
                MPID_Comm *comm_ptr)
{
  DCMF_Embedded_Info_Set * properties = &(comm_ptr -> dcmf.properties);
  
  int numprocs = comm_ptr->local_size;
  int tsndlen, trcvlen, snd_contig, rcv_contig, rc ,i;
  MPI_Aint sdt_true_lb, rdt_true_lb;
  MPID_Datatype *dt_null = NULL;
  
  MPIDI_Datatype_get_info(1, sendtype, snd_contig, tsndlen,
			  dt_null, sdt_true_lb);
  MPIDI_Datatype_get_info(1, recvtype, rcv_contig, trcvlen,
			  dt_null, rdt_true_lb);
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				   sdt_true_lb);
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   rdt_true_lb);
  
  if (DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_ALLTOALLV) ||
      !DCMF_INFO_ISSET(properties, DCMF_USE_TORUS_ALLTOALLV) ||
      !snd_contig ||
      !rcv_contig ||
      tsndlen != trcvlen)
    return MPIR_Alltoallv(sendbuf, sendcounts, senddispls, sendtype,
			  recvbuf, recvcounts, recvdispls, recvtype,
			  comm_ptr);
  
  if (!DCMF_AllocateAlltoallBuffers(comm_ptr))
    return MPIR_Err_create_code(MPI_SUCCESS,
				MPIR_ERR_RECOVERABLE,
				"MPI_Alltoallv",
				__LINE__, MPI_ERR_OTHER, "**nomem", 0);

  /* ---------------------------------------------- */
  /* Initialize the send buffers and lengths        */
  /* pktInject is the number of packets to inject   */
  /* per advance loop.  The best performance is 2   */
  /* ---------------------------------------------- */
  for (i = 0; i < numprocs; i++)
    {
      comm_ptr->dcmf.sndlen [i] = tsndlen * sendcounts[i];
      comm_ptr->dcmf.sdispls[i] = tsndlen * senddispls[i];
      comm_ptr->dcmf.rcvlen [i] = trcvlen * recvcounts[i];
      comm_ptr->dcmf.rdispls[i] = trcvlen * recvdispls[i];
    }


  /* ---------------------------------------------- */
  /* Create a message layer collective message      */
  /* ---------------------------------------------- */

  rc = MPIDO_Alltoallv_torus((char *) sendbuf + sdt_true_lb,
			     sendcounts,
			     senddispls,
			     sendtype,
			     (char *)recvbuf + rdt_true_lb,
			     recvcounts,
			     recvdispls,
			     recvtype,
			     comm_ptr);
  return rc;
}
