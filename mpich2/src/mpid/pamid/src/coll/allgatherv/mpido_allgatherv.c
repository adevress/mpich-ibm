/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgatherv/mpido_allgatherv.c
 * \brief ???
 */

#include "mpidimpl.h"

/* ****************************************************************** */
/**
 * \brief Use (tree) MPIDO_Allreduce() to do a fast Allgatherv operation
 *
 * \note This function requires that:
 *       - The send/recv data types are contiguous
 *       - The recv buffer is continuous
 *       - Tree allreduce is availible (for max performance)
 */
/* ****************************************************************** */
int MPIDO_Allgatherv_allreduce(void *sendbuf,
			       int sendcount,
			       MPI_Datatype sendtype,
			       void *recvbuf,
			       int *recvcounts,
			       int buffer_sum, 
			       int *displs,
			       MPI_Datatype recvtype,
			       MPI_Aint send_true_lb,
			       MPI_Aint recv_true_lb,
			       size_t send_size,
			       size_t recv_size,
			       MPID_Comm * comm_ptr)
{
  int start, rc;
  int length;
  char *startbuf = NULL;
  char *destbuf = NULL;
  
  startbuf = (char *) recvbuf + recv_true_lb;
  destbuf = startbuf + displs[comm_ptr->rank] * recv_size;
  
  start = 0;
  length = displs[comm_ptr->rank] * recv_size;
  memset(startbuf + start, 0, length);
  
  start  = (displs[comm_ptr->rank] + 
	    recvcounts[comm_ptr->rank]) * recv_size;
  length = buffer_sum - (displs[comm_ptr->rank] + 
			 recvcounts[comm_ptr->rank]) * recv_size;
  memset(startbuf + start, 0, length);
  
  if (sendbuf != MPI_IN_PLACE)
  {
    char *outputbuf = (char *) sendbuf + send_true_lb;
    memcpy(destbuf, outputbuf, send_size);
  }
  
  //if (0==comm_ptr->rank) puts("allreduce allgatherv");

  rc = MPIDO_Allreduce(MPI_IN_PLACE,
		       startbuf,
		       buffer_sum/sizeof(int),
		       MPI_INT,
		       MPI_BOR,
		       comm_ptr);
  
  return rc;
}

/* ****************************************************************** */
/**
 * \brief Use (tree/rect) MPIDO_Bcast() to do a fast Allgatherv operation
 *
 * \note This function requires one of these (for max performance):
 *       - Tree broadcast
 *       - Rect broadcast
 *       ? Binomial broadcast
 */
/* ****************************************************************** */
int MPIDO_Allgatherv_bcast(void *sendbuf,
			   int sendcount,
			   MPI_Datatype sendtype,
			   void *recvbuf,
			   int *recvcounts,
			   int buffer_sum, 
			   int *displs,
			   MPI_Datatype recvtype,
			   MPI_Aint send_true_lb,
			   MPI_Aint recv_true_lb,
			   size_t send_size,
			   size_t recv_size,
			   MPID_Comm * comm_ptr)
{
  int i, rc=MPI_ERR_INTERN;
  MPI_Aint extent;
  MPID_Datatype_get_extent_macro(recvtype, extent);

  if (sendbuf != MPI_IN_PLACE)
  {
    void *destbuffer = recvbuf + displs[comm_ptr->rank] * extent;
    MPIR_Localcopy(sendbuf,
                   sendcount,
                   sendtype,
                   destbuffer,
                   recvcounts[comm_ptr->rank],
                   recvtype);
  }
  
  for (i = 0; i < comm_ptr->local_size; i++)
  {
    void *destbuffer = recvbuf + displs[i] * extent;
    rc = MPIDO_Bcast(destbuffer,
                     recvcounts[i],
                     recvtype,
                     i,
                     comm_ptr);
  }
  //if (0==comm_ptr->rank) puts("bcast allgatherv");

  return rc;
}

/* ****************************************************************** */
/**
 * \brief Use (tree/rect) MPIDO_Alltoall() to do a fast Allgatherv operation
 *
 * \note This function requires that:
 *       - The send/recv data types are contiguous
 *       - DMA alltoallv is availible (for max performance)
 */
/* ****************************************************************** */

int MPIDO_Allgatherv_alltoall(void *sendbuf,
			      int sendcount,
			      MPI_Datatype sendtype,
			      void *recvbuf,
			      int *recvcounts,
			      int buffer_sum, 
			      int *displs,
			      MPI_Datatype recvtype,
			      MPI_Aint send_true_lb,
			      MPI_Aint recv_true_lb,
			      size_t send_size,
			      size_t recv_size,
			      MPID_Comm * comm_ptr)
{
  size_t total_send_size;
  char *startbuf;
  char *destbuf;
  int i, rc;
  int my_recvcounts = -1;
  void *a2a_sendbuf = NULL;
  int a2a_sendcounts[comm_ptr->local_size];
  int a2a_senddispls[comm_ptr->local_size];
  
  total_send_size = recvcounts[comm_ptr->rank] * recv_size;
  for (i = 0; i < comm_ptr->local_size; ++i)
  {
    a2a_sendcounts[i] = total_send_size;
    a2a_senddispls[i] = 0;
  }
  if (sendbuf != MPI_IN_PLACE)
  {
    a2a_sendbuf = sendbuf + send_true_lb;
  }
  else
  {
    startbuf = (char *) recvbuf + recv_true_lb;
    destbuf = startbuf + displs[comm_ptr->rank] * recv_size;
    a2a_sendbuf = destbuf;
    a2a_sendcounts[comm_ptr->rank] = 0;
    my_recvcounts = recvcounts[comm_ptr->rank];
    recvcounts[comm_ptr->rank] = 0;
  }
  
  //if (0==comm_ptr->rank) puts("all2all allgatherv");
  rc = MPIR_Alltoallv(a2a_sendbuf,
		       a2a_sendcounts,
		       a2a_senddispls,
		       MPI_CHAR,
		       recvbuf,
		       recvcounts,
		       displs,
		       recvtype,
		       comm_ptr);
  if (sendbuf == MPI_IN_PLACE)
    recvcounts[comm_ptr->rank] = my_recvcounts;

  return rc;
}

static void allred_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *)clientdata;
   (*active)--;
}

int
MPIDO_Allgatherv(void *sendbuf,
		 int sendcount,
		 MPI_Datatype sendtype,
		 void *recvbuf,
		 int *recvcounts,
		 int *displs,
		 MPI_Datatype recvtype,
		 MPID_Comm * comm_ptr)
{
  /* function pointer to be used to point to approperiate algorithm */

  /* Check the nature of the buffers */
  MPID_Datatype *dt_null = NULL;
  MPI_Aint send_true_lb  = 0;
  MPI_Aint recv_true_lb  = 0;
  size_t   send_size     = 0;
  size_t   recv_size     = 0;
  int config[6];
  double msize;
  
  int i, rc, buffer_sum = 0, np = comm_ptr->local_size;
  char use_tree_reduce, use_alltoall, use_bcast;
  char *sbuf, *rbuf;

  pami_xfer_t allred;
  volatile unsigned allred_active = 1;

  for(i=0;i<6;i++) config[i] = 1;

  allred.cb_done = allred_cb_done;
  allred.cookie = (void *)&allred_active;
  allred.algorithm = comm_ptr->mpid.allreduces[0];
  allred.cmd.xfer_allreduce.sndbuf = (void *)config;
  allred.cmd.xfer_allreduce.stype = PAMI_BYTE;
  allred.cmd.xfer_allreduce.rcvbuf = (void *)config;
  allred.cmd.xfer_allreduce.rtype = PAMI_BYTE;
  allred.cmd.xfer_allreduce.stypecount = 6*sizeof(int);
  allred.cmd.xfer_allreduce.rtypecount = 6*sizeof(int);
  allred.cmd.xfer_allreduce.dt = PAMI_SIGNED_INT;
  allred.cmd.xfer_allreduce.op = PAMI_BAND;

  use_alltoall = comm_ptr->mpid.allgathervs[0];
  use_tree_reduce = comm_ptr->mpid.allgathervs[1];
  use_bcast = comm_ptr->mpid.allgathervs[2];
  if(!(use_alltoall || use_tree_reduce || use_bcast))
  {
   MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHERV_MPICH");
   return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
			   recvbuf, recvcounts, displs, recvtype,
			   comm_ptr);
  }

  MPIDI_Datatype_get_info(1,
			  recvtype,
			  config[MPID_RECV_CONTIG],
			  recv_size,
			  dt_null,
			  recv_true_lb);
  
  
  if (sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            config[MPID_SEND_CONTIG],
                            send_size,
                            dt_null,
                            send_true_lb);
    sbuf = (char *)sendbuf+send_true_lb;
  }
  
  if (displs[0])
    config[MPID_RECV_CONTINUOUS] = 0;
  
  for (i = 1; i < np; i++)
  {
    buffer_sum += recvcounts[i - 1];
    if (buffer_sum != displs[i])
    {
      config[MPID_RECV_CONTINUOUS] = 0;
      break;
    }
  }
  
  buffer_sum += recvcounts[np - 1];
  
  buffer_sum *= recv_size;
  msize = (double)buffer_sum / (double)np; 
  
   rbuf = (char *)recvbuf+recv_true_lb;
  
   /* disable with "safe allgatherv" env var */
      {      
         rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&allred);
         MPID_PROGRESS_WAIT_WHILE(allred_active);
      }

   use_tree_reduce = comm_ptr->mpid.allgathervs[1] &&
      config[MPID_RECV_CONTIG] && config[MPID_SEND_CONTIG] &&
      config[MPID_RECV_CONTINUOUS] && buffer_sum % sizeof(int) == 0;

   use_alltoall = comm_ptr->mpid.allgathervs[0] && 
      config[MPID_RECV_CONTIG] && config[MPID_SEND_CONTIG];

   use_bcast = comm_ptr->mpid.allgathervs[2];

   if(use_tree_reduce)
   {
     rc = MPIDO_Allgatherv_allreduce(sendbuf, sendcount, sendtype,
             recvbuf, recvcounts, buffer_sum, displs, recvtype,
             send_true_lb, recv_true_lb, send_size, recv_size,
             comm_ptr);
     MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHERV_TREE_REDUCE");
     return rc;
   }
   if(use_bcast)
   {
     rc = MPIDO_Allgatherv_bcast(sendbuf, sendcount, sendtype,
             recvbuf, recvcounts, buffer_sum, displs, recvtype,
             send_true_lb, recv_true_lb, send_size, recv_size,
             comm_ptr);
     MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHERV_BCAST");
     return rc;
   }
   if(use_alltoall)
   {
     rc = MPIDO_Allgatherv_alltoall(sendbuf, sendcount, sendtype,
             recvbuf, recvcounts, buffer_sum, displs, recvtype,
             send_true_lb, recv_true_lb, send_size, recv_size,
             comm_ptr);
     MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHERV_ALLTOALL");
     return rc;
   }
      
   MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHERV_MPICH");
   return MPIR_Allgatherv(sendbuf, sendcount, sendtype,
                          recvbuf, recvcounts, displs, recvtype,
                          comm_ptr);
}
 
