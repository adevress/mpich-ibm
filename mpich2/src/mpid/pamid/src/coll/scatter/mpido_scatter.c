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

   /* TODO: Needs to be a PAMI bcast */
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
   const int selected_type = mpid->user_selected_type[PAMI_XFER_SCATTER];
   char use_pami = !(selected_type == MPID_COLL_USE_MPICH);

  /* if (rank == root)
     We can't decide on just the root to use MPICH. Really need a pre-allreduce.
     For now check sendtype on non-roots too and hope it matches? I think that's what
     scatterv does... */
  {
    if(MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      use_pami = 0;
  }
  if(MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
    use_pami = 0;

  if(!use_pami)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH scatter algorithm\n");
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


   pami_xfer_t scatter;
   MPIDI_Post_coll_t scatter_post;
   pami_algorithm_t my_scatter;
   const pami_metadata_t *my_scatter_md;
   volatile unsigned scatter_active = 1;
   int queryreq = 0;

   if(selected_type == MPID_COLL_OPTIMIZED)
   {
      TRACE_ERR("Optimized scatter %s was selected\n",
         mpid->opt_protocol_md[PAMI_XFER_SCATTER][0].name);
      my_scatter = mpid->opt_protocol[PAMI_XFER_SCATTER][0];
      my_scatter_md = &mpid->opt_protocol_md[PAMI_XFER_SCATTER][0];
      queryreq = mpid->must_query[PAMI_XFER_SCATTER][0];
   }
   else
   {
      TRACE_ERR("Optimized scatter %s was set by user\n",
         mpid->user_metadata[PAMI_XFER_SCATTER].name);
      my_scatter = mpid->user_selected[PAMI_XFER_SCATTER];
      my_scatter_md = &mpid->user_metadata[PAMI_XFER_SCATTER];
      queryreq = selected_type;
   }
 
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
     scatter.cmd.xfer_scatter.rcvbuf = (char *)sendbuf + nbytes*rank;
     scatter.cmd.xfer_scatter.rtype = stype;
     scatter.cmd.xfer_scatter.rtypecount = sendcount;
   }
   else
   {
     scatter.cmd.xfer_scatter.rcvbuf = (void *)recvbuf;
     scatter.cmd.xfer_scatter.rtype = rtype;
     scatter.cmd.xfer_scatter.rtypecount = recvcount;
   }

   if(unlikely(queryreq == MPID_COLL_ALWAYS_QUERY ||
               queryreq == MPID_COLL_CHECK_FN_REQUIRED))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying scatter protoocl %s, type was %d\n",
         my_scatter_md->name, queryreq);
      if(queryreq == MPID_COLL_ALWAYS_QUERY)
      {
        /* process metadata bits */
      }
      else /* (queryreq == MPID_COLL_CHECK_FN_REQUIRED - calling the check fn is sufficient */
        result = my_scatter_md->check_fn(&scatter);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
      if(result.bitmask)
      {
        if(unlikely(verbose))
          fprintf(stderr,"query failed for %s\n", my_scatter_md->name);
        MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH");
        return MPIR_Scatter(sendbuf, sendcount, sendtype,
                            recvbuf, recvcount, recvtype,
                            root, comm_ptr, mpierrno);
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
              my_scatter_md->name,
              (unsigned) comm_ptr->context_id);
   }
   TRACE_ERR("%s scatter\n", MPIDI_Process.context_post.active>0?"Posting":"Invoking");
   MPIDI_Context_post(MPIDI_Context[0], &scatter_post.state,
                      MPIDI_Pami_post_wrapper, (void *)&scatter);
   TRACE_ERR("Waiting on active %d\n", scatter_active);
   MPID_PROGRESS_WAIT_WHILE(scatter_active);


   TRACE_ERR("Leaving MPIDO_Scatter\n");

   return 0;
}



#if 0 /* old glue-based scatter-via-bcast */

  /* Assume glue protocol sucks for now.... Needs work if not or to enable */



  /* see if we all agree to use bcast scatter */
  MPIDO_Allreduce(MPI_IN_PLACE, &success, 1, MPI_INT, MPI_BAND, comm_ptr, mpierrno);

  if (!success)
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH scatter algorithm\n");
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr, mpierrno);
  }

   sbuf = (char *)sendbuf+true_lb;
   rbuf = (char *)recvbuf+true_lb;

   MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_OPT_BCAST");
   return MPIDO_Scatter_bcast(sbuf, sendcount, sendtype,
                              rbuf, recvcount, recvtype,
                              root, comm_ptr, mpierrno);
#endif