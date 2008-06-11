/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgatherv/mpido_allgatherv.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Allgatherv = MPIDO_Allgatherv

int
MPIDO_Allgatherv(void *sendbuf,
		 int sendcount,
		 MPI_Datatype sendtype,
		 void *recvbuf,
		 int *recvcounts,
		 int *displs,
		 MPI_Datatype recvtype,
		 MPID_Comm * comm)
{
  /* function pointer to be used to point to approperiate algorithm */
  allgatherv_fptr func;

  /* Check the nature of the buffers */
  MPID_Datatype *dt_null = NULL;
  MPI_Aint send_true_lb  = 0;
  MPI_Aint recv_true_lb  = 0;
  size_t   send_size     = 0;
  size_t   recv_size     = 0;
  MPIDO_Coll_config config = {1,1,1,1};

  int i, rc, buffer_sum = 0;
  char use_tree_reduce, use_tree_bcast, use_alltoall;
  char use_binom_async, use_rect_async;

  DCMF_Embedded_Info_Set * properties = &(comm->dcmf.properties);
  
  if (comm -> comm_kind != MPID_INTRACOMM)
    return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
			   recvbuf, recvcounts, displs, recvtype,
			   comm);

  MPIDI_Datatype_get_info(1,
			  recvtype,
			  config.recv_contig,
			  recv_size,
			  dt_null,
			  recv_true_lb);
  
  
  if (sendbuf != MPI_IN_PLACE)
    {
      MPIDI_Datatype_get_info(sendcount,
			      sendtype,
			      config.send_contig,
			      send_size,
			      dt_null,
			      send_true_lb);
      
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT
				       sendbuf + send_true_lb);
    }
  
  if (displs[0])
    config.recv_continuous = 0;
  
  for (i = 1; i < comm -> local_size; i++)
    {
      buffer_sum += recvcounts[i - 1];
      if (buffer_sum != displs[i])
	{
	  config.recv_continuous = 0;
	  break;
	}
    }
  
  buffer_sum += recvcounts[comm -> local_size - 1];
  
  buffer_sum *= recv_size;
  
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   recv_true_lb + buffer_sum);
  
  if (MPIDI_CollectiveProtocols.allgatherv.preallreduce)
    MPIDO_Allreduce(MPI_IN_PLACE, &config, 4, MPI_INT, MPI_BAND, comm);

  config.largecount = (sendcount > 65536);
 
  if (!DCMF_CHECK_INFO(properties,
		       DCMF_ALLREDUCE_ALLGATHERV,
		       DCMF_ALLTOALL_ALLGATHERV,
		       DCMF_BCAST_ALLGATHERV,
		       DCMF_END_ARGS))
    return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
			   recvbuf, recvcounts, displs, recvtype,
			   comm);
      

  use_tree_reduce = DCMF_CHECK_INFO(properties, DCMF_TREE_ALLREDUCE ,
				    DCMF_ALLREDUCE_ALLGATHERV, DCMF_END_ARGS) &&
                    config.recv_contig &&
                    config.send_contig &&
                    config.recv_continuous &&
                    buffer_sum % sizeof(int) == 0;

  use_tree_bcast = DCMF_CHECK_INFO(properties,
				   DCMF_TREE_BCAST,
				   DCMF_BCAST_ALLGATHERV,
				   DCMF_END_ARGS);

  use_alltoall = DCMF_CHECK_INFO(properties,
				 DCMF_TORUS_ALLTOALL, 
				 DCMF_ALLTOALL_ALLGATHERV,
				 DCMF_END_ARGS) &&
             	 config.recv_contig && config.send_contig;
      
  use_binom_async = DCMF_CHECK_INFO(properties,
				    DCMF_ASYNC_BINOM_BCAST_ALLGATHERV,
				    DCMF_ASYNC_BINOM_BCAST,
				    DCMF_END_ARGS) &&
                    config.recv_contig &&
	            config.send_contig;

  use_rect_async = DCMF_CHECK_INFO(properties,
				   DCMF_ASYNC_RECT_BCAST_ALLGATHERV,
				   DCMF_ASYNC_RECT_BCAST,
				   DCMF_END_ARGS) &&
                   config.recv_contig &&
                   config.send_contig;
      
  if (use_tree_reduce && use_tree_bcast)
    if (config.largecount)
      func = MPIDO_Allgatherv_bcast;
    else
      func = MPIDO_Allgatherv_allreduce;

  /* we only can use allreduce, so use it regardless of size */
  else if (use_tree_reduce)
    func = MPIDO_Allgatherv_allreduce;
  
  /* or, we can only use bcast, so use it regardless of size */
  else if (use_tree_bcast || (!use_binom_async && !use_rect_async))
    func = MPIDO_Allgatherv_bcast;
  
  /* no tree protocols (probably not comm_world) so use alltoall */
  else if (use_alltoall)
    func = MPIDO_Allgatherv_alltoall;
  
  else if (use_rect_async)
    if (!config.largecount)
      func = MPIDO_Allgatherv_bcast_rect_async;
    else
      func = MPIDO_Allgatherv_bcast;
  
  else if (use_binom_async)
    if (!config.largecount)
      func = MPIDO_Allgatherv_bcast_binom_async;
    else
      func = MPIDO_Allgatherv_bcast;
  
  else
    return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
			   recvbuf, recvcounts, displs, recvtype,
			   comm);
  
  rc = (func)(sendbuf, sendcount, sendtype,
	      recvbuf, recvcounts, buffer_sum, displs, recvtype,
	      send_true_lb, recv_true_lb, send_size, recv_size,
	      comm);
  
  return rc;
}
