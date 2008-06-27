/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/gather/mpido_gather.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Gather = MPIDO_Gather

/* works for simple data types, assumes fast reduce is available */

int MPIDO_Gather(void *sendbuf,
                  int sendcount,
                  MPI_Datatype sendtype,
                  void *recvbuf,
                  int recvcount,
                  MPI_Datatype recvtype,
                  int root,
                  MPID_Comm *comm)
{
  DCMF_Embedded_Info_Set * properties = &(comm->dcmf.properties);
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;

  int success = 1, contig, send_bytes=-1, recv_bytes = 0;
  int rank = comm->rank;
  int rc;

  if (sendtype != MPI_DATATYPE_NULL && sendcount >= 0)
    {
      MPIDI_Datatype_get_info(sendcount, sendtype, contig,
			      send_bytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
  else
    success = 0;

  if (success && rank == root)
    {
      if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
	{
	  MPIDI_Datatype_get_info(recvcount, recvtype, contig,
				  recv_bytes, data_ptr, true_lb);
	  if (!contig) success = 0;
	}
      else
	success = 0;
    }

  MPIDO_Allreduce(MPI_IN_PLACE, &success, 1, MPI_INT, MPI_BAND, comm);

  if (!success || 
      sendcount < 2048 ||
      DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_GATHER) || 
      !DCMF_INFO_ISSET(properties, DCMF_USE_REDUCE_GATHER))
    return MPIR_Gather(sendbuf, sendcount, sendtype,
		       recvbuf, recvcount, recvtype,
		       root, comm);

  if (sendbuf != MPI_IN_PLACE)
    {
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				       true_lb);
      sendbuf = (char *) sendbuf + true_lb;
    }
  
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   true_lb);

  recvbuf = (char *) recvbuf + true_lb;

  return MPIDO_Gather_reduce(sendbuf, sendcount, sendtype,
			     recvbuf, recvcount, recvtype,
			     root, comm);    
}
