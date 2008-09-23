/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgather/mpido_allgather.c
 * \brief ???
 */

#include "mpidi_star.h"
#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"


#ifdef USE_CCMI_COLL

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
  
  allgather_fptr func = NULL;
  DCMF_Embedded_Info_Set * coll_prop = &MPIDI_CollectiveProtocols.properties;
  DCMF_Embedded_Info_Set * comm_prop = &(comm->dcmf.properties);
  MPIDO_Coll_config config = {1,1,1,1,1};
  MPID_Datatype * dt_null = NULL;
  MPI_Aint send_true_lb = 0;
  MPI_Aint recv_true_lb = 0;
  size_t send_size = 0;
  size_t recv_size = 0;

  unsigned char userenvset = DCMF_INFO_ISSET(comm_prop,
                                             DCMF_ALLGATHER_ENVVAR);

   char use_tree_reduce, use_alltoall, use_rect_async, use_tree_bcast;


  
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
				   + recv_true_lb + comm->local_size *
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
  {
    STAR_info.internal_control_flow = 1;
    MPIDO_Allreduce(MPI_IN_PLACE, &config, 5, MPI_INT, MPI_BAND, comm);
    STAR_info.internal_control_flow = 0;
  }

  
  /* Here is the Default code path or if coming from within another coll */
   if (!STAR_info.enabled || STAR_info.internal_control_flow) 
   {
      use_alltoall = 
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_TORUS_ALLTOALL) &&
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_ALLTOALL_ALLGATHER) &&
                  config.recv_contig && config.send_contig;
      /* The tree doesn't support reduce of chars for the operation we need,
       * so we change to ints. Therefore the size needs to be a multiple of
       * sizeof(int) */
      use_tree_bcast = DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_BCAST) &&
                     DCMF_INFO_ISSET(comm_prop, DCMF_USE_BCAST_ALLGATHER); 

      use_tree_reduce = 
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_TREE_ALLREDUCE) &&
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_ALLREDUCE_ALLGATHER) &&
                  config.recv_contig && config.send_contig && 
                  config.recv_continuous && (recv_size % sizeof(int) == 0);
      use_rect_async = 
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_ARECT_BCAST_ALLGATHER) &&
                  DCMF_INFO_ISSET(comm_prop, DCMF_USE_ARECT_BCAST) &&
                  config.recv_contig && config.send_contig;

      if(userenvset)
      {
         if(use_tree_reduce)
            func = MPIDO_Allgather_allreduce;
         if(use_alltoall)
            func = MPIDO_Allgather_alltoall;
         if(use_rect_async)
            func = MPIDO_Allgather_bcast_rect_async;
         if(use_tree_bcast)
            func = MPIDO_Allgather_bcast;
      }
      else
      {
         /* Tree bcast is faster for large messages */
         if(use_tree_reduce && use_tree_bcast && sendcount > 131072)
            func = MPIDO_Allgather_bcast;
         if(!func && use_tree_reduce && use_tree_bcast)
            func = MPIDO_Allgather_allreduce;
         if(!func && use_tree_reduce)
            func = MPIDO_Allgather_allreduce;
         if(!func && use_tree_bcast)
            func = MPIDO_Allgather_bcast;
         /* No tree, so need to use torus protocols. alltoall is good for
          * medium sized messages. MPICH is good for small messages. Async
          * bcast is best for larger messages */
         if(!func && use_alltoall && (sendcount > 128 && sendcount <= 8192))
            func = MPIDO_Allgather_alltoall;
         if(!func && use_rect_async && (sendcount > 8192))
            func = MPIDO_Allgather_bcast_rect_async;
      }
      #if 0
         if(sendcount <= 512 )
         {
            if(use_tree_reduce)
               func = MPIDO_Allgather_allreduce;
            if(!func && use_alltoall)
               func = MPIDO_Allgather_alltoall;
         }
         /* tree reduce smokes async bcast */
         else if(sendcount <= 1024)
         {
            if(use_tree_reduce)
               func = MPIDO_Allgather_allreduce;
            if(!func && use_rect_async)
               func = MPIDO_Allgather_bcast_rect_async;
            if(!func && use_alltoall)
               func = MPIDO_Allgather_alltoall;
         }
      }
      #endif 
         
      if (!func)
         return MPIR_Allgather(sendbuf, sendcount, sendtype,
                               recvbuf, recvcount, recvtype,
                               comm);

      rc = (func)(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                send_true_lb, recv_true_lb, send_size, recv_size, comm);
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
    collective_site.call_type = ALLGATHER_CALL;
    collective_site.comm = comm;
    collective_site.bytes = recv_size;
    collective_site.op_type_support = DCMF_SUPPORT_NOT_NEEDED;
    collective_site.buff_attributes[0] = config.send_contig;
    collective_site.buff_attributes[1] = config.recv_contig;
    collective_site.buff_attributes[2] = config.recv_continuous;
    
    /* decide buffer alignment */
    collective_site.buff_attributes[3] = 1; /* assume aligned */
    if (((unsigned)sendbuf & 0x0F) || ((unsigned)recvbuf & 0x0F))
      collective_site.buff_attributes[3] = 0; /* set to not aligned */
    
    collective_site.id = (int) tb_ptr[STAR_info.traceback_levels - 1];
    
    rc = STAR_Allgather(sendbuf,
                        sendcount,
                        sendtype,
                        recvbuf,
                        recvcount,
                        recvtype,
                        send_true_lb,
                        recv_true_lb,
                        send_size,
                        recv_size,
                        &collective_site,
                        STAR_allgather_repository,
                        STAR_info.allgather_algorithms);
      
    /* unset the internal control flow */
    STAR_info.internal_control_flow = 0;
      
    if (rc == STAR_FAILURE)
      rc = MPIR_Allgather(sendbuf, sendcount, sendtype,
                          recvbuf, recvcount, recvtype,
                          comm);
    MPIU_Free(tb_ptr);
  }
  
  return rc;
}

#else /* !USE_CCMI_COLL */

int MPIDO_Allgather(void *sendbuf,
                    int sendcount,
                    MPI_Datatype sendtype,
                    void *recvbuf,
                    int recvcount,
                    MPI_Datatype recvtype,
                    MPID_Comm * comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
