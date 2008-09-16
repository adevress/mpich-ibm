/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgatherv/mpido_allgatherv.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Allgatherv = MPIDO_Allgatherv

#ifdef USE_CCMI_COLL

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
  allgatherv_fptr func = NULL;

  /* Check the nature of the buffers */
  MPID_Datatype *dt_null = NULL;
  MPI_Aint send_true_lb  = 0;
  MPI_Aint recv_true_lb  = 0;
  size_t   send_size     = 0;
  size_t   recv_size     = 0;
  MPIDO_Coll_config config = {1,1,1,1};

  int i, rc, buffer_sum = 0;
  char use_tree_reduce, use_alltoall, use_rect_async;

  DCMF_Embedded_Info_Set * comm_prop = &(comm->dcmf.properties);
  DCMF_Embedded_Info_Set * coll_prop = &MPIDI_CollectiveProtocols.properties;

  unsigned char userenvset = DCMF_INFO_ISSET(comm_prop,
                                             DCMF_ALLGATHERV_ENVVAR);

  if (DCMF_INFO_ISSET(comm_prop, DCMF_USE_MPICH_ALLGATHERV))
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
  
  for (i = 1; i < comm->local_size; i++)
  {
    buffer_sum += recvcounts[i - 1];
    if (buffer_sum != displs[i])
    {
      config.recv_continuous = 0;
      break;
    }
  }
  
  buffer_sum += recvcounts[comm->local_size - 1];
  
  buffer_sum *= recv_size;
  
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   recv_true_lb + buffer_sum);
  
  if (DCMF_INFO_ISSET(coll_prop, DCMF_USE_PREALLREDUCE_ALLGATHERV))  {
    STAR_info.internal_control_flow = 1;
    MPIDO_Allreduce(MPI_IN_PLACE, &config, 4, MPI_INT, MPI_BAND, comm);
    STAR_info.internal_control_flow = 0;
  }

  if (!STAR_info.enabled || STAR_info.internal_control_flow)
  {
    use_tree_reduce = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_ALLREDUCE) &&
                      DCMF_INFO_ISSET(comm_prop,
                                      DCMF_USE_ALLREDUCE_ALLGATHERV) &&
                      config.recv_contig &&
                      config.send_contig &&
                      config.recv_continuous &&
                      buffer_sum % sizeof(int) == 0;
    
    use_alltoall = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TORUS_ALLTOALL) &&
                   DCMF_INFO_ISSET(comm_prop, DCMF_USE_ALLTOALL_ALLGATHERV) &&
                   config.recv_contig &&
                   config.send_contig;

    use_rect_async = DCMF_INFO_ISSET(comm_prop,
                                     DCMF_USE_ARECT_BCAST_ALLGATHERV) &&
                     DCMF_INFO_ISSET(comm_prop, DCMF_USE_ARECT_BCAST) &&
                     config.recv_contig &&
                     config.send_contig;
    /*
    config.largecount = (sendcount > 65536); 
    use_tree_bcast = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_BCAST) &&
                     DCMF_INFO_ISSET(comm_prop, DCMF_USE_BCAST_ALLGATHERV);
    
    use_binom_async = DCMF_INFO_ISSET(comm_prop,
                                      DCMF_USE_ABINOM_BCAST_ALLGATHERV) &&
      DCMF_INFO_ISSET(comm_prop, DCMF_USE_ABINOM_BCAST) &&
      config.recv_contig &&
      config.send_contig;
    */

    if (buffer_sum <= 512 || userenvset)
    {
      if (use_alltoall && !use_tree_reduce)
        func = MPIDO_Allgatherv_alltoall;
      if (!func && use_tree_reduce)
        func = MPIDO_Allgatherv_allreduce;
    }

    if (!func && (buffer_sum <= 1024 || userenvset))
    {
      if (use_tree_reduce)
        func = MPIDO_Allgatherv_allreduce;
      if (!func && use_rect_async)
        func = MPIDO_Allgatherv_bcast_rect_async;
    }

    if (!func && buffer_sum > 1024)
      if (use_rect_async)
        func = MPIDO_Allgatherv_bcast_rect_async;

    if (!func)
      return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
                             recvbuf, recvcounts, displs, recvtype,
                             comm);

    rc = (func)(sendbuf, sendcount, sendtype,
                recvbuf, recvcounts, buffer_sum, displs, recvtype,
                send_true_lb, recv_true_lb, send_size, recv_size,
                comm);
    
    
#if 0

    /*
      if (comm->rank == 0) printf("tree red %d tree bcast %d alltoall %d abino %d arect %d :%d %d\n",
      use_tree_reduce,use_tree_bcast , use_alltoall,use_binom_async,use_rect_async,
      config.recv_contig ,config.send_contig);
      char name[256];
    */
  
  
    if (use_tree_reduce && use_tree_bcast)
      if (config.largecount)
      {
        //strcpy(name, "bcast");
        func = MPIDO_Allgatherv_bcast;
      }
      else
      {
        func = MPIDO_Allgatherv_allreduce;
        //strcpy(name, "allred");
      }
    /* we only can use allreduce, so use it regardless of size */
    else if (use_tree_reduce)
    {
      func = MPIDO_Allgatherv_allreduce;
      //strcpy(name, "allreduce2");

    }
    /* or, we can only use bcast, so use it regardless of size */
    else if (use_tree_bcast && !use_binom_async && !use_rect_async)
    {
      func = MPIDO_Allgatherv_bcast;
      //strcpy(name, "bcast2");
    }
    /* no tree protocols (probably not comm_world) so use alltoall */
    else if (use_alltoall)
    {
      func = MPIDO_Allgatherv_alltoall;
      //strcpy(name, "alltoall");
    }
    else if (use_rect_async)
      if (!config.largecount)
      {
        func = MPIDO_Allgatherv_bcast_rect_async;
        //strcpy(name, "arect");
      }
      else
      {
        func = MPIDO_Allgatherv_bcast;
        //strcpy(name, "bcast3");
      }
    else if (use_binom_async)
      if (!config.largecount)
      {
        func = MPIDO_Allgatherv_bcast_binom_async;
        //strcpy(name, "abinom");
      }
      else
      {
        func = MPIDO_Allgatherv_bcast;
        //strcpy(name, "bcast4");
      }
    else
      return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
                             recvbuf, recvcounts, displs, recvtype,
                             comm);
    /*
      MPI_Barrier(comm->handle);
      int j;
      for (j = 0; j < comm->local_size; j++)
      {
      MPI_Barrier(comm->handle);
      if (j == comm->rank)
      printf("%d using %s %d\n", comm->rank, name, comm->handle);

      }
    */
    rc = (func)(sendbuf, sendcount, sendtype,
                recvbuf, recvcounts, buffer_sum, displs, recvtype,
                send_true_lb, recv_true_lb, send_size, recv_size,
                comm);
#endif 
  }
  else
  {
    STAR_Callsite collective_site;
    void ** tb_ptr = (void **) MPIU_Malloc(sizeof(void *) *
                                           STAR_info.traceback_levels);

    /* set the internal control flow to disable internal star tuning */
    STAR_info.internal_control_flow = 1;
      
    /* get backtrace info for caller to this func, use that as callsite_id */
    backtrace(tb_ptr, STAR_info.traceback_levels);
      
    /* create a signature callsite info for this particular call site */
    collective_site.call_type = ALLGATHERV_CALL;
    collective_site.comm = comm;
    collective_site.bytes = buffer_sum;
    collective_site.op_type_support = DCMF_SUPPORT_NOT_NEEDED;
    collective_site.buff_attributes[0] = config.send_contig;
    collective_site.buff_attributes[1] = config.recv_contig;
    collective_site.buff_attributes[2] = config.recv_continuous;
      
    /* decide buffer alignment */
    collective_site.buff_attributes[3] = 1; /* assume aligned */
    if (((unsigned)sendbuf & 0x0F) || ((unsigned)recvbuf & 0x0F))
      collective_site.buff_attributes[3] = 0; /* set to not aligned */
      
    collective_site.id = (int) tb_ptr[STAR_info.traceback_levels - 1];
      
    rc = STAR_Allgatherv(sendbuf,
                         sendcount,
                         sendtype,
                         recvbuf,
                         recvcounts,
                         buffer_sum,
                         displs,
                         recvtype,
                         send_true_lb,
                         recv_true_lb,
                         send_size,
                         recv_size,
                         &collective_site,
                         STAR_allgatherv_repository,
                         STAR_info.allgatherv_algorithms);
      
    /* unset the internal control flow */
    STAR_info.internal_control_flow = 0;
      
    if (rc == STAR_FAILURE)
      rc = MPIR_Allgatherv(sendbuf, sendcount, sendtype,
                           recvbuf, recvcounts, displs, recvtype,
                           comm);
    MPIU_Free(tb_ptr);
  }  
  return rc;
}

#else /* !USE_CCMI_COLL */

int MPIDO_Allgatherv(void *sendbuf,
                     int sendcount,
                     MPI_Datatype sendtype,
                     void *recvbuf,
                     int *recvcounts,
                     int *displs,
                     MPI_Datatype recvtype,
                     MPID_Comm * comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
