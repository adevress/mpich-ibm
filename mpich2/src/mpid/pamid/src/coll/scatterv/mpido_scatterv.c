/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/scatterv/mpido_scatterv.c
 * \brief ???
 */

#include <mpidimpl.h>


/* basically, everyone receives recvcount via bcast */
/* requires a contiguous/continous buffer on root though */
int MPIDO_Scatterv_bcast(void *sendbuf,
                         int *sendcounts,
                         int *displs,
                         MPI_Datatype sendtype,
                         void *recvbuf,
                         int recvcount,
                         MPI_Datatype recvtype,
                         int root,
                         MPID_Comm *comm_ptr)
{
  int rank = comm_ptr->rank;
  int np = comm_ptr->local_size;
  char *tempbuf;
  int i, sum = 0, dtsize, rc=0, contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_lb;

  for (i = 0; i < np; i++)
    if (sendcounts > 0)
      sum += sendcounts[i];

  MPIDI_Datatype_get_info(1,
			  recvtype,
			  contig,
			  dtsize,
			  dt_ptr,
			  dt_lb);

  if (rank != root)
  {
    tempbuf = MPIU_Malloc(dtsize * sum);
    if (!tempbuf)
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  __FUNCTION__,
                                  __LINE__,
                                  MPI_ERR_OTHER,
                                  "**nomem", 0);
  }
  else
    tempbuf = sendbuf;

  rc = MPIDO_Bcast(tempbuf, sum, sendtype, root, comm_ptr);

  if(rank == root && recvbuf == MPI_IN_PLACE)
    return rc;

  memcpy(recvbuf, tempbuf + displs[rank], sendcounts[rank] * dtsize);

  if (rank != root)
    MPIU_Free(tempbuf);

  return rc;
}

/* this guy requires quite a few buffers. maybe
 * we should somehow "steal" the comm_ptr alltoall ones? */
int MPIDO_Scatterv_alltoallv(void * sendbuf,
                             int * sendcounts,
                             int * displs,
                             MPI_Datatype sendtype,
                             void * recvbuf,
                             int recvcount,
                             MPI_Datatype recvtype,
                             int root,
                             MPID_Comm * comm_ptr)
{
  int rank = comm_ptr->rank;
  int size = comm_ptr->local_size;

  int *sdispls, *scounts;
  int *rdispls, *rcounts;
  char *sbuf, *rbuf;
  int contig, rbytes, sbytes;
  int rc;

  MPID_Datatype *dt_ptr;
  MPI_Aint dt_lb=0;

  MPIDI_Datatype_get_info(recvcount,
                          recvtype,
                          contig,
                          rbytes,
                          dt_ptr,
                          dt_lb);

  if(rank == root)
    MPIDI_Datatype_get_info(1, sendtype, contig, sbytes, dt_ptr, dt_lb);

  rbuf = MPIU_Malloc(size * rbytes * sizeof(char));
  if(!rbuf)
  {
    return MPIR_Err_create_code(MPI_SUCCESS,
                                MPIR_ERR_RECOVERABLE,
                                __FUNCTION__,
                                __LINE__,
                                MPI_ERR_OTHER,
                                "**nomem", 0);
  }
  //   memset(rbuf, 0, rbytes * size * sizeof(char));

  if(rank == root)
  {
    sdispls = displs;
    scounts = sendcounts;
    sbuf = sendbuf;
  }
  else
  {
    sdispls = MPIU_Malloc(size * sizeof(int));
    scounts = MPIU_Malloc(size * sizeof(int));
    sbuf = MPIU_Malloc(rbytes * sizeof(char));
    if(!sdispls || !scounts || !sbuf)
    {
      if(sdispls)
        MPIU_Free(sdispls);
      if(scounts)
        MPIU_Free(scounts);
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  __FUNCTION__,
                                  __LINE__,
                                  MPI_ERR_OTHER,
                                  "**nomem", 0);
    }
    memset(sdispls, 0, size*sizeof(int));
    memset(scounts, 0, size*sizeof(int));
    //      memset(sbuf, 0, rbytes * sizeof(char));
  }

  rdispls = MPIU_Malloc(size * sizeof(int));
  rcounts = MPIU_Malloc(size * sizeof(int));
  if(!rdispls || !rcounts)
  {
    if(rdispls)
      MPIU_Free(rdispls);
    return MPIR_Err_create_code(MPI_SUCCESS,
                                MPIR_ERR_RECOVERABLE,
                                __FUNCTION__,
                                __LINE__,
                                MPI_ERR_OTHER,
                                "**nomem", 0);
  }

  memset(rdispls, 0, size*sizeof(unsigned));
  memset(rcounts, 0, size*sizeof(unsigned));

  rcounts[root] = rbytes;

  rc = MPIR_Alltoallv(sbuf,
                  scounts,
                  sdispls,
                  sendtype,
                  rbuf,
                  rcounts,
                  rdispls,
                  MPI_CHAR,
                  comm_ptr);

  if(rank == root && recvbuf == MPI_IN_PLACE)
  {
    MPIU_Free(rbuf);
    MPIU_Free(rdispls);
    MPIU_Free(rcounts);
    return rc;
  }
  else
  {
    //      memcpy(recvbuf, rbuf+(root*rbytes), rbytes);
    memcpy(recvbuf, rbuf, rbytes);
    MPIU_Free(rbuf);
    MPIU_Free(rdispls);
    MPIU_Free(rcounts);
    if(rank != root)
    {
      MPIU_Free(sbuf);
      MPIU_Free(sdispls);
      MPIU_Free(scounts);
    }
  }

  return rc;
}

static void allred_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   (*active)--;
}

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
  int rank = comm_ptr->rank, size = comm_ptr->local_size;
  int i, nbytes, sum=0, contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint true_lb=0;
  volatile unsigned allred_active = 1;
  pami_xfer_t allred;
  int optscatterv[3];

  allred.cb_done = allred_cb_done;
  allred.cookie = (void *)&allred_active;
  allred.algorithm = comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][0][0];
  allred.cmd.xfer_allreduce.sndbuf = (void *)optscatterv;
  allred.cmd.xfer_allreduce.stype = PAMI_BYTE;
  allred.cmd.xfer_allreduce.rcvbuf = (void *)optscatterv;
  allred.cmd.xfer_allreduce.rtype = PAMI_BYTE;
  allred.cmd.xfer_allreduce.stypecount = 3 * sizeof(int);
  allred.cmd.xfer_allreduce.rtypecount = 3 * sizeof(int);
  allred.cmd.xfer_allreduce.dt = PAMI_SIGNED_INT;
  allred.cmd.xfer_allreduce.op = PAMI_BAND;


   if(!(comm_ptr->mpid.scattervs[0] || comm_ptr->mpid.scattervs[1]) ||
      comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCATTERV] == 0)
  {
   MPIDI_Update_last_algorithm(comm_ptr, "SCATTERV_MPICH");
    return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
                         recvbuf, recvcount, recvtype,
                         root, comm_ptr);
  }


  /* we can't call scatterv-via-bcast unless we know all nodes have
   * valid sendcount arrays. so the user must explicitly ask for it.
   */

   /* optscatterv[0] == optscatterv bcast?
    * optscatterv[1] == optscatterv alltoall?
    *  (having both allows cutoff agreement)
    * optscatterv[2] == sum of sendcounts
    */

   optscatterv[0] = !comm_ptr->mpid.scattervs[0];
   optscatterv[1] = !comm_ptr->mpid.scattervs[1];
   optscatterv[2] = 1;

   if(rank == root)
   {
      if(!optscatterv[1])
      {
         if(sendcounts)
         {
            for(i=0;i<size;i++)
               sum+=sendcounts[i];
         }
         optscatterv[2] = sum;
      }

      MPIDI_Datatype_get_info(1,
                            sendtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);
      if(recvtype == MPI_DATATYPE_NULL || recvcount <= 0 || !contig)
      {
         optscatterv[0] = 1;
         optscatterv[1] = 1;
      }
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
      {
         optscatterv[0] = 1;
         optscatterv[1] = 1;
      }
   }

  /* Make sure parameters are the same on all the nodes */
  /* specifically, noncontig on the receive */
  /* set the internal control flow to disable internal star tuning */
  {
      PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&allred);
      MPID_PROGRESS_WAIT_WHILE(allred_active);
   }
  /* reset flag */

   if(!optscatterv[0] || (!optscatterv[1]))
   {
      char *newrecvbuf = recvbuf;
      char *newsendbuf = sendbuf;
      if(rank == root)
      {
         newsendbuf = sendbuf + true_lb;
      }
      else
      {
         newrecvbuf = recvbuf + true_lb;
      }
      if(!optscatterv[0])
      {
         MPIDI_Update_last_algorithm(comm_ptr, "SCATTERV_OPT_ALLTOALL");
        return MPIDO_Scatterv_alltoallv(newsendbuf,
                                        sendcounts,
                                        displs,
                                        sendtype,
                                        newrecvbuf,
                                        recvcount,
                                        recvtype,
                                        root,
                                        comm_ptr);

      }
      else
      {
         MPIDI_Update_last_algorithm(comm_ptr, "SCATTERV_OPT_BCAST");
        return MPIDO_Scatterv_bcast(newsendbuf,
                                    sendcounts,
                                    displs,
                                    sendtype,
                                    newrecvbuf,
                                    recvcount,
                                    recvtype,
                                    root,
                                    comm_ptr);
      }
   } /* nothing valid to try, go to mpich */
   else
   {
      MPIDI_Update_last_algorithm(comm_ptr, "SCATTERV_MPICH");
      return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
                           recvbuf, recvcount, recvtype,
                           root, comm_ptr);
   }
}
