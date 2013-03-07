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
 * \file src/coll/scatter/mpido_scatter.c
 * \brief ???
 */

/* #define TRACE_ON */

#include <mpidimpl.h>

static void cb_scatter(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_scatter enter, active: %u\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}

/* works for simple data types, assumes fast bcast is available */
int MPIDO_Scatter_bcast(void * sendbuf,
			int sendcount,
			MPI_Datatype sendtype,
			void *recvbuf,
			int recvcount,
			MPI_Datatype recvtype,
			int root,
			MPID_Comm *comm_ptr,
                        int *mpierrno)
{
  /* Pretty simple - bcast a temp buffer and copy our little chunk out */
  int contig, nbytes, rc;
  const int rank = comm_ptr->rank;
  const int size = comm_ptr->local_size;
  char *tempbuf = NULL;

  MPID_Datatype * dt_ptr;
  MPI_Aint true_lb = 0;

  if(rank == root)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);
    tempbuf = sendbuf;
  }
  else
  {
    MPIDI_Datatype_get_info(recvcount,
                            recvtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);

    tempbuf = MPIU_Malloc(nbytes * size);
    if(!tempbuf)
    {
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  __FUNCTION__,
                                  __LINE__,
                                  MPI_ERR_OTHER,
                                  "**nomem", 0);
    }
  }

  /* Switch to comm->coll_fns->fn() */
  rc = MPIDO_Bcast(tempbuf, nbytes*size, MPI_CHAR, root, comm_ptr, mpierrno);

  if(rank == root && recvbuf == MPI_IN_PLACE)
    return rc;
  else
    memcpy(recvbuf, tempbuf+(rank*nbytes), nbytes);

  if (rank!=root)
    MPIU_Free(tempbuf);

  return rc;
}


int MPIDO_Scatter(const void *sendbuf,
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
  int contig, nbytes = 0;
  const int rank = comm_ptr->rank;
  int success = 1;
  pami_type_t stype, rtype;
  int tmp;
#if ASSERT_LEVEL==0
   /* We can't afford the tracing in ndebug/performance libraries */
    const unsigned verbose = 0;
#else
    const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && (rank == 0);
#endif
   const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
   const int optimized_algorithm_type = mpid->optimized_algorithm_type[PAMI_XFER_SCATTER][0];
   char use_pami = 1;


  if((optimized_algorithm_type == MPID_COLL_USE_MPICH) && !(mpid->optscatter))
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH scatter algorithm\n");
    MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr, mpierrno);
  }
  /* if (rank == root)
     We can't decide on just the root to use MPICH. Really need a pre-allreduce.
     For now check sendtype on non-roots too and hope it matches? I think that's what
     scatterv does...
   
     TODO: Actually, we need to convert non-pami and non-contig to PAMI chars and forget
     this 'use_pami' stuff.  It doesn't work that way.
*/
  {
    if(MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      use_pami = 0;
  }
  if(recvbuf != MPI_IN_PLACE && (MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS))
    use_pami = 0;

  if(!use_pami) // TODO: Actually, we need to convert non-pami and non-contig to PAMI

  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH scatter algorithm for unsupported data type\n");
    MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr, mpierrno);
  }

  if (rank == root)
  {
    if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)/* Should this be send or recv? */
    {
      MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                              nbytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
    else
      success = 0;

    if (success)
    {
      if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
      {
        MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                                nbytes, data_ptr, true_lb);
        if (!contig) success = 0;
      }
      else success = 0;
    }
  }

  else
  {
    if (sendtype != MPI_DATATYPE_NULL && sendcount >= 0)/* Should this be send or recv? */
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                              nbytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
    else
      success = 0;
  }

  if(mpid->optscatter)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using GLUE_BCAST scatter algorithm\n");
//    if(use_glue)
    {
      char* sbuf = (char *)sendbuf+true_lb;
      char* rbuf = (char *)recvbuf+true_lb;

      MPIDI_Update_last_algorithm(comm_ptr, "GLUE_BCAST");
      return MPIDO_Scatter_bcast(sbuf, sendcount, sendtype,
                                 rbuf, recvcount, recvtype,
                                 root, comm_ptr, mpierrno);
    }
/*    else
    {
      MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
      if(unlikely(verbose))
        fprintf(stderr,"Using MPICH scatter algorithm (%x), selected type %d, use_glue %d\n",mpid->optscatter,optimized_algorithm_type, use_glue);
      return MPIR_Scatter(sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, recvtype,
                         root, comm_ptr, mpierrno);
    }
 */
  }
  
   pami_xfer_t scatter;
   MPIDI_Post_coll_t scatter_post;
   pami_algorithm_t my_scatter;
   const pami_metadata_t *my_md = (pami_metadata_t *)NULL;
   volatile unsigned scatter_active = 1;
   int queryreq = 0;

      TRACE_ERR("Optimized scatter %s was selected\n",
         mpid->optimized_algorithm_metadata[PAMI_XFER_SCATTER][0].name);
      my_scatter = mpid->optimized_algorithm[PAMI_XFER_SCATTER][0];
      my_md = &mpid->optimized_algorithm_metadata[PAMI_XFER_SCATTER][0];
      queryreq = mpid->optimized_algorithm_type[PAMI_XFER_SCATTER][0];
 
   scatter.algorithm = my_scatter;
   scatter.cb_done = cb_scatter;
   scatter.cookie = (void *)&scatter_active;
   scatter.cmd.xfer_scatter.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
   scatter.cmd.xfer_scatter.sndbuf = (void *)sendbuf;
   scatter.cmd.xfer_scatter.stype = stype;
   scatter.cmd.xfer_scatter.stypecount = sendcount;
   if(recvbuf == MPI_IN_PLACE) 
   {
     if(unlikely(verbose))
       fprintf(stderr,"scatter MPI_IN_PLACE buffering\n");
     MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                             nbytes, data_ptr, true_lb);
     scatter.cmd.xfer_scatter.rcvbuf = PAMI_IN_PLACE;
     scatter.cmd.xfer_scatter.rtype = stype;
     scatter.cmd.xfer_scatter.rtypecount = sendcount;
   }
   else
   {
     scatter.cmd.xfer_scatter.rcvbuf = (void *)recvbuf;
     scatter.cmd.xfer_scatter.rtype = rtype;
     scatter.cmd.xfer_scatter.rtypecount = recvcount;
   }

   if(unlikely(queryreq == MPID_COLL_QUERY ||
               queryreq == MPID_COLL_DEFAULT_QUERY))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying scatter protoocl %s, type was %d\n",
         my_md->name, queryreq);
      if(my_md->check_fn == NULL)
      {
        /* process metadata bits */
        if((!my_md->check_correct.values.inplace) && (recvbuf == MPI_IN_PLACE))
           result.check.unspecified = 1;
         if(my_md->check_correct.values.rangeminmax)
         {
           MPI_Aint data_true_lb;
           MPID_Datatype *data_ptr;
           int data_size, data_contig;
           if(rank == root)
             MPIDI_Datatype_get_info(sendcount, sendtype, data_contig, data_size, data_ptr, data_true_lb); 
           else
             MPIDI_Datatype_get_info(recvcount, recvtype, data_contig, data_size, data_ptr, data_true_lb); 
           if((my_md->range_lo <= data_size) &&
              (my_md->range_hi >= data_size))
              ; /* ok, algorithm selected */
           else
           {
              result.check.range = 1;
              if(unlikely(verbose))
              {   
                 fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                         data_size,
                         my_md->range_lo,
                         my_md->range_hi,
                         my_md->name);
              }
           }
         }
      }
      else /* calling the check fn is sufficient */
        result = my_md->check_fn(&scatter);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
      if(result.bitmask)
      {
        if(unlikely(verbose))
          fprintf(stderr,"query failed for %s\n", my_md->name);
        MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
        return MPIR_Scatter(sendbuf, sendcount, sendtype,
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

   if(unlikely(verbose))
   {
      unsigned long long int threadID;
      MPIU_Thread_id_t tid;
      MPIU_Thread_self(&tid);
      threadID = (unsigned long long int)tid;
      fprintf(stderr,"<%llx> Using protocol %s for scatter on %u\n", 
              threadID,
              my_md->name,
              (unsigned) comm_ptr->context_id);
   }
   MPIDI_Context_post(MPIDI_Context[0], &scatter_post.state,
                      MPIDI_Pami_post_wrapper, (void *)&scatter);
   TRACE_ERR("Waiting on active %d\n", scatter_active);
   MPID_PROGRESS_WAIT_WHILE(scatter_active);


   TRACE_ERR("Leaving MPIDO_Scatter\n");

   return 0;
}


#ifndef __BGQ__
int MPIDO_Scatter_simple(const void *sendbuf,
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
  const int rank = comm_ptr->rank;
  int success = 1, snd_contig = 1, rcv_contig = 1;
  void *snd_noncontig_buff = NULL, *rcv_noncontig_buff = NULL;
  void *sbuf = NULL, *rbuf = NULL;
  size_t send_size = 0;
  size_t recv_size = 0;
  MPI_Aint snd_true_lb = 0, rcv_true_lb = 0; 
  int tmp;
  int use_pami = 1;
  MPID_Segment segment;
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
  const int size = comm_ptr->local_size;

  if(rank == root && sendtype != MPI_DATATYPE_NULL && sendcount >= 0)
  {
    MPIDI_Datatype_get_info(sendcount, sendtype, snd_contig,
                            send_size, data_ptr, snd_true_lb);
    if(MPIDI_Pamix_collsel_advise != NULL)
    {
      advisor_algorithm_t advisor_algorithms[1];
      int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_SCATTER, send_size, advisor_algorithms, 1);
      if(num_algorithms)
      {
        if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
        {
          return MPIR_Scatter(sendbuf, sendcount, sendtype,
                              recvbuf, recvcount, recvtype,
                              root, comm_ptr, mpierrno);
        }
      }
    }
  }

  if(recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
  {
    MPIDI_Datatype_get_info(recvcount, recvtype, rcv_contig,
                            recv_size, data_ptr, rcv_true_lb);
    if(MPIDI_Pamix_collsel_advise != NULL)
    {
      advisor_algorithm_t advisor_algorithms[1];
      int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_SCATTER, recv_size, advisor_algorithms, 1);
      if(num_algorithms)
      {
        if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
        {
          return MPIR_Scatter(sendbuf, sendcount, sendtype,
                              recvbuf, recvcount, recvtype,
                              root, comm_ptr, mpierrno);
        }
      }
    }
  }
  sbuf = (char *)sendbuf + snd_true_lb;
  rbuf = (char *)recvbuf + rcv_true_lb;
  if(rank == root)
  {
    if(send_size)
    {
      sbuf = (char *)sendbuf + snd_true_lb;
      if(!snd_contig)
      {
        snd_noncontig_buff = MPIU_Malloc(send_size * size);
        sbuf = snd_noncontig_buff;
        if(snd_noncontig_buff == NULL)
        {
          MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                     "Fatal:  Cannot allocate pack buffer");
        }
        DLOOP_Offset last = send_size * size;
        MPID_Segment_init(sendbuf, sendcount * size, sendtype, &segment, 0);
        MPID_Segment_pack(&segment, 0, &last, snd_noncontig_buff);
      }
    }
    else
      success = 0;

    if(success && recvbuf != MPI_IN_PLACE)
    {
      if(recv_size)
      {
        rbuf = (char *)recvbuf + rcv_true_lb;
        if(!rcv_contig)
        {
          rcv_noncontig_buff = MPIU_Malloc(recv_size);
          rbuf = rcv_noncontig_buff;
          if(rcv_noncontig_buff == NULL)
          {
            MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                       "Fatal:  Cannot allocate pack buffer");
          }
        }
      }
      else success = 0;
    }
  }

  else
  {
    if(recv_size)/* Should this be send or recv? */
    {
      rbuf = (char *)recvbuf + rcv_true_lb;
      if(!rcv_contig)
      {
        rcv_noncontig_buff = MPIU_Malloc(recv_size);
        rbuf = rcv_noncontig_buff;
        if(rcv_noncontig_buff == NULL)
        {
          MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                     "Fatal:  Cannot allocate pack buffer");
        }
      }
    }
    else
      success = 0;
  }

  if(!success)
  {
    MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr, mpierrno);
  }

  pami_xfer_t scatter;
  MPIDI_Post_coll_t scatter_post;
  const pami_metadata_t *my_scatter_md;
  volatile unsigned scatter_active = 1;


  scatter.algorithm = mpid->algorithm_list[PAMI_XFER_SCATTER][0][0];
  my_scatter_md = &mpid->algorithm_metadata_list[PAMI_XFER_SCATTER][0][0];

  scatter.cb_done = cb_scatter;
  scatter.cookie = (void *)&scatter_active;
  scatter.cmd.xfer_scatter.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
  scatter.cmd.xfer_scatter.sndbuf = (void *)sbuf;
  scatter.cmd.xfer_scatter.stype = PAMI_TYPE_BYTE;
  scatter.cmd.xfer_scatter.stypecount = send_size;
  scatter.cmd.xfer_scatter.rcvbuf = (void *)rbuf;
  scatter.cmd.xfer_scatter.rtype = PAMI_TYPE_BYTE;/* rtype is ignored when rcvbuf == PAMI_IN_PLACE */
  scatter.cmd.xfer_scatter.rtypecount = recv_size;

  if(recvbuf == MPI_IN_PLACE)
  {
    scatter.cmd.xfer_scatter.rcvbuf = PAMI_IN_PLACE;
  }


  TRACE_ERR("%s scatter\n", MPIDI_Process.context_post.active>0?"Posting":"Invoking");
  MPIDI_Context_post(MPIDI_Context[0], &scatter_post.state,
                     MPIDI_Pami_post_wrapper, (void *)&scatter);
  TRACE_ERR("Waiting on active %d\n", scatter_active);
  MPID_PROGRESS_WAIT_WHILE(scatter_active);

  if(!rcv_contig && rcv_noncontig_buff)
  {
    MPIR_Localcopy(rcv_noncontig_buff, recv_size, MPI_CHAR,
                   recvbuf,         recvcount,     recvtype);
    MPIU_Free(rcv_noncontig_buff);
  }
  if(!snd_contig)  MPIU_Free(snd_noncontig_buff);


  TRACE_ERR("Leaving MPIDO_Scatter_optimized\n");

  return MPI_SUCCESS;
}


int
MPIDO_CSWrapper_scatter(pami_xfer_t *scatter,
                        void        *comm)
{
  int mpierrno = 0;
  MPID_Comm   *comm_ptr = (MPID_Comm*)comm;
  MPI_Datatype sendtype, recvtype;
  void *rbuf;
  MPIDI_coll_check_in_place(scatter->cmd.xfer_scatter.rcvbuf, &rbuf);
  int rc = MPIDI_Dtpami_to_dtmpi(  scatter->cmd.xfer_scatter.stype,
                                   &sendtype,
                                   NULL,
                                   NULL);
  if(rc == -1) return rc;

  rc = MPIDI_Dtpami_to_dtmpi(  scatter->cmd.xfer_scatter.rtype,
                               &recvtype,
                               NULL,
                               NULL);
  if(rc == -1) return rc;

  rc  =  MPIR_Scatter(scatter->cmd.xfer_scatter.sndbuf,
                      scatter->cmd.xfer_scatter.stypecount, sendtype,
                      rbuf,
                      scatter->cmd.xfer_scatter.rtypecount, recvtype,
                      scatter->cmd.xfer_scatter.root, comm_ptr, &mpierrno);
  if(scatter->cb_done && rc == 0)
    scatter->cb_done(NULL, scatter->cookie, PAMI_SUCCESS);
  return rc;

}
#endif

