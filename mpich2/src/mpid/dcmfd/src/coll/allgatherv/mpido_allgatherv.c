/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgatherv/mpido_allgatherv.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"


#ifdef USE_CCMI_COLL

#pragma weak PMPIDO_Allgatherv = MPIDO_Allgatherv

static void allred_cb_done(void *clientdata, DCMF_Error_t *err)
{
   volatile unsigned *work_left = (unsigned *)clientdata;
   *work_left = 0;
   MPID_Progress_signal();
}

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
  int config[6];
  double msize;
  
  int i, rc, buffer_sum = 0, np = comm->local_size;
  char use_tree_reduce, use_alltoall, use_rect_async, use_bcast;
  char *sbuf, *rbuf;

  DCMF_CollectiveRequest_t request;
  volatile unsigned allred_active = 1;
  DCMF_Callback_t allred_cb = {allred_cb_done, (void *) &allred_active};

  for(i=0;i<6;i++) config[i] = 1;

  MPIDO_Embedded_Info_Set * comm_prop = &(comm->dcmf.properties);
  MPIDO_Embedded_Info_Set * coll_prop = &MPIDI_CollectiveProtocols.properties;

  unsigned char userenvset = MPIDO_INFO_ISSET(comm_prop,
                                             MPIDO_ALLGATHERV_ENVVAR);

  if (MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_MPICH_ALLGATHERV))
  {
    comm->dcmf.last_algorithm = MPIDO_USE_MPICH_ALLGATHERV;
    return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
			   recvbuf, recvcounts, displs, recvtype,
			   comm);
  }
  MPIDI_Datatype_get_info(1,
			  recvtype,
			  config[MPIDO_RECV_CONTIG],
			  recv_size,
			  dt_null,
			  recv_true_lb);
  
  
  if (sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            config[MPIDO_SEND_CONTIG],
                            send_size,
                            dt_null,
                            send_true_lb);
    MPIDI_VerifyBuffer(sendbuf, sbuf, send_true_lb);
  }
  
  if (displs[0])
    config[MPIDO_RECV_CONTINUOUS] = 0;
  
  for (i = 1; i < np; i++)
  {
    buffer_sum += recvcounts[i - 1];
    if (buffer_sum != displs[i])
    {
      config[MPIDO_RECV_CONTINUOUS] = 0;
      break;
    }
  }
  
  buffer_sum += recvcounts[np - 1];
  
  buffer_sum *= recv_size;
  msize = (double)buffer_sum / (double)np; 
  
  MPIDI_VerifyBuffer(recvbuf, rbuf, (recv_true_lb + buffer_sum));
  
   if (MPIDO_INFO_ISSET(coll_prop, MPIDO_USE_PREALLREDUCE_ALLGATHERV))
   {
      /* Check buffer alignment now, since we're pre-allreducing anyway */
      config[MPIDO_ALIGNEDBUFFER] = 
            !((unsigned)sendbuf & 0x0F) && !((unsigned)recvbuf & 0x0F);
      if(comm->dcmf.short_allred == NULL)
      {
         STAR_info.internal_control_flow = 1;
         MPIDO_Allreduce(MPI_IN_PLACE, &config, 6, MPI_INT, MPI_BAND, comm);
         STAR_info.internal_control_flow = 0;
      }
      else
      {      
         DCMF_Allreduce(comm->dcmf.short_allred,
                     &request,
                     allred_cb,
                     DCMF_MATCH_CONSISTENCY,
                     &(comm->dcmf.geometry),
                     (void *)config,
                     (void *)config,
                     6,
                     DCMF_SIGNED_INT,
                     DCMF_BAND);
         MPID_PROGRESS_WAIT_WHILE(allred_active);
      }
   }

  if (!STAR_info.enabled || STAR_info.internal_control_flow ||
      ((double)buffer_sum / (double)np) < STAR_info.allgather_threshold)
  {
    use_tree_reduce = 
      (MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_TREE_ALLREDUCE) ||
       MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_GLOBAL_TREE_ALLREDUCE)) &&
      MPIDO_INFO_ISSET(comm_prop,
                      MPIDO_USE_ALLREDUCE_ALLGATHERV) &&
      config[MPIDO_RECV_CONTIG] &&
      config[MPIDO_SEND_CONTIG] &&
      config[MPIDO_RECV_CONTINUOUS] &&
      buffer_sum % sizeof(int) == 0;
    
    use_alltoall = MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_TORUS_ALLTOALL) &&
      MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_ALLTOALL_ALLGATHERV) &&
      config[MPIDO_RECV_CONTIG] &&
      config[MPIDO_SEND_CONTIG];

    use_rect_async = MPIDO_INFO_ISSET(comm_prop,
                                     MPIDO_USE_ARECT_BCAST_ALLGATHERV) &&
      MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_ARECT_BCAST) &&
      config[MPIDO_RECV_CONTIG] &&
      config[MPIDO_SEND_CONTIG];
    
    use_bcast = //MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_TREE_BCAST) &&
      MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_BCAST_ALLGATHERV);

    if(userenvset)
    {
      if(use_bcast)
      {
        func = MPIDO_Allgatherv_bcast;
        comm->dcmf.last_algorithm = MPIDO_USE_BCAST_ALLGATHERV;
      }
      if(use_tree_reduce)
      {
        func = MPIDO_Allgatherv_allreduce;
        comm->dcmf.last_algorithm = MPIDO_USE_ALLREDUCE_ALLGATHERV;
      }
      if(use_alltoall)
      {
        func = MPIDO_Allgatherv_alltoall;
        comm->dcmf.last_algorithm = MPIDO_USE_ALLTOALL_ALLGATHERV;
      }
      if(use_rect_async)
      {
        func = MPIDO_Allgatherv_bcast_rect_async;
        comm->dcmf.last_algorithm = MPIDO_USE_ARECT_BCAST_ALLGATHERV;
      }
    }
    else
    {
      if (!MPIDO_INFO_ISSET(comm_prop, MPIDO_IRREG_COMM))
      {
        if (np <= 512)
        {
          if (use_tree_reduce && msize < 128 * np)
          {
            func = MPIDO_Allgatherv_allreduce;
            comm->dcmf.last_algorithm = MPIDO_USE_ALLREDUCE_ALLGATHERV;
          }
          if (!func && use_bcast && msize >= 128 * np)
          {
            func = MPIDO_Allgatherv_bcast;
            comm->dcmf.last_algorithm = MPIDO_USE_BCAST_ALLGATHERV;
          }
          if (!func && use_alltoall &&
              msize > 128 && msize <= 8*np)
          {
            func = MPIDO_Allgatherv_alltoall;
            comm->dcmf.last_algorithm = MPIDO_USE_ALLTOALL_ALLGATHERV;
          }
          if (!func && use_rect_async && msize > 8*np)
          {
            func = MPIDO_Allgatherv_bcast_rect_async;
            comm->dcmf.last_algorithm = MPIDO_USE_ARECT_BCAST_ALLGATHERV;
          }
        }
        else
        {
          if (use_tree_reduce && msize < 512)
          {
            func = MPIDO_Allgatherv_allreduce;
            comm->dcmf.last_algorithm = MPIDO_USE_ALLREDUCE_ALLGATHERV;
          }
          if (!func && use_alltoall &&
              msize > 128 * (512.0 / (float) np) &&
              msize <= 128)
          {
            func = MPIDO_Allgatherv_alltoall;
            comm->dcmf.last_algorithm = MPIDO_USE_ALLTOALL_ALLGATHERV;
          }
          if (!func && use_rect_async &&
              msize >= 512 && msize <= 65536)
          {
            func = MPIDO_Allgatherv_bcast_rect_async;
            comm->dcmf.last_algorithm = MPIDO_USE_ARECT_BCAST_ALLGATHERV;
          }
          if (!func && use_bcast && msize > 65536)
          {
            func = MPIDO_Allgatherv_bcast;
            comm->dcmf.last_algorithm = MPIDO_USE_BCAST_ALLGATHERV;
          }
        }
      }
      else
      {
        if (msize >= 64 && use_alltoall)
        {
          func = MPIDO_Allgatherv_alltoall;
          comm->dcmf.last_algorithm = MPIDO_USE_ALLTOALL_ALLGATHERV;
        }
      }
    }

    if(!func)
    {
      comm->dcmf.last_algorithm = MPIDO_USE_MPICH_ALLGATHERV;
      return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
                             recvbuf, recvcounts, displs, recvtype,
                             comm);
    }
    
    /* If we're using allreduce, we might have verified the buffer
       alignment. If so, tell allreduce that it's "safe".
    */
    int is_set = 0;
    if((func == MPIDO_Allgatherv_allreduce) && config[MPIDO_ALIGNEDBUFFER])
    {
      is_set = MPIDO_INFO_ISSET(comm_prop, MPIDO_USE_PREALLREDUCE_ALLREDUCE);
      MPIDO_INFO_UNSET(comm_prop, MPIDO_USE_PREALLREDUCE_ALLREDUCE);
    }
    rc = (func)(sendbuf, sendcount, sendtype,
                recvbuf, recvcounts, buffer_sum, displs, recvtype,
                send_true_lb, recv_true_lb, send_size, recv_size,
                comm);
    /* Reset the "safe" property */
    if(is_set)
    {
      MPIDO_INFO_SET(comm_prop, MPIDO_USE_PREALLREDUCE_ALLREDUCE);
    }
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
    collective_site.op_type_support = MPIDO_SUPPORT_NOT_NEEDED;
    collective_site.buff_attributes[0] = config[MPIDO_SEND_CONTIG];
    collective_site.buff_attributes[1] = config[MPIDO_RECV_CONTIG];
    collective_site.buff_attributes[2] = config[MPIDO_RECV_CONTINUOUS];
      
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
