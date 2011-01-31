/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/allgather/mpido_allgather.c
 * \brief ???
 */

#include <mpidimpl.h>

/* ****************************************************************** */
/**
 * \brief Use (tree) MPIDO_Allreduce() to do a fast Allgather operation
 *
 * \note This function requires that:
 *       - The send/recv data types are contiguous
 *       - Tree allreduce is availible (for max performance)
 *       - The datatype parameters needed added to the function signature
 */
/* ****************************************************************** */
int MPIDO_Allgather_allreduce(void *sendbuf,
			      int sendcount,
			      MPI_Datatype sendtype,
			      void *recvbuf,
			      int recvcount,
			      MPI_Datatype recvtype,
			      MPI_Aint send_true_lb,
			      MPI_Aint recv_true_lb,
			      size_t send_size,
			      size_t recv_size,
			      MPID_Comm * comm_ptr)

{
  int rc, rank;
  char *startbuf = NULL;
  char *destbuf = NULL;

  rank = comm_ptr->rank;

  startbuf   = (char *) recvbuf + recv_true_lb;
  destbuf    = startbuf + rank * send_size;

  memset(startbuf, 0, rank * send_size);
  memset(destbuf + send_size, 0, recv_size - (rank + 1) * send_size);

  if (sendbuf != MPI_IN_PLACE)
  {
    char *outputbuf = (char *) sendbuf + send_true_lb;
    memcpy(destbuf, outputbuf, send_size);
  }
  rc = MPIDO_Allreduce(MPI_IN_PLACE,
		       startbuf,
		       recv_size/sizeof(int),
		       MPI_INT,
		       MPI_BOR,
		       comm_ptr);

  return rc;
}


/* ****************************************************************** */
/**
 * \brief Use (tree/rect) MPIDO_Bcast() to do a fast Allgather operation
 *
 * \note This function requires one of these (for max performance):
 *       - Tree broadcast
 */
/* ****************************************************************** */
int MPIDO_Allgather_bcast(void *sendbuf,
			  int sendcount,
			  MPI_Datatype sendtype,
			  void *recvbuf,
			  int recvcount,
			  MPI_Datatype recvtype,
			  MPI_Aint send_true_lb,
			  MPI_Aint recv_true_lb,
			  size_t send_size,
			  size_t recv_size,
			  MPID_Comm * comm_ptr)
{
  int i, np, rc = 0;
  MPI_Aint extent;

  np = comm_ptr ->local_size;
  MPID_Datatype_get_extent_macro(recvtype, extent);

  MPID_Ensure_Aint_fits_in_pointer ((MPI_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				     np * recvcount * extent));
  if (sendbuf != MPI_IN_PLACE)
  {
    void *destbuf = recvbuf + comm_ptr->rank * recvcount * extent;
    MPIR_Localcopy(sendbuf,
                   sendcount,
                   sendtype,
                   destbuf,
                   recvcount,
                   recvtype);
  }

/* this code should either abort on first error or somehow aggregate
 * error codes, esp since it calls internal routines */
  for (i = 0; i < np; i++)
  {
    void *destbuf = recvbuf + i * recvcount * extent;
    rc = MPIDO_Bcast(destbuf,
                     recvcount,
                     recvtype,
                     i,
                     comm_ptr);
  }

  return rc;
}

/* ****************************************************************** */
/**
 * \brief Use (tree/rect) MPIDO_Alltoall() to do a fast Allgather operation
 *
 * \note This function requires that:
 *       - The send/recv data types are contiguous
 *       - DMA alltoallv is availible (for max performance)
 *       - The datatype parameters needed added to the function signature
 */
/* ****************************************************************** */
int MPIDO_Allgather_alltoall(void *sendbuf,
			     int sendcount,
			     MPI_Datatype sendtype,
			     void *recvbuf,
			     int recvcount,
			     MPI_Datatype recvtype,
			     MPI_Aint send_true_lb,
			     MPI_Aint recv_true_lb,
			     size_t send_size,
			     size_t recv_size,
			     MPID_Comm * comm_ptr)
{
  int i, rc;
  void *a2a_sendbuf = NULL;
  char *destbuf=NULL;
  char *startbuf=NULL;

  int a2a_sendcounts[comm_ptr->local_size];
  int a2a_senddispls[comm_ptr->local_size];
  int a2a_recvcounts[comm_ptr->local_size];
  int a2a_recvdispls[comm_ptr->local_size];

  for (i = 0; i < comm_ptr->local_size; ++i)
  {
    a2a_sendcounts[i] = send_size;
    a2a_senddispls[i] = 0;
    a2a_recvcounts[i] = recvcount;
    a2a_recvdispls[i] = recvcount * i;
  }
  if (sendbuf != MPI_IN_PLACE)
  {
    a2a_sendbuf = sendbuf + send_true_lb;
  }
  else
  {
    startbuf = (char *) recvbuf + recv_true_lb;
    destbuf = startbuf + comm_ptr->rank * send_size;
    a2a_sendbuf = destbuf;
    a2a_sendcounts[comm_ptr->rank] = 0;

    a2a_recvcounts[comm_ptr->rank] = 0;
  }

  rc = MPIR_Alltoallv(a2a_sendbuf,
		       a2a_sendcounts,
		       a2a_senddispls,
		       MPI_CHAR,
		       recvbuf,
		       a2a_recvcounts,
		       a2a_recvdispls,
		       recvtype,
		       comm_ptr);

  return rc;
}



static void allred_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *)clientdata;
   (*active)--;
}


int
MPIDO_Allgather(void *sendbuf,
                int sendcount,
                MPI_Datatype sendtype,
                void *recvbuf,
                int recvcount,
                MPI_Datatype recvtype,
                MPID_Comm * comm_ptr)
{
  /* *********************************
   * Check the nature of the buffers
   * *********************************
   */
//  MPIDO_Coll_config config = {1,1,1,1,1,1};
   int config[6], i;
   MPID_Datatype * dt_null = NULL;
   MPI_Aint send_true_lb = 0;
   MPI_Aint recv_true_lb = 0;
   int rc, comm_size = comm_ptr->local_size;
   size_t send_size = 0;
   size_t recv_size = 0;
   volatile unsigned allred_active = 1;
   pami_xfer_t allred;
   for (i=0;i<6;i++) config[i] = 1;

   allred.cb_done = allred_cb_done;
   allred.cookie = (void *)&allred_active;
   /* Pick an algorithm that is guaranteed to work for the pre-allreduce */
   allred.algorithm = comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][0][0]; 
   allred.cmd.xfer_allreduce.sndbuf = (void *)config;
   allred.cmd.xfer_allreduce.stype = PAMI_TYPE_CONTIGUOUS;
   allred.cmd.xfer_allreduce.rcvbuf = (void *)config;
   allred.cmd.xfer_allreduce.rtype = PAMI_TYPE_CONTIGUOUS;
   allred.cmd.xfer_allreduce.stypecount = 6*sizeof(int);
   allred.cmd.xfer_allreduce.rtypecount = 6*sizeof(int);
   allred.cmd.xfer_allreduce.dt = PAMI_SIGNED_INT;
   allred.cmd.xfer_allreduce.op = PAMI_BAND;

  char use_tree_reduce, use_alltoall, use_bcast;
  char *sbuf, *rbuf;

   use_alltoall = comm_ptr->mpid.allgathers[2];
   use_tree_reduce = comm_ptr->mpid.allgathers[0];
   use_bcast = comm_ptr->mpid.allgathers[1];
   if(!( use_alltoall || use_tree_reduce || use_bcast) || 
      comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLGATHER] == 0)
   {
      MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_MPICH");
      return MPIR_Allgather(sendbuf, sendcount, sendtype,
                            recvbuf, recvcount, recvtype,
                            comm_ptr);
   }
  if ((sendcount < 1 && sendbuf != MPI_IN_PLACE) || recvcount < 1)
    return MPI_SUCCESS;

  MPIDI_Datatype_get_info(recvcount,
			  recvtype,
        config[MPID_RECV_CONTIG],
			  recv_size,
			  dt_null,
			  recv_true_lb);
  send_size = recv_size;
  recv_size *= comm_size;
  rbuf = (char *)recvbuf+recv_true_lb;


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

  /* verify everyone's datatype contiguity */
  /* Check buffer alignment now, since we're pre-allreducing anyway */
  config[MPID_ALIGNEDBUFFER] =
            !((long)sendbuf & 0x0F) && !((long)recvbuf & 0x0F);

   /* #warning need to determine best allreduce for short messages */
   if(comm_ptr->mpid.preallreduces[MPID_ALLGATHER_PREALLREDUCE])
  {
      MPIDI_Post_coll_t allred_post;
      allred_post.coll_struct = &allred;
      /* Post the collective call */
      PAMI_Context_post(MPIDI_Context[0], &allred_post.state, 
                        MPIDI_Pami_post_wrapper, (void *)&allred_post);
      MPID_PROGRESS_WAIT_WHILE(allred_active);
  }


  /* Here is the Default code path or if coming from within another coll */
    use_alltoall = comm_ptr->mpid.allgathers[2] &&
         config[MPID_RECV_CONTIG] && config[MPID_SEND_CONTIG];;

    use_tree_reduce = comm_ptr->mpid.allgathers[0] &&
      config[MPID_RECV_CONTIG] && config[MPID_SEND_CONTIG] &&
      config[MPID_RECV_CONTINUOUS] && (recv_size % sizeof(int) == 0);

    use_bcast = comm_ptr->mpid.allgathers[1];


   if(use_tree_reduce)
   {
      MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_ALLREDUCE");
     rc = MPIDO_Allgather_allreduce(sendbuf, sendcount, sendtype,
                               recvbuf, recvcount, recvtype,
                               send_true_lb, recv_true_lb, send_size, recv_size, comm_ptr);
      return rc;
   }
   if(use_alltoall)
   {
      MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_ALLTOALL");
     rc = MPIDO_Allgather_alltoall(sendbuf, sendcount, sendtype,
                               recvbuf, recvcount, recvtype,
                               send_true_lb, recv_true_lb, send_size, recv_size, comm_ptr);
   return rc;
   }
   if(use_bcast)
   {
      MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_BCAST");
     rc = MPIDO_Allgather_bcast(sendbuf, sendcount, sendtype,
                               recvbuf, recvcount, recvtype,
                               send_true_lb, recv_true_lb, send_size, recv_size, comm_ptr);
   return rc;
   }

      MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_MPICH");
      return MPIR_Allgather(sendbuf, sendcount, sendtype,
                            recvbuf, recvcount, recvtype,
                            comm_ptr);
}
