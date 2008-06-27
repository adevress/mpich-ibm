/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/alltoallw/mpido_alltoallw.c
 * \brief ???
 */

#include "mpido_coll.h"

#pragma weak PMPIDO_Alltoallw = MPIDO_Alltoallw


int
MPIDO_Alltoallw(void *sendbuf,
                int *sendcounts,
                int *senddispls,
                MPI_Datatype *sendtypes,
                void *recvbuf,
                int *recvcounts,
                int *recvdispls,
                MPI_Datatype *recvtypes,
                MPID_Comm *comm_ptr)
{
  DCMF_Embedded_Info_Set * properties = &(comm_ptr -> dcmf.properties);
  int numprocs = comm_ptr->local_size;
  int *tsndlen, *trcvlen, snd_contig, rcv_contig, rc ,i;
  MPI_Aint sdt_true_lb=0, rdt_true_lb=0;
  MPID_Datatype *dt_null = NULL;
  
  tsndlen = MPIU_Malloc(numprocs * sizeof(unsigned));
  trcvlen = MPIU_Malloc(numprocs * sizeof(unsigned));
  if(!tsndlen || !trcvlen)
    return MPIR_Err_create_code(MPI_SUCCESS,
				MPIR_ERR_RECOVERABLE,
				"MPI_Alltoallw",
				__LINE__, MPI_ERR_OTHER, "**nomem", 0);
  
  for(i=0;i<numprocs;i++)
    {
      MPIDI_Datatype_get_info(1, sendtypes[i], snd_contig, tsndlen[i],
			      dt_null, sdt_true_lb);
      MPIDI_Datatype_get_info(1, recvtypes[i], rcv_contig, trcvlen[i],
			      dt_null, rdt_true_lb);

      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				       sdt_true_lb);
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				       rdt_true_lb);


      if (DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_ALLTOALLW) ||
	  !DCMF_INFO_ISSET(properties, DCMF_USE_TORUS_ALLTOALLW) ||
	  !snd_contig ||
	  !rcv_contig ||
	  tsndlen[i] != trcvlen[i])
	{
	  if(tsndlen) MPIU_Free(tsndlen);
	  if(trcvlen) MPIU_Free(trcvlen);
	  return MPIR_Alltoallw(sendbuf, sendcounts, senddispls, sendtypes,
				recvbuf, recvcounts, recvdispls, recvtypes,
				comm_ptr);
	}
    }
  
  if (!DCMF_AllocateAlltoallBuffers(comm_ptr))
    return MPIR_Err_create_code(MPI_SUCCESS,
				MPIR_ERR_RECOVERABLE,
				"MPI_Alltoallw",
				__LINE__, MPI_ERR_OTHER, "**nomem", 0);
      
  /* ---------------------------------------------- */
  /* Initialize the send buffers and lengths        */
  /* pktInject is the number of packets to inject   */
  /* per advance loop.  The best performance is 2   */
  /* ---------------------------------------------- */
  for (i = 0; i < numprocs; i++)
    {
      comm_ptr->dcmf.sndlen [i] = tsndlen[i] * sendcounts[i];
      comm_ptr->dcmf.sdispls[i] = senddispls[i];

      comm_ptr->dcmf.rcvlen [i] = trcvlen[i] * recvcounts[i];
      comm_ptr->dcmf.rdispls[i] = recvdispls[i];
    }


  /* ---------------------------------------------- */
  /* Create a message layer collective message      */
  /* ---------------------------------------------- */

  rc = MPIDO_Alltoallw_torus((char *) sendbuf + sdt_true_lb,
			     sendcounts,
			     senddispls,
			     sendtypes,
			     (char *) recvbuf + rdt_true_lb,
			     recvcounts,
			     recvdispls,
			     recvtypes,
			     comm_ptr);
  
   if(tsndlen) MPIU_Free(tsndlen);
   if(trcvlen) MPIU_Free(trcvlen);

  return rc;
}
