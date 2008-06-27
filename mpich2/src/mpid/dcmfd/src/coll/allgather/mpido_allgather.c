/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgather/mpido_allgather.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Allgather = MPIDO_Allgather

int
MPIDO_Allgather(void *sendbuf,
               int sendcount,
               MPI_Datatype sendtype,
               void *recvbuf,
               int recvcount,
               MPI_Datatype recvtype,
               MPID_Comm * comm)
{
  /* *********************************
   * Check the nature of the buffers
   * *********************************
   */
  
  allgather_fptr func;
  DCMF_Embedded_Info_Set * coll_prop = &MPIDI_CollectiveProtocols.properties;
  DCMF_Embedded_Info_Set * comm_prop = &(comm -> dcmf.properties);
  MPIDO_Coll_config config = {1,1,1,1};
  MPID_Datatype * dt_null = NULL;
  MPI_Aint send_true_lb = 0;
  MPI_Aint recv_true_lb = 0;
  size_t send_size = 0;
  size_t recv_size = 0;

  char use_tree_reduce, use_tree_bcast, use_alltoall; 
  char use_rect_async, use_binom_async;

  
  int rc;

  /* no optimized allgather, punt to mpich */
  if (DCMF_INFO_ISSET(comm_prop, DCMF_USE_MPICH_ALLGATHER))
    return MPIR_Allgather(sendbuf, sendcount, sendtype,
			  recvbuf, recvcount, recvtype,
			  comm);

  if ((sendcount < 1 && sendbuf != MPI_IN_PLACE) || recvcount < 1)
    return MPI_SUCCESS;
   
  MPIDI_Datatype_get_info(recvcount,
			  recvtype,
			  config.recv_contig,
			  recv_size,
			  dt_null,
			  recv_true_lb);
  send_size = recv_size;
  recv_size *= comm->local_size;
  
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf
				   + recv_true_lb + comm -> local_size *
				   send_size);
  
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

  /* verify everyone's datatype contiguity */
  if (DCMF_INFO_ISSET(coll_prop, DCMF_USE_PREALLREDUCE_ALLGATHER))
    MPIDO_Allreduce(MPI_IN_PLACE, &config, 4, MPI_INT, MPI_BAND, comm);
  
  
  
  config.largecount = (sendcount > 32768);  

  use_tree_reduce = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_ALLREDUCE) &&
                    DCMF_INFO_ISSET(comm_prop, DCMF_USE_ALLREDUCE_ALLGATHER) &&
	            config.recv_contig &&
                    config.send_contig &&
 	            config.recv_continuous &&
                    recv_size % sizeof(int) == 0;


  use_tree_bcast = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_BCAST) &&
                   DCMF_INFO_ISSET(comm_prop, DCMF_USE_BCAST_ALLGATHER);
  
  use_alltoall = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TORUS_ALLTOALL) &&
                 DCMF_INFO_ISSET(comm_prop, DCMF_USE_ALLTOALL_ALLGATHER) &&
                 config.recv_contig && config.send_contig;
  
  use_binom_async = DCMF_INFO_ISSET(comm_prop,
				    DCMF_USE_ABINOM_BCAST_ALLGATHER) &&
                    DCMF_INFO_ISSET(comm_prop, DCMF_USE_ABINOM_BCAST) &&
                    config.recv_contig &&
	            config.send_contig;

  use_rect_async = DCMF_INFO_ISSET(comm_prop, 
				   DCMF_USE_ARECT_BCAST_ALLGATHER) &&
                   DCMF_INFO_ISSET(comm_prop, DCMF_USE_ARECT_BCAST) &&
	           config.recv_contig &&
	           config.send_contig;

  /*
    Benchmark data shows bcast is faster for larger messages, so if
    both bcast and reduce are available, use bcast >32768
  */
  if (use_tree_reduce && use_tree_bcast)
    if (config.largecount)
      func = MPIDO_Allgather_bcast;
    else
      func = MPIDO_Allgather_allreduce;
  
  /* we only can use allreduce, so use it regardless of size */
  else if (use_tree_reduce)
    func = MPIDO_Allgather_allreduce;
  
  /* or, we can only use tree, or sync version of rect and binom */
  else if (use_tree_bcast || (!use_rect_async && !use_binom_async))
    func = MPIDO_Allgather_bcast;
  
      /* no tree protocols (probably not comm_world) so use alltoall */
  else if (use_alltoall)
    func = MPIDO_Allgather_alltoall;
  
  else if (use_rect_async)
    if (!config.largecount)
      func = MPIDO_Allgather_bcast_rect_async;
    else
      func = MPIDO_Allgather_bcast;
  
  else if(use_binom_async)
    if (!config.largecount)
      func = MPIDO_Allgather_bcast_binom_async;
    else
      func = MPIDO_Allgather_bcast;
  
  else
    return MPIR_Allgather(sendbuf, sendcount, sendtype,
			  recvbuf, recvcount, recvtype,
			  comm);
  
  rc = (func)(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
	      send_true_lb, recv_true_lb, send_size, recv_size, comm);
  
  
   return rc;
}
