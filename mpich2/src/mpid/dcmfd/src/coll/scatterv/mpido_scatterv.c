/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/scatterv/mpido_scatterv.c
 * \brief ???
 */

#include "mpido_coll.h"

#pragma weak PMPIDO_Scatterv = MPIDO_Scatterv

int MPIDO_Scatterv(void *sendbuf,
                   int *sendcounts,
                   int *displs,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   int root,
                   MPID_Comm *comm_ptr)
{
  DCMF_Embedded_Info_Set * properties = &(comm_ptr->dcmf.properties);
  int rank = comm_ptr -> rank, np = comm_ptr -> local_size;
  int i, nbytes, sum, contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint true_lb=0;
  unsigned char alltoall_scatterv = 0, bcast_scatterv = 0;
  int info[2] = {1, 0}; /* 1: denotes success, 0 to be used in sum */

  for (i = 0; i < np; i++)
    if (sendcounts[i] > 0)
      info[1] += sendcounts[i];
  
  sum = info[1];
  
  alltoall_scatterv = DCMF_INFO_ISSET(properties, DCMF_ALLTOALLV_SCATTERV);
  bcast_scatterv = DCMF_INFO_ISSET(properties, DCMF_BCAST_SCATTERV);
  
  if(rank == root)
    {
      MPIDI_Datatype_get_info(1,
			      sendtype,
			      contig,
			      nbytes,
			      dt_ptr,
			      true_lb);
      if(recvtype == MPI_DATATYPE_NULL || recvcount <= 0 || !contig)
	info[0] = 0;
    }
  else
    {
      MPIDI_Datatype_get_info(1,
			      recvtype,
			      contig,
			      nbytes,
			      dt_ptr,
			      true_lb);
      if(sendtype == MPI_DATATYPE_NULL || !contig)
	info[0] = 0;
    }

  /* Make sure parameters are the same on all the nodes */
  /* specifically, noncontig on the receive */
  MPIDO_Allreduce(MPI_IN_PLACE,
		  info,
		  2,
		  MPI_INT,
		  MPI_BAND,
		  comm_ptr);

  if(comm_ptr -> comm_kind != MPID_INTRACOMM ||
     !info[0] ||
     !alltoall_scatterv ||
     !bcast_scatterv ||
     (bcast_scatterv && info[1] != sum))
    return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
			 recvbuf, recvcount, recvtype,
			 root, comm_ptr);
  
  
  if(rank == root)
    {
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT
				       sendbuf + true_lb);
      sendbuf = (char *) sendbuf + true_lb;
    }
  else
    {
      if(recvbuf != MPI_IN_PLACE)
	{
	  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT
					   recvbuf + true_lb);
	  recvbuf = (char *) recvbuf + true_lb;
	}
    }

  if (alltoall_scatterv)
    return MPIDO_Scatterv_alltoallv(sendbuf,
				    sendcounts,
				    displs,
				    sendtype,
				    recvbuf,
				    recvcount,
				    recvtype,
				    root,
				    comm_ptr);
  
  else
    return MPIDO_Scatterv_bcast(sendbuf,
				sendcounts,
				displs,
				sendtype,
				recvbuf,
				recvcount,
				recvtype,
				root,
				comm_ptr);
}
