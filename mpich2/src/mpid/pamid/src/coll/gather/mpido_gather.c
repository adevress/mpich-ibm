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

static void cb_allred(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_allred preallred enter, active: %u\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}
int MPIDO_Gather_reduce(void * sendbuf,
			int sendcount,
			MPI_Datatype sendtype,
			void *recvbuf,
			int recvcount,
			MPI_Datatype recvtype,
			int root,
			MPID_Comm * comm_ptr,
			int *mpierrno)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
  int rc, sbytes, rbytes, contig;
  char *tempbuf = NULL;
  char *inplacetemp = NULL;

  MPIDI_Datatype_get_info(sendcount, sendtype, contig,
			  sbytes, data_ptr, true_lb);

  MPIDI_Datatype_get_info(recvcount, recvtype, contig,
			  rbytes, data_ptr, true_lb);

  if(rank == root)
  {
    tempbuf = recvbuf;
    /* zero the buffer if we aren't in place */
    if(sendbuf != MPI_IN_PLACE)
    {
      memset(tempbuf, 0, sbytes * size * sizeof(char));
      memcpy(tempbuf+(rank * sbytes), sendbuf, sbytes);
    }
    /* copy our contribution temporarily, zero the buffer, put it back */
    /* this will likely suck */
    else
    {
      inplacetemp = MPIU_Malloc(rbytes * sizeof(char));
      memcpy(inplacetemp, recvbuf+(rank * rbytes), rbytes);
      memset(tempbuf, 0, sbytes * size * sizeof(char));
      memcpy(tempbuf+(rank * rbytes), inplacetemp, rbytes);
      MPIU_Free(inplacetemp);
    }
  }
  /* everyone might need to speciifcally allocate a tempbuf, or
   * we might need to make sure we don't end up at mpich in the
   * mpido_() functions - seems to be a problem?
   */

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
  /* #warning TODO need an optimal reduce */
  rc = MPIR_Reduce(MPI_IN_PLACE,
                    tempbuf,
                    (sbytes * size)/4,
                    MPI_INT,
                    MPI_BOR,
                    root,
                    comm_ptr,
                    mpierrno);

  if(rank != root)
    MPIU_Free(tempbuf);

  return rc;
}

/* works for simple data types, assumes fast reduce is available */

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
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;
  pami_xfer_t gather;
  MPIDI_Post_coll_t gather_post;
  int use_opt = 1, contig=0, send_bytes=-1, recv_bytes = 0;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
#if ASSERT_LEVEL==0
   /* We can't afford the tracing in ndebug/performance libraries */
    const unsigned verbose = 0;
#else
    const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && (rank == 0);
#endif
   const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
   const int selected_type = mpid->user_selected_type[PAMI_XFER_GATHER];

  if (rank == root)
  {
    if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                              recv_bytes, data_ptr, true_lb);
      if (!contig || ((recv_bytes * size) % sizeof(int))) /* ? */
        use_opt = 0;
    }
    else
      use_opt = 0;
  }

  if ((sendbuf != MPI_IN_PLACE) && sendtype != MPI_DATATYPE_NULL && sendcount >= 0)
  {
    MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                            send_bytes, data_ptr, true_lb);
    if (!contig || ((send_bytes * size) % sizeof(int)))
      use_opt = 0;
  }
  else 
  {
    if(sendbuf == MPI_IN_PLACE)
      send_bytes = recv_bytes;
    if (sendtype == MPI_DATATYPE_NULL || sendcount == 0)
    {
      send_bytes = 0;
      use_opt = 0;
    }
  }

  if(!mpid->optgather &&
   selected_type == MPID_COLL_USE_MPICH)
  {
    MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH gather algorithm (01) opt %x, selected type %d\n",mpid->optgather,selected_type);
    return MPIR_Gather(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr, mpierrno);
  }
  if(mpid->preallreduces[MPID_GATHER_PREALLREDUCE])
  {
    if(unlikely(verbose))
      fprintf(stderr,"MPID_GATHER_PREALLREDUCE opt %x, selected type %d, use_opt %d\n",mpid->optgather,selected_type, use_opt);
    volatile unsigned allred_active = 1;
    pami_xfer_t allred;
    MPIDI_Post_coll_t allred_post;
    allred.cb_done = cb_allred;
    allred.cookie = (void *)&allred_active;
    /* Guaranteed to work allreduce */
    allred.algorithm = mpid->coll_algorithm[PAMI_XFER_ALLREDUCE][0][0];
    allred.cmd.xfer_allreduce.sndbuf = (void *)PAMI_IN_PLACE;
    allred.cmd.xfer_allreduce.stype = PAMI_TYPE_SIGNED_INT;
    allred.cmd.xfer_allreduce.rcvbuf = (void *)&use_opt;
    allred.cmd.xfer_allreduce.rtype = PAMI_TYPE_SIGNED_INT;
    allred.cmd.xfer_allreduce.stypecount = 1;
    allred.cmd.xfer_allreduce.rtypecount = 1;
    allred.cmd.xfer_allreduce.op = PAMI_DATA_BAND;
    
    MPIDI_Context_post(MPIDI_Context[0], &allred_post.state,
                       MPIDI_Pami_post_wrapper, (void *)&allred);
    MPID_PROGRESS_WAIT_WHILE(allred_active);
    if(unlikely(verbose))
      fprintf(stderr,"MPID_GATHER_PREALLREDUCE opt %x, selected type %d, use_opt %d\n",mpid->optgather,selected_type, use_opt);
  }

  if(mpid->optgather)
  {
    if(use_opt)
    {
      MPIDI_Update_last_algorithm(comm_ptr, "GLUE_REDUCDE");
      abort();
      /* GLUE_REDUCE ? */
    }
    else
    {
      MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
      if(unlikely(verbose))
        fprintf(stderr,"Using MPICH gather algorithm (02) opt %x, selected type %d, use_opt %d\n",mpid->optgather,selected_type, use_opt);
      return MPIR_Gather(sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, recvtype,
                         root, comm_ptr, mpierrno);
    }
  }


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
   if(selected_type == MPID_COLL_OPTIMIZED)
   {
      TRACE_ERR("Optimized gather (%s) was pre-selected\n",
         mpid->opt_protocol_md[PAMI_XFER_GATHER][0].name);
      my_gather = mpid->opt_protocol[PAMI_XFER_GATHER][0];
      my_md = &mpid->opt_protocol_md[PAMI_XFER_GATHER][0];
      queryreq = mpid->must_query[PAMI_XFER_GATHER][0];
   }
   else
   {
      TRACE_ERR("Optimized gather (%s) was specified by user\n",
      mpid->user_metadata[PAMI_XFER_GATHER].name);
      my_gather = mpid->user_selected[PAMI_XFER_GATHER];
      my_md = &mpid->user_metadata[PAMI_XFER_GATHER];
      queryreq = selected_type;
   }

   gather.algorithm = my_gather;
   if(unlikely(queryreq == MPID_COLL_ALWAYS_QUERY || 
               queryreq == MPID_COLL_CHECK_FN_REQUIRED))
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
             ((my_md->range_lo <= send_bytes) &&
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
      if(my_md->check_correct.values.asyncflowctl) 
      { /* need better flow control than a barrier every time */
        int tmpmpierrno;   
        if(unlikely(verbose))
          fprintf(stderr,"Query barrier required for %s\n", my_md->name);
        MPIR_Barrier(comm_ptr, &tmpmpierrno);
      }
   }

   MPIDI_Update_last_algorithm(comm_ptr,
            mpid->user_metadata[PAMI_XFER_GATHER].name);


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


