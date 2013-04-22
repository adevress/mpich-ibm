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
 * \file src/coll/gather/mpido_gather.c
 * \brief ???
 */

#include <mpidimpl.h>

static void cb_gather(void *ctxt, void *clientdata, pami_result_t err)
{
  unsigned *active = (unsigned *)clientdata;
  TRACE_ERR("cb_gather enter, active: %u\n", (*active));
  MPIDI_Progress_signal();
  (*active)--;
}

int MPIDO_Gather_reduce(const void *sendbuf,
                        int sendcount,
                        MPI_Datatype sendtype,
                        void *recvbuf,
                        int recvcount,
                        MPI_Datatype recvtype,
                        int root,
                        MPID_Comm * comm_ptr,
                        int *mpierrno,
                        char reduce)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
  int rc, sbytes, rbytes, contig;
  char *tempbuf = NULL;
  MPI_Datatype reduce_type  = (sendtype == MPI_DOUBLE)? MPI_DOUBLE     : MPI_INT;
  MPI_Op reduce_op          = (sendtype == MPI_DOUBLE)? MPI_SUM        : MPI_BOR;
  unsigned reduce_type_size = (sendtype == MPI_DOUBLE)? sizeof(double) : sizeof(int);
#if ASSERT_LEVEL==0
  /* We can't afford the tracing in ndebug/performance libraries */
  const unsigned verbose = 0;
#else
  const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && ((rank == root) || (rank == root+1));
#endif
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);


  MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                          rbytes, data_ptr, true_lb);
  if(sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                            sbytes, data_ptr, true_lb);
  }
  else
    sbytes = rbytes;

  if(unlikely(verbose))
    fprintf(stderr,"gather over %s: recv %u(%u), send %u(%u), size %u, reduce_type_size %u\n",reduce==1?"reduce":"allreduce",recvcount, rbytes, sendcount, sbytes, size, reduce_type_size);

  if(rank == root)
  {
    tempbuf = recvbuf;
    /* zero the buffer if we aren't in place */
    if(sendbuf != MPI_IN_PLACE)
    {
      memset(tempbuf, 0, sbytes * size * sizeof(char));
      memcpy(tempbuf+(rank * sbytes), sendbuf, sbytes);
    }
    /* zero before/after our in place contribution */
    else
    {
      /* Init before my sdata */
      memset(tempbuf, 0, (rank * sbytes) * sizeof(char));
      /* Init after my sdata */
      int endsbytes = (rank * sbytes)+ sbytes; 
      memset(tempbuf+endsbytes, 0, ((rbytes*size)-endsbytes) * sizeof(char));
    }
  }
  /* If we aren't root, malloc tempbuf and zero it,
   * then copy our contribution to the right spot in the big buffer */
  else
  {
    tempbuf = MPIU_Malloc(sbytes * size * sizeof(char));
    if(!tempbuf)
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  __FUNCTION__,
                                  __LINE__,
                                  MPI_ERR_OTHER,
                                  "**nomem", 0);

    memset(tempbuf, 0, sbytes * size * sizeof(char));
    memcpy(tempbuf+(rank*sbytes), sendbuf, sbytes);
  }
  /* Switch to comm->coll_fns->fn() */
  if(reduce==1)
    rc = MPIDO_Reduce(rank==root?MPI_IN_PLACE:tempbuf,
                      rank==root?tempbuf:NULL,
                      (sbytes * size)/reduce_type_size,
                      reduce_type,
                      reduce_op,
                      root,
                      comm_ptr,
                      mpierrno);
  else if(reduce==2)
    rc = MPIDO_Allreduce(MPI_IN_PLACE,
                         tempbuf,
                         (sbytes * size)/reduce_type_size,
                         reduce_type,
                         reduce_op,
                         comm_ptr,
                         mpierrno);
  else abort();

  if(rank != root)
    MPIU_Free(tempbuf);

  return rc;
}

int MPIDO_Gather(const void *sendbuf,
                 int sendcount,
                 MPI_Datatype sendtype,
                 void *recvbuf,
                 int recvcount,
                 MPI_Datatype recvtype,
                 int root,
                 MPID_Comm *comm_ptr,
                 int *mpierrno)
{
  void *snd_noncontig_buff = NULL, *rcv_noncontig_buff = NULL, *sbuf = NULL, *rbuf = NULL;
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;
  pami_xfer_t gather;
  MPIDI_Post_coll_t gather_post;
  int why = 0,rcontig=0,scontig=0, send_bytes=-1, recv_bytes = 0;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
#if ASSERT_LEVEL==0
  /* We can't afford the tracing in ndebug/performance libraries */
  const unsigned verbose = 0;
#else
  const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && ((rank == root) || (rank == root+1));
#endif
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
  const int optimized_algorithm_type = mpid->optimized_algorithm_type[PAMI_XFER_GATHER][0];
  double glue_reduce = (double) (unsigned) mpid->optgather[0];
  const char glue_allgather = mpid->optgather[1];
  double rcvvals[3] = {0.0,0.0,0.0}; /* for coordinating the root recv parms with senders for glue protocols */
  /*   Make life easier for the common (?) case by resetting null parms: 
  if(root) 
    MPI_Gather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, results, NTIMES, MPI_INT, 
               0, MPI_COMM_WORLD);
  else
    MPI_Gather(counter_vals, NTIMES, MPI_INT, NULL, 0, MPI_DATATYPE_NULL, 
               0, MPI_COMM_WORLD);
  */
  if(sendbuf == MPI_IN_PLACE)
  {
    sendtype = recvtype;
    sendcount = recvcount; 
  }
  else if(sendtype == MPI_DATATYPE_NULL) /* valid? */
  {
    sendtype = recvtype;
    sendcount = recvcount;
  }
  if((recvtype == MPI_DATATYPE_NULL) || 
     (rank != root))                    /* non-roots don't trust recvtype */
  {
    recvtype = sendtype;
    recvcount = sendcount;
  }

  if(glue_allgather)
  {
    MPIDI_Datatype_get_info(recvcount, recvtype, rcontig,
                            recv_bytes, data_ptr, true_lb);
    MPIDI_Datatype_get_info(sendcount, sendtype, scontig,
                            send_bytes, data_ptr, true_lb);
    rcvvals[1] = (scontig && rcontig)? 0.0 : 1.0;            /* Don't support (1.0) noncontig, mainly because nonroots don't have the 
                                                                same recv type as the root */
    if(rank == root)
    {
      rcvvals[0] = (recv_bytes & 0x3) ? 1.0 : 0.0;        /* recv data size multiple of int's */
      rcvvals[2] = recv_bytes * size * 1.0;               /* total bytes recv  */
    }
    else
      rcvvals[2] = send_bytes * size * 1.0;               /* total bytes recv  */

    int met_cutoff = 1;  /* combine various checks into one flag */

    if((comm_ptr->mpid.optimized_algorithm_cutoff_size[PAMI_XFER_GATHER][0] > 0) /* non-zero cutoff */
       && (rcvvals[2] >= comm_ptr->mpid.optimized_algorithm_cutoff_size[PAMI_XFER_GATHER][0])) /* over cutoff */
      met_cutoff = 0;

    if (rcvvals[2] > MPIDI_Process.optimized.max_alloc)    /* too big to allocate ? don't do it */
      met_cutoff = 0;

    if(met_cutoff)
      MPIDO_Allreduce(MPI_IN_PLACE,
                    rcvvals,
                    2,
                    MPI_DOUBLE,
                    MPI_MAX,
                    comm_ptr,
                    mpierrno);
    if(unlikely(verbose))
    {
      fprintf(stderr,"Checking protocol GLUE_ALLGATHER for gather recv bytes %u/%u (%f), contig %f, recv buffer size %f\n",recv_bytes/recvcount,recv_bytes,rcvvals[0],rcvvals[1],rcvvals[2]);
    }
    if((rcvvals[0] == 0.0) &&                               /* Integers or better */
       (rcvvals[1] == 0.0) &&                               /* Not going to handle noncontig with GLUE_ALLGATHER */
       (met_cutoff))                                        /* met cut offs */
    {
      MPIDI_Update_last_algorithm(comm_ptr, "GLUE_ALLGATHER");
      /* Allgather (over allreduce) is too tricky with doubles, we can get nan's or mismatched dt's and sizes */
      if(sendtype == MPI_DOUBLE)
      {
        sendtype = MPI_INT;
        sendcount *= 2;
      }
      if(recvtype == MPI_DOUBLE)
      {
        recvtype = MPI_INT;
        recvcount *= 2;
      }
      if(rank != root)
      {
        if(unlikely(verbose))
        {
          fprintf(stderr,"Using protocol GLUE_ALLGATHER for gather (non-root),size %d, root %d, recv count %d, recv_bytes %d, sendcount %d, send_bytes %d, contig %d on %u\n",size, root, recvcount, recv_bytes, sendcount, send_bytes/sendcount,scontig,(unsigned) comm_ptr->context_id);
        }
        void* tmp = MPIU_Malloc((size_t)rcvvals[2]);
        MPIDO_Allgather(sendbuf,
                        sendcount,
                        sendtype,
                        tmp,
                        sendcount,
                        sendtype,
                        comm_ptr,
                        mpierrno);
        MPIU_Free(tmp);
        return 0;
      }
      else
      {
        if(unlikely(verbose))
        {
          fprintf(stderr,"Using protocol GLUE_ALLGATHER for gather,size %d, root %d, recv count %d, recv_bytes %d, sendcount %d, send_bytes %d, contig %d on %u\n",size, root, recvcount, recv_bytes, sendcount, send_bytes, scontig, (unsigned) comm_ptr->context_id);
        }
        return MPIDO_Allgather(sendbuf,
                               sendcount,
                               sendtype,
                               recvbuf,
                               recvcount,
                               recvtype,
                               comm_ptr,
                               mpierrno);
      }
    }
  }
  if(glue_reduce!=0.0)
  {
    int contig;
    MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                            recv_bytes, data_ptr, true_lb);
    MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                            send_bytes, data_ptr, true_lb);
    if((send_bytes * size) > MPIDI_Process.optimized.max_alloc) 
    {
      why = __LINE__;  
      glue_reduce = 0.0;
    }
    if(!rcontig || ((recv_bytes * size) % sizeof(int))) /* ? */
    {
      why = __LINE__;  
      glue_reduce = 0.0;
    }
    if(!scontig || ((send_bytes * size) % sizeof(int)))
    {
      why = __LINE__;  
      glue_reduce = 0.0;
    }
  }

  if(glue_reduce!=0.0)
  {
    /* Switch to comm->coll_fns->fn() */
    MPIDO_Allreduce(MPI_IN_PLACE, &glue_reduce, 1, MPI_DOUBLE, MPI_MIN, comm_ptr, mpierrno);
    if(unlikely(verbose))
      fprintf(stderr,"MPID_GATHER_PREALLREDUCE glue_reduce %f, selected type %d, why %d\n",glue_reduce,optimized_algorithm_type, why);
    if(glue_reduce!=0.0)
    {
      if(unlikely(verbose))
        fprintf(stderr,"Using protocol %s for gather, root %d on %u\n",glue_reduce==1.0?"GLUE_REDUCE":"GLUE_ALLREDUCE", root, (unsigned) comm_ptr->context_id);
      MPIDI_Update_last_algorithm(comm_ptr, glue_reduce==1.0?"GLUE_REDUCE":"GLUE_ALLREDUCE");
      return MPIDO_Gather_reduce(sendbuf,
                                 sendcount,
                                 sendtype,
                                 recvbuf,
                                 (rank == root)?recvcount:sendcount,
                                 (rank == root)?recvtype:sendtype,
                                 root,
                                 comm_ptr,
                                 mpierrno,
                                 glue_reduce==1.0?'1':'2');
    }
  }
  if(optimized_algorithm_type == MPID_COLL_USE_MPICH)
  {
    MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH gather algorithm (glue_reduce=%f), selected type %d on %u\n",glue_reduce,optimized_algorithm_type, (unsigned) comm_ptr->context_id);
    return MPIR_Gather(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr, mpierrno);
  }


  MPIDI_Datatype_get_info(recvcount, recvtype, rcontig,
                          recv_bytes, data_ptr, true_lb);
  if(rank == root)
    if(unlikely(verbose))
      fprintf(stderr,"Root of gather glue_reduce %f, selected type %d, recv count %d, recv bytes %du, size %d, contig %d\n",glue_reduce,optimized_algorithm_type, recvcount, recv_bytes, size, rcontig);
  MPIDI_Datatype_get_info(sendcount, sendtype, scontig,
                          send_bytes, data_ptr, true_lb);
  if(unlikely(verbose))
    fprintf(stderr,"gather(%u) glue_reduce %f, selected type %d, send count %d, send bytes %d, size %d, contig %d\n",__LINE__, glue_reduce,optimized_algorithm_type, sendcount, send_bytes, size, scontig);

  pami_algorithm_t my_gather;
  const pami_metadata_t *my_md = (pami_metadata_t *)NULL;
  int queryreq = 0;
  volatile unsigned active = 1;

  gather.cb_done = cb_gather;
  gather.cookie = (void *)&active;
  gather.cmd.xfer_gather.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
  if(sendbuf == MPI_IN_PLACE)
  {
    if(unlikely(verbose))
      fprintf(stderr,"gather MPI_IN_PLACE buffering\n");
    gather.cmd.xfer_gather.stypecount = recv_bytes;
    gather.cmd.xfer_gather.sndbuf = PAMI_IN_PLACE;
  }
  else
  {
    gather.cmd.xfer_gather.stypecount = send_bytes;
    gather.cmd.xfer_gather.sndbuf = (void *)sendbuf;
  }
  gather.cmd.xfer_gather.stype = PAMI_TYPE_BYTE;
  gather.cmd.xfer_gather.rcvbuf = (void *)recvbuf;
  gather.cmd.xfer_gather.rtype = PAMI_TYPE_BYTE;
  gather.cmd.xfer_gather.rtypecount = recv_bytes;

  /* If glue-level protocols are good, this will require some changes */
  TRACE_ERR("Optimized gather (%s) was pre-selected\n",
            mpid->optimized_algorithm_metadata[PAMI_XFER_GATHER][0].name);
  my_gather = mpid->optimized_algorithm[PAMI_XFER_GATHER][0];
  my_md = &mpid->optimized_algorithm_metadata[PAMI_XFER_GATHER][0];
  queryreq = mpid->optimized_algorithm_type[PAMI_XFER_GATHER][0];

  gather.algorithm = my_gather;
  if(unlikely(queryreq == MPID_COLL_QUERY || 
              queryreq == MPID_COLL_DEFAULT_QUERY))
  {
    metadata_result_t result = {0};
    TRACE_ERR("querying gather protocol %s, type was %d\n",
              my_md->name, queryreq);
    if(my_md->check_fn == NULL)
    {
      /* process metadata bits */
      if((!my_md->check_correct.values.inplace) && (sendbuf == MPI_IN_PLACE))
        result.check.unspecified = 1;
      if(my_md->check_correct.values.rangeminmax)
      {
        /* Non-local decision? */
        if(((rank == root) &&
            (my_md->range_lo <= recv_bytes) &&
            (my_md->range_hi >= recv_bytes)
           ) &&
           ((rank != root) &&
            (my_md->range_lo <= send_bytes) &&
            (my_md->range_hi >= send_bytes)
           )
          )
          ; /* ok, algorithm selected */
        else
        {
          result.check.range = 1;
          if(unlikely(verbose))
          {
            fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                    recv_bytes,
                    my_md->range_lo,
                    my_md->range_hi,
                    my_md->name);
          }
        }
      }
    }
    else /* calling the check fn is sufficient */
      result = my_md->check_fn(&gather);
    TRACE_ERR("bitmask: %#X\n", result.bitmask);
    result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
    if(result.bitmask)
    {
      if(unlikely(verbose))
        fprintf(stderr,"query failed for %s\n", my_md->name);
      MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
      return MPIR_Gather(sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, recvtype,
                         root, comm_ptr, mpierrno);
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

  MPIDI_Update_last_algorithm(comm_ptr, my_md->name);

  if(unlikely(verbose))
  {
    unsigned long long int threadID;
    MPIU_Thread_id_t tid;
    MPIU_Thread_self(&tid);
    threadID = (unsigned long long int)tid;
    fprintf(stderr,"<%llx> Using protocol %s for gather on %u\n", 
            threadID,
            my_md->name,
            (unsigned) comm_ptr->context_id);
  }

  MPIDI_Context_post(MPIDI_Context[0], &gather_post.state,
                     MPIDI_Pami_post_wrapper, (void *)&gather);

  TRACE_ERR("Waiting on active: %d\n", active);
  MPID_PROGRESS_WAIT_WHILE(active);

  TRACE_ERR("Leaving MPIDO_Gather\n");
  return 0;
}


#ifndef __BGQ__
int MPIDO_Gather_simple(const void *sendbuf,
                        int sendcount,
                        MPI_Datatype sendtype,
                        void *recvbuf,
                        int recvcount,
                        MPI_Datatype recvtype,
                        int root,
                        MPID_Comm *comm_ptr,
                        int *mpierrno)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;
  pami_xfer_t gather;
  MPIDI_Post_coll_t gather_post;
  int success = 1, snd_contig = 1, rcv_contig = 1;
  void *snd_noncontig_buff = NULL, *rcv_noncontig_buff = NULL;
  void *sbuf = NULL, *rbuf = NULL;
  int send_size = 0;
  int recv_size = 0;
  MPID_Segment segment;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);

  if(sendbuf != MPI_IN_PLACE)
  {
    MPIDI_Datatype_get_info(sendcount, sendtype, snd_contig,
                            send_size, data_ptr, true_lb);
    if(MPIDI_Pamix_collsel_advise != NULL)
    {
      advisor_algorithm_t advisor_algorithms[1];
      int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_GATHER, send_size, advisor_algorithms, 1);
      if(num_algorithms)
      {
        if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
        {
          return MPIR_Gather(sendbuf, sendcount, sendtype,
                             recvbuf, recvcount, recvtype,
                             root, comm_ptr, mpierrno);
        }
      }
    }

    sbuf = (char *)sendbuf + true_lb;
    if(!snd_contig)
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
  else
  {
    if(MPIDI_Pamix_collsel_advise != NULL)
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, rcv_contig,
                              recv_size, data_ptr, true_lb);
      advisor_algorithm_t advisor_algorithms[1];
      int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_GATHER, recv_size, advisor_algorithms, 1);
      if(num_algorithms)
      {
        if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
        {
          return MPIR_Gather(sendbuf, sendcount, sendtype,
                             recvbuf, recvcount, recvtype,
                             root, comm_ptr, mpierrno);
        }
      }
    }
  }

  rbuf = (char *)recvbuf + true_lb;
  if(rank == root)
  {
    if(recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, rcv_contig,
                              recv_size, data_ptr, true_lb);
      rbuf = (char *)recvbuf + true_lb;
      if(!rcv_contig)
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
          size_t extent;
          MPID_Datatype_get_extent_macro(recvtype,extent);
          MPIR_Localcopy(recvbuf + (rank*recvcount*extent), recvcount, recvtype,
                         rcv_noncontig_buff + (rank*recv_size), recv_size,MPI_CHAR);
        }
      }
    }
    else
      success = 0;
  }

  if(!success)
  {
    MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
    return MPIR_Gather(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr, mpierrno);
  }


  const pami_metadata_t *my_gather_md;
  volatile unsigned active = 1;

  gather.cb_done = cb_gather;
  gather.cookie = (void *)&active;
  gather.cmd.xfer_gather.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
  gather.cmd.xfer_gather.stypecount = send_size;/* stypecount is ignored when sndbuf == PAMI_IN_PLACE */
  gather.cmd.xfer_gather.sndbuf = (void *)sbuf;
  if(sendbuf == MPI_IN_PLACE)
  {
    gather.cmd.xfer_gather.sndbuf = PAMI_IN_PLACE;
  }
  gather.cmd.xfer_gather.stype = PAMI_TYPE_BYTE;/* stype is ignored when sndbuf == PAMI_IN_PLACE */
  gather.cmd.xfer_gather.rcvbuf = (void *)rbuf;
  gather.cmd.xfer_gather.rtype = PAMI_TYPE_BYTE;
  gather.cmd.xfer_gather.rtypecount = recv_size;
  gather.algorithm = mpid->algorithm_list[PAMI_XFER_GATHER][0][0];
  my_gather_md = &mpid->algorithm_metadata_list[PAMI_XFER_GATHER][0][0];

  MPIDI_Update_last_algorithm(comm_ptr,
                              my_gather_md->name);

  TRACE_ERR("%s gather\n", MPIDI_Process.context_post.active>0?"Posting":"Invoking");
  MPIDI_Context_post(MPIDI_Context[0], &gather_post.state,
                     MPIDI_Pami_post_wrapper, (void *)&gather);

  TRACE_ERR("Waiting on active: %d\n", active);
  MPID_PROGRESS_WAIT_WHILE(active);

  if(!rcv_contig)
  {
    MPIR_Localcopy(rcv_noncontig_buff, recv_size*size, MPI_CHAR,
                   recvbuf,         recvcount*size,     recvtype);
    MPIU_Free(rcv_noncontig_buff);
  }
  if(!snd_contig)  MPIU_Free(snd_noncontig_buff);

  TRACE_ERR("Leaving MPIDO_Gather_optimized\n");
  return MPI_SUCCESS;
}

int
MPIDO_CSWrapper_gather(pami_xfer_t *gather,
                       void        *comm)
{
  int mpierrno = 0;
  MPID_Comm   *comm_ptr = (MPID_Comm*)comm;
  MPI_Datatype sendtype, recvtype;
  void *sbuf;
  MPIDI_coll_check_in_place(gather->cmd.xfer_gather.sndbuf, &sbuf);
  int rc = MPIDI_Dtpami_to_dtmpi(  gather->cmd.xfer_gather.stype,
                                   &sendtype,
                                   NULL,
                                   NULL);
  if(rc == -1) return rc;

  rc = MPIDI_Dtpami_to_dtmpi(  gather->cmd.xfer_gather.rtype,
                               &recvtype,
                               NULL,
                               NULL);
  if(rc == -1) return rc;

  rc  =  MPIR_Gather(sbuf,
                     gather->cmd.xfer_gather.stypecount, sendtype,
                     gather->cmd.xfer_gather.rcvbuf,
                     gather->cmd.xfer_gather.rtypecount, recvtype,
                     gather->cmd.xfer_gather.root, comm_ptr, &mpierrno);

  if(gather->cb_done && rc == 0)
    gather->cb_done(NULL, gather->cookie, PAMI_SUCCESS);
  return rc;

}
#endif
