/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/allgather/mpido_allgather.c
 * \brief ???
 */

/* #define TRACE_ON */
#include <mpidimpl.h>


static void allgather_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
  int *active = (int *)clientdata;
  (*active)--;
}


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

int MPIDO_Allgather_allreduce(const void *sendbuf,
                              int sendcount,
                              MPI_Datatype sendtype,
                              void *recvbuf,
                              int recvcount,
                              MPI_Datatype recvtype,
                              MPI_Aint send_true_lb,
                              MPI_Aint recv_true_lb,
                              size_t send_size, /* bytes */
                              size_t recv_size, /* bytes */
                              MPID_Comm * comm_ptr,
                              int *mpierrno)
{
  int rc, i;
  char *startbuf = NULL;
  char *destbuf = NULL;
  const int rank = comm_ptr->rank;

  startbuf   = (char *) recvbuf + recv_true_lb;
  destbuf    = startbuf + rank * send_size;

  if(sendbuf != MPI_IN_PLACE)
  {
    char *outputbuf = (char *) sendbuf + send_true_lb;
    memcpy(destbuf, outputbuf, send_size);
  }

  /* This next bit is dangerous -- what if only one rank does this? */
  /*Do a convert to dbl and then do the allreduce*/
  if( recv_size <= MPIDI_Process.optimized.max_alloc &&
      (send_size & 0x3)==0 && 
      (sendtype != MPI_DOUBLE || recvtype != MPI_DOUBLE))
  {
    double *tmprbuf = (double *)MPIU_Malloc(recv_size*2);
    /* This next bit is dangerous -- what if only one rank does this? another allreduce?! */
    if(tmprbuf == NULL)
      goto direct_algo; /*skip int to fp conversion and go to direct
        algo*/

    double *tmpsbuf = tmprbuf + (rank*send_size)/sizeof(int);
    int *sibuf = (int *) destbuf;

    memset(tmprbuf, 0, rank*send_size*2);
    memset(tmpsbuf + send_size/sizeof(int), 0, 
           (recv_size - (rank + 1)*send_size)*2);

    for(i = 0; i < (send_size/sizeof(int)); ++i)
      tmpsbuf[i] = (double)sibuf[i];
    /* Switch to comm->coll_fns->fn() */
    rc = MPIDO_Allreduce(MPI_IN_PLACE,
                         tmprbuf,
                         recv_size/sizeof(int),
                         MPI_DOUBLE,
                         MPI_SUM,
                         comm_ptr,
                         mpierrno);

    sibuf = (int *) startbuf;
    for(i = 0; i < (rank*send_size/sizeof(int)); ++i)
      sibuf[i] = (int)tmprbuf[i];

    for(i = (rank+1)*send_size/sizeof(int); i < recv_size/sizeof(int); ++i)
      sibuf[i] = (int)tmprbuf[i];

    MPIU_Free(tmprbuf);
    return rc;
  }

  direct_algo:

  memset(startbuf, 0, rank * send_size);
  memset(destbuf + send_size, 0, recv_size - (rank + 1) * send_size);

  if(sendtype == MPI_DOUBLE && recvtype == MPI_DOUBLE)
    /* Switch to comm->coll_fns->fn() */
    rc = MPIDO_Allreduce(MPI_IN_PLACE,
                         startbuf,
                         recv_size/sizeof(double),
                         MPI_DOUBLE,
                         MPI_SUM,
                         comm_ptr,
                         mpierrno);
  else
    /* Switch to comm->coll_fns->fn() */
    rc = MPIDO_Allreduce(MPI_IN_PLACE,
                         startbuf,
                         recv_size/sizeof(int),
                         MPI_UNSIGNED,
                         MPI_BOR,
                         comm_ptr,
                         mpierrno);

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
int MPIDO_Allgather_bcast(const void *sendbuf,
                          int sendcount,
                          MPI_Datatype sendtype,
                          void *recvbuf,
                          int recvcount,  
                          MPI_Datatype recvtype,
                          MPI_Aint send_true_lb,
                          MPI_Aint recv_true_lb,
                          size_t send_size,
                          size_t recv_size,
                          MPID_Comm * comm_ptr,
                          int *mpierrno)
{
  int i, np, rc = 0;
  MPI_Aint extent;
  const int rank = comm_ptr->rank;

  np = comm_ptr ->local_size;
  MPID_Datatype_get_extent_macro(recvtype, extent);

  MPID_Ensure_Aint_fits_in_pointer ((MPI_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
                                     np * recvcount * extent));
  if(sendbuf != MPI_IN_PLACE)
  {
    void *destbuf = recvbuf + rank * recvcount * extent;
    MPIR_Localcopy(sendbuf,
                   sendcount,
                   sendtype,
                   destbuf,
                   recvcount,
                   recvtype);
  }

/* this code should either abort on first error or somehow aggregate
 * error codes, esp since it calls internal routines */
  for(i = 0; i < np; i++)
  {
    void *destbuf = recvbuf + i * recvcount * extent;
    /* Switch to comm->coll_fns->fn() */
    rc = MPIDO_Bcast(destbuf,
                     recvcount,
                     recvtype,
                     i,
                     comm_ptr,
                     mpierrno);
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
int MPIDO_Allgather_alltoall(const void *sendbuf,
                             int sendcount,
                             MPI_Datatype sendtype,
                             void *recvbuf,
                             int recvcount,
                             MPI_Datatype recvtype,
                             MPI_Aint send_true_lb,
                             MPI_Aint recv_true_lb,
                             size_t send_size,
                             size_t recv_size,
                             MPID_Comm * comm_ptr,
                             int *mpierrno)
{
  int i, rc;
  void *a2a_sendbuf = NULL;
  char *destbuf=NULL;
  char *startbuf=NULL;
  const int size = comm_ptr->local_size;
  const int rank = comm_ptr->rank;

  int a2a_sendcounts[size];
  int a2a_senddispls[size];
  int a2a_recvcounts[size];
  int a2a_recvdispls[size];

  for(i = 0; i < size; ++i)
  {
    a2a_sendcounts[i] = send_size;
    a2a_senddispls[i] = 0;
    a2a_recvcounts[i] = recvcount;
    a2a_recvdispls[i] = recvcount * i;
  }
  if(sendbuf != MPI_IN_PLACE)
  {
    a2a_sendbuf = (char *)sendbuf + send_true_lb;
  }
  else
  {
    startbuf = (char *) recvbuf + recv_true_lb;
    destbuf = startbuf + rank * send_size;
    a2a_sendbuf = destbuf;
    a2a_sendcounts[rank] = 0;

    a2a_recvcounts[rank] = 0;
  }


  /* Switch to comm->coll_fns->fn() */
  rc = MPIDO_Alltoallv((const void *)a2a_sendbuf,
                       a2a_sendcounts,
                       a2a_senddispls,
                       MPI_CHAR,
                       recvbuf,
                       a2a_recvcounts,
                       a2a_recvdispls,
                       recvtype,
                       comm_ptr,
                       mpierrno); 

  return rc;
}




int
MPIDO_Allgather(const void *sendbuf,
                int sendcount,
                MPI_Datatype sendtype,
                void *recvbuf,
                int recvcount,
                MPI_Datatype recvtype,
                MPID_Comm * comm_ptr,
                int *mpierrno)
{
  /* *********************************
   * Check the nature of the buffers
   * *********************************
   */
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
  double config[6];
  int i;
  MPID_Datatype * dt_null = NULL;
  MPI_Aint send_true_lb = 0;
  MPI_Aint recv_true_lb = 0;
  int rc, comm_size = comm_ptr->local_size;
  size_t send_bytes = 0;
  size_t recv_bytes = 0;
  volatile unsigned allgather_active = 1;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
  int queryreq = 0;
#if ASSERT_LEVEL==0
  /* We can't afford the tracing in ndebug/performance libraries */
  const unsigned verbose = 0;
#else
  const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && ((rank == 0) || (rank == 1));
#endif
  const int optimized_algorithm_type = mpid->optimized_algorithm_type[PAMI_XFER_ALLGATHER][0];

  for(i=0;i<6;i++) config[i] = 1.0;
  const pami_metadata_t *my_md = (pami_metadata_t *)NULL;



  char use_allreduce, use_alltoall, use_bcast, use_pami, use_opt;
  char *rbuf = NULL, *sbuf = NULL;
  void *snd_noncontig_buff = NULL, *rcv_noncontig_buff = NULL;

  /* Notes
  allgathers[0]=1; // GLUE_ALLREDUCE 
  allgathers[1]=1; // GLUE_BCAST 
  mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][1] = nnn; // used for GLUE_BCAST 
  mpid.allgathers[2]=1; // GLUE_ALLTOALL 
  */
  const char * const allgathers = mpid->allgathers;

  /* Gather datatype information */
  int rcontig = 0, scontig = 0;
  MPIDI_Datatype_get_info(recvcount,
                          recvtype,
                          rcontig,
                          recv_bytes,
                          dt_null,
                          recv_true_lb);

  /* Check for glue protocols/cutoffs */

  /* alltoall doubles >= 2048, other dts >= 512 */
  use_alltoall = allgathers[2] && 
                (((recvtype == MPI_DOUBLE) && (recv_bytes >= 2048)) 
                    || 
                 (recv_bytes >= 512)
                );

  /* bcast has a cutoff in mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][1] */
  use_bcast = allgathers[1] && (recv_bytes >= mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][1]);

  /* allreduce only when below alltoall or bcast cutoffs */
  use_allreduce = allgathers[0] && !use_alltoall && !use_bcast && (recv_bytes > 3);

  /* Use pami if not specifically using MPICH or a GLUE */
  use_pami = (optimized_algorithm_type == MPID_COLL_USE_MPICH) ? 0 : 1;
  use_pami = use_pami && !use_allreduce && !use_alltoall && !use_bcast ;
     
  use_opt = use_alltoall || use_allreduce || use_bcast || use_pami;


  TRACE_ERR("flags before: b: %d a: %d t: %d p: %d\n", use_bcast, use_alltoall, use_allreduce, use_pami);
  if(!use_opt)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH allgather algorithm\n");
    TRACE_ERR("No options set/available; using MPICH for allgather\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_MPICH");
    return MPIR_Allgather(sendbuf, sendcount, sendtype,
                          recvbuf, recvcount, recvtype,
                          comm_ptr, mpierrno);
  }
  if((sendcount < 1 && sendbuf != MPI_IN_PLACE) || recvcount < 1)
    return MPI_SUCCESS;


  config[MPID_RECV_CONTIG] = rcontig? 1.0 : 0.0;
  config[MPID_RECV_CONTINUOUS] = config[MPID_RECV_CONTIG];

  send_bytes = recv_bytes;
  rbuf = (char *)recvbuf+recv_true_lb;

  if(!rcontig)
  {
    rcv_noncontig_buff = MPIU_Malloc(recv_bytes * size);
    rbuf = rcv_noncontig_buff;
    if(rcv_noncontig_buff == NULL)
    {
      MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                 "Fatal:  Cannot allocate pack buffer");
    }
    if(sendbuf == MPI_IN_PLACE)
    {
      size_t extent;
      MPID_Datatype_get_extent_macro(recvtype,extent);
      MPIR_Localcopy(recvbuf + (rank*recvcount*extent), recvcount, recvtype,
                     rcv_noncontig_buff + (rank*recv_bytes), recv_bytes,MPI_CHAR);
    }
  }

  sbuf = PAMI_IN_PLACE;
  if(sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            scontig, 
                            send_bytes,
                            dt_null,
                            send_true_lb);
    config[MPID_SEND_CONTIG] = scontig? 1.0 : 0.0;
    sbuf = (char *)sendbuf+send_true_lb;
    config[MPID_SEND_CONTINUOUS] = config[MPID_SEND_CONTIG];
    if(!scontig)
    {
      snd_noncontig_buff = MPIU_Malloc(send_bytes);
      sbuf = snd_noncontig_buff;
      if(snd_noncontig_buff == NULL)
      {
        MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                   "Fatal:  Cannot allocate pack buffer");
      }
      MPIR_Localcopy(sendbuf,            sendcount,  sendtype, 
                     snd_noncontig_buff, send_bytes, MPI_CHAR);
    }
  }
  else
    if(unlikely(verbose))
      fprintf(stderr,"allgather MPI_IN_PLACE buffering\n");

  if(unlikely(verbose))
    fprintf(stderr,"allgather recvcount %d, recv_bytes %zd, rcontig %d, sendcount %d, send_bytes %zd, scontig %d\n",
            recvcount,recv_bytes,rcontig,sendcount,send_bytes,scontig);

  /* verify everyone's datatype contiguity */
  if(0) // (use_alltoall || use_allreduce || use_bcast)  // not really applicable anymore since we pack...?
  {
    if(mpid->preallreduces[MPID_ALLGATHER_PREALLREDUCE])
    {
      MPIDO_Allreduce(MPI_IN_PLACE, config, 6, MPI_DOUBLE, MPI_MIN, comm_ptr, mpierrno);
    }

    use_alltoall = allgathers[2] &&
                   (config[MPID_RECV_CONTIG]==1.0) && (config[MPID_SEND_CONTIG]==1.0);

    /* Note: some of the glue protocols use recv_bytes*comm_size rather than 
     * recv_bytes so we use that for comparison here, plus we pass that in
     * to those protocols. */
    use_allreduce =  allgathers[0] &&
                       (config[MPID_RECV_CONTIG]==1.0) && (config[MPID_SEND_CONTIG]==1.0) &&
                       (config[MPID_RECV_CONTINUOUS]==1.0) && (recv_bytes*comm_size%sizeof(unsigned)) == 0;

    use_bcast = allgathers[1];

    TRACE_ERR("flags after: b: %d a: %d t: %d p: %d\n", use_bcast, use_alltoall, use_allreduce, use_pami);
  }
  if(unlikely(verbose))
    fprintf(stderr,"allgather config %f,%f,%f,%f,%f,%f use_alltoall %d, use_allreduce %d, use_bcast %d, use_pami %d \n",
            config[0],config[1],config[2],config[3],config[4],config[5],use_alltoall, use_allreduce, use_bcast, use_pami);

  if(use_pami)
  {
    TRACE_ERR("Using PAMI-level allgather protocol\n");
    pami_xfer_t allgather;
    allgather.cb_done = allgather_cb_done;
    allgather.cookie = (void *)&allgather_active;
    allgather.cmd.xfer_allgather.rcvbuf = rbuf;
    allgather.cmd.xfer_allgather.sndbuf = sbuf;
    allgather.cmd.xfer_allgather.stype = PAMI_TYPE_BYTE;
    allgather.cmd.xfer_allgather.rtype = PAMI_TYPE_BYTE;
    allgather.cmd.xfer_allgather.stypecount = send_bytes;
    allgather.cmd.xfer_allgather.rtypecount = recv_bytes;
    if((mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][0] == 0) || 
       (mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][0] > 0 && mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLGATHER][0] >= send_bytes))
    {
      allgather.algorithm = mpid->optimized_algorithm[PAMI_XFER_ALLGATHER][0];
      my_md = &mpid->optimized_algorithm_metadata[PAMI_XFER_ALLGATHER][0];
      queryreq     = mpid->optimized_algorithm_type[PAMI_XFER_ALLGATHER][0];
    }
    else
    {
      return MPIR_Allgather(sendbuf, sendcount, sendtype,
                            recvbuf, recvcount, recvtype,
                            comm_ptr, mpierrno);
    }

    if(unlikely( queryreq == MPID_COLL_QUERY ||
                 queryreq == MPID_COLL_DEFAULT_QUERY))
    {
      metadata_result_t result = {0};
      TRACE_ERR("Querying allgather protocol %s, type was: %d\n",
                my_md->name,
                optimized_algorithm_type);
      if(my_md->check_fn == NULL)
      {
        /* process metadata bits */
        if((!my_md->check_correct.values.inplace) && (sendbuf == MPI_IN_PLACE))
          result.check.unspecified = 1;
        if(my_md->check_correct.values.rangeminmax)
        {
          if((my_md->range_lo <= recv_bytes) &&
             (my_md->range_hi >= recv_bytes))
            ; /* ok, algorithm selected */
          else
          {
            result.check.range = 1;
            if(unlikely(verbose))
            {
              fprintf(stderr,"message size (%zu) outside range (%zu<->%zu) for %s.\n",
                      recv_bytes,
                      my_md->range_lo,
                      my_md->range_hi,
                      my_md->name);
            }
          }
        }
      }
      else /* calling the check fn is sufficient */
        result = my_md->check_fn(&allgather);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
      if(result.bitmask)
      {
        if(unlikely(verbose))
          fprintf(stderr,"Query failed for %s.  Using MPICH allgather\n",
                  my_md->name);
        MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_MPICH");
        return MPIR_Allgather(sendbuf, sendcount, sendtype,
                              recvbuf, recvcount, recvtype,
                              comm_ptr, mpierrno);
      }
      if(my_md->check_correct.values.asyncflowctl && !(--(comm_ptr->mpid.num_requests)))
      {
        comm_ptr->mpid.num_requests = MPIDI_Process.optimized.num_requests;
        int tmpmpierrno;   
        if(unlikely(verbose))
          fprintf(stderr,"Query barrier required for %s\n", my_md->name);
        MPIDO_Barrier(comm_ptr, &tmpmpierrno);
      }
    }

    if(unlikely(verbose))
    {
      unsigned long long int threadID;
      MPIU_Thread_id_t tid;
      MPIU_Thread_self(&tid);
      threadID = (unsigned long long int)tid;
      fprintf(stderr,"<%llx> Using protocol %s for allgather on %u\n", 
              threadID,
              my_md->name,
              (unsigned) comm_ptr->context_id);
    }
    TRACE_ERR("Calling PAMI_Collective with allgather structure\n");
    MPIDI_Post_coll_t allgather_post;
    MPIDI_Context_post(MPIDI_Context[0], &allgather_post.state, MPIDI_Pami_post_wrapper, (void *)&allgather);

    MPIDI_Update_last_algorithm(comm_ptr, my_md->name);
    MPID_PROGRESS_WAIT_WHILE(allgather_active);
    TRACE_ERR("Allgather done\n");
  }
  else if(use_allreduce)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using protocol GLUE_ALLREDUCE for allgather\n");
    TRACE_ERR("Using allgather via allreduce\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_ALLREDUCE");
    MPIDO_Allgather_allreduce(sendbuf, sendcount, sendtype,
                              recvbuf, recvcount, recvtype,
                              send_true_lb, recv_true_lb, send_bytes, recv_bytes*comm_size, comm_ptr, mpierrno);
  }
  else if(use_alltoall)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using protocol GLUE_ALLTOALL for allgather\n");
    TRACE_ERR("Using allgather via alltoall\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_ALLTOALL");
    MPIDO_Allgather_alltoall(sendbuf, sendcount, sendtype,
                                    recvbuf, recvcount, recvtype,
                                    send_true_lb, recv_true_lb, send_bytes, recv_bytes*comm_size, comm_ptr, mpierrno);
  }
  else if(use_bcast)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using protocol GLUE_BCAST for allgather\n");
    TRACE_ERR("Using allgather via bcast\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_OPT_BCAST");
    MPIDO_Allgather_bcast(sendbuf, sendcount, sendtype,
                                 recvbuf, recvcount, recvtype,
                                 send_true_lb, recv_true_lb, send_bytes, recv_bytes*comm_size, comm_ptr, mpierrno);
  }
  /* Nothing used yet; dump to MPICH */
  else 
  {  
    if(unlikely(verbose))
    fprintf(stderr,"Using MPICH allgather algorithm\n");
    TRACE_ERR("Using allgather via mpich\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLGATHER_MPICH");
    MPIR_Allgather(sendbuf, sendcount, sendtype,
                   recvbuf, recvcount, recvtype,
                   comm_ptr, mpierrno);
  }
  if(!rcontig)
  {
    MPIR_Localcopy(rcv_noncontig_buff, recv_bytes * size, MPI_CHAR,
                   recvbuf,         recvcount,     recvtype);
    MPIU_Free(rcv_noncontig_buff);   
  }
  if(!scontig)  MPIU_Free(snd_noncontig_buff);
  return 0;
}


#ifndef __BGQ__
int
MPIDO_Allgather_simple(const void *sendbuf,
                       int sendcount,
                       MPI_Datatype sendtype,
                       void *recvbuf,
                       int recvcount,
                       MPI_Datatype recvtype,
                       MPID_Comm * comm_ptr,
                       int *mpierrno)
{
  /* *********************************
* Check the nature of the buffers
* *********************************
*/
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
  MPID_Datatype * dt_null = NULL;
  void *snd_noncontig_buff = NULL, *rcv_noncontig_buff = NULL;
  MPI_Aint send_true_lb = 0;
  MPI_Aint recv_true_lb = 0;
  int snd_data_contig = 1, rcv_data_contig = 1;
  size_t send_size = 0;
  size_t recv_size = 0;
  MPID_Segment segment;
  volatile unsigned allgather_active = 1;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;

  const pami_metadata_t *my_md;

  char *rbuf = NULL, *sbuf = NULL;


  if((sendcount < 1 && sendbuf != MPI_IN_PLACE) || recvcount < 1)
    return MPI_SUCCESS;

  /* Gather datatype information */
  MPIDI_Datatype_get_info(recvcount,
                          recvtype,
                          rcv_data_contig,
                          recv_size,
                          dt_null,
                          recv_true_lb);

  send_size = recv_size;

  if(MPIDI_Pamix_collsel_advise != NULL)
  {
    advisor_algorithm_t advisor_algorithms[1];
    int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_ALLGATHER, send_size, advisor_algorithms, 1);
    if(num_algorithms)
    {
      if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
      {
        return MPIR_Allgather(sendbuf, sendcount, sendtype,
                              recvbuf, recvcount, recvtype,
                              comm_ptr, mpierrno); 
      }
    }
  }

  rbuf = (char *)recvbuf+recv_true_lb;

  if(!rcv_data_contig)
  {
    rcv_noncontig_buff = MPIU_Malloc(recv_size * size);
    rbuf = rcv_noncontig_buff;
    if(rcv_noncontig_buff == NULL)
    {
      MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                 "Fatal:  Cannot allocate pack buffer");
    }
    if(sendbuf == MPI_IN_PLACE)
    {
      sbuf = PAMI_IN_PLACE;
      size_t extent;
      MPID_Datatype_get_extent_macro(recvtype,extent);
      MPIR_Localcopy(recvbuf + (rank*recvcount*extent), recvcount, recvtype,
                       rcv_noncontig_buff + (rank*recv_size), recv_size,MPI_CHAR);
    }
  }

  if(sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            snd_data_contig,
                            send_size,
                            dt_null,
                            send_true_lb);

    sbuf = (char *)sendbuf+send_true_lb;

    if(!snd_data_contig)
    {
      snd_noncontig_buff = MPIU_Malloc(send_size);
      sbuf = snd_noncontig_buff;
      if(snd_noncontig_buff == NULL)
      {
        MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                   "Fatal:  Cannot allocate pack buffer");
      }
      DLOOP_Offset last = send_size;
      MPID_Segment_init(sendbuf, sendcount, sendtype, &segment, 0);
      MPID_Segment_pack(&segment, 0, &last, snd_noncontig_buff);
    }
  }

  TRACE_ERR("Using PAMI-level allgather protocol\n");
  pami_xfer_t allgather;
  allgather.cb_done = allgather_cb_done;
  allgather.cookie = (void *)&allgather_active;
  allgather.cmd.xfer_allgather.rcvbuf = rbuf;
  allgather.cmd.xfer_allgather.sndbuf = sbuf;
  allgather.cmd.xfer_allgather.stype = PAMI_TYPE_BYTE;/* stype is ignored when sndbuf == PAMI_IN_PLACE */
  allgather.cmd.xfer_allgather.rtype = PAMI_TYPE_BYTE;
  allgather.cmd.xfer_allgather.stypecount = send_size;
  allgather.cmd.xfer_allgather.rtypecount = recv_size;
  allgather.algorithm = mpid->algorithm_list[PAMI_XFER_ALLGATHER][0][0];
  my_md = &mpid->algorithm_metadata_list[PAMI_XFER_ALLGATHER][0][0];

  TRACE_ERR("Calling PAMI_Collective with allgather structure\n");
  MPIDI_Post_coll_t allgather_post;
  MPIDI_Context_post(MPIDI_Context[0], &allgather_post.state, MPIDI_Pami_post_wrapper, (void *)&allgather);
  TRACE_ERR("Allgather %s\n", MPIDI_Process.context_post.active>0?"posted":"invoked");

  MPIDI_Update_last_algorithm(comm_ptr, my_md->name);
  MPID_PROGRESS_WAIT_WHILE(allgather_active);
  if(!rcv_data_contig)
  {
    MPIR_Localcopy(rcv_noncontig_buff, recv_size * size, MPI_CHAR,
                   recvbuf,         recvcount,     recvtype);
    MPIU_Free(rcv_noncontig_buff);   
  }
  if(!snd_data_contig)  MPIU_Free(snd_noncontig_buff);
  TRACE_ERR("Allgather done\n");
  return MPI_SUCCESS;
}


int
MPIDO_CSWrapper_allgather(pami_xfer_t *allgather,
                          void        *comm)
{
  int mpierrno = 0;
  MPID_Comm   *comm_ptr = (MPID_Comm*)comm;
  MPI_Datatype sendtype, recvtype;
  void *sbuf;
  MPIDI_coll_check_in_place(allgather->cmd.xfer_allgather.sndbuf, &sbuf);
  int rc = MPIDI_Dtpami_to_dtmpi(  allgather->cmd.xfer_allgather.stype,
                                   &sendtype,
                                   NULL,
                                   NULL);
  if(rc == -1) return rc;

  rc = MPIDI_Dtpami_to_dtmpi(  allgather->cmd.xfer_allgather.rtype,
                               &recvtype,
                               NULL,
                               NULL);
  if(rc == -1) return rc;

  rc  =  MPIR_Allgather(sbuf, 
                        allgather->cmd.xfer_allgather.stypecount, sendtype,
                        allgather->cmd.xfer_allgather.rcvbuf, 
                        allgather->cmd.xfer_allgather.rtypecount, recvtype,
                        comm_ptr, &mpierrno);
  if(allgather->cb_done && rc == 0)
    allgather->cb_done(NULL, allgather->cookie, PAMI_SUCCESS);
  return rc;

}
#endif
