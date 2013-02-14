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
 * \file src/coll/alltoall/mpido_alltoall.c
 * \brief ???
 */

/* #define TRACE_ON */

#include <mpidimpl.h>

static void cb_alltoall(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *)clientdata;
   TRACE_ERR("alltoall callback enter, active: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}


int MPIDO_Alltoall(const void *sendbuf,
                   int sendcount,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   MPID_Comm *comm_ptr,
                   int *mpierrno)
{
   TRACE_ERR("Entering MPIDO_Alltoall\n");
   volatile unsigned active = 1;
   MPID_Datatype *sdt, *rdt;
   pami_type_t stype, rtype;
   MPI_Aint sdt_true_lb=0, rdt_true_lb;
   MPIDI_Post_coll_t alltoall_post;
   int sndlen, rcvlen, snd_contig, rcv_contig, pamidt=1;
   int tmp;
#if ASSERT_LEVEL==0
   /* We can't afford the tracing in ndebug/performance libraries */
    const unsigned verbose = 0;
#else
    const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && (comm_ptr->rank == 0);
#endif
   const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
   const int selected_type = mpid->user_selected_type[PAMI_XFER_ALLTOALL];

   if(sendbuf != MPI_IN_PLACE)
   {
      MPIDI_Datatype_get_info(1, sendtype, snd_contig, sndlen, sdt, sdt_true_lb);
      if(!snd_contig) pamidt = 0;
   }
   MPIDI_Datatype_get_info(1, recvtype, rcv_contig, rcvlen, rdt, rdt_true_lb);
   if(!rcv_contig) pamidt = 0;

   /* Alltoall is much simpler if bytes are required because we don't need to
    * malloc displ/count arrays and copy things
    */


   /* Is it a built in type? If not, send to MPICH */
   if(MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;

   if((selected_type == MPID_COLL_USE_MPICH) ||
       pamidt == 0)
   {
      if(unlikely(verbose))
         fprintf(stderr,"Using MPICH alltoall algorithm\n");
      return MPIR_Alltoall_intra(sendbuf, sendcount, sendtype,
                      recvbuf, recvcount, recvtype,
                      comm_ptr, mpierrno);

   }

   pami_xfer_t alltoall;
   pami_algorithm_t my_alltoall;
   const pami_metadata_t *my_md = (pami_metadata_t *)NULL;
   int queryreq = 0;
   if(selected_type == MPID_COLL_OPTIMIZED)
   {
      TRACE_ERR("Optimized alltoall was pre-selected\n");
      my_alltoall = mpid->opt_protocol[PAMI_XFER_ALLTOALL][0];
      my_md = &mpid->opt_protocol_md[PAMI_XFER_ALLTOALL][0];
      queryreq = mpid->must_query[PAMI_XFER_ALLTOALL][0];
   }
   else
   {
      TRACE_ERR("Alltoall was specified by user\n");
      my_alltoall = mpid->user_selected[PAMI_XFER_ALLTOALL];
      my_md = &mpid->user_metadata[PAMI_XFER_ALLTOALL];
      queryreq = selected_type;
   }
   char *pname = my_md->name;
   TRACE_ERR("Using alltoall protocol %s\n", pname);

   alltoall.cb_done = cb_alltoall;
   alltoall.cookie = (void *)&active;
   alltoall.algorithm = my_alltoall;
   if(sendbuf == MPI_IN_PLACE)
   {
      if(unlikely(verbose))
         fprintf(stderr,"alltoall MPI_IN_PLACE buffering\n");
      alltoall.cmd.xfer_alltoall.stype = rtype;
      alltoall.cmd.xfer_alltoall.stypecount = recvcount;
      alltoall.cmd.xfer_alltoall.sndbuf = PAMI_IN_PLACE;
   }
   else
   {
      alltoall.cmd.xfer_alltoall.stype = stype;
      alltoall.cmd.xfer_alltoall.stypecount = sendcount;
      alltoall.cmd.xfer_alltoall.sndbuf = (char *)sendbuf + sdt_true_lb;
   }
   alltoall.cmd.xfer_alltoall.rcvbuf = (char *)recvbuf + rdt_true_lb;

   alltoall.cmd.xfer_alltoall.rtypecount = recvcount;
   alltoall.cmd.xfer_alltoall.rtype = rtype;

   if(unlikely(queryreq == MPID_COLL_ALWAYS_QUERY || 
               queryreq == MPID_COLL_CHECK_FN_REQUIRED))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying alltoall protocol %s, query level was %d\n", pname,
         queryreq);
      if(my_md->check_fn == NULL)
      {
        /* process metadata bits */
         if((!my_md->check_correct.values.inplace) && (sendbuf == MPI_IN_PLACE))
            result.check.unspecified = 1;
         MPI_Aint data_true_lb;
         MPID_Datatype *data_ptr;
         int data_size, data_contig;
         MPIDI_Datatype_get_info(sendcount, sendtype, data_contig, data_size, data_ptr, data_true_lb); 
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
      else /* calling the check fn is sufficient */
         result = my_md->check_fn(&alltoall);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
      if(result.bitmask)
      {
        if(unlikely(verbose))
           fprintf(stderr,"Query failed for %s. Using MPICH alltoall.\n", pname);
        MPIDI_Update_last_algorithm(comm_ptr, "ALLTOALL_MPICH");
        return MPIR_Alltoall_intra(sendbuf, sendcount, sendtype,
                                   recvbuf, recvcount, recvtype,
                                   comm_ptr, mpierrno);
      }
      if(my_md->check_correct.values.asyncflowctl) 
      { /* need better flow control than a barrier every time */
         int tmpmpierrno;   
         if(unlikely(verbose))
            fprintf(stderr,"Query barrier required for %s\n", my_md->name);
         MPIR_Barrier(comm_ptr, &tmpmpierrno);
      }
   }

   if(unlikely(verbose))
   {
      unsigned long long int threadID;
      MPIU_Thread_id_t tid;
      MPIU_Thread_self(&tid);
      threadID = (unsigned long long int)tid;
      fprintf(stderr,"<%llx> Using protocol %s for alltoall on %u\n", 
              threadID,
              my_md->name,
              (unsigned) comm_ptr->context_id);
   }

   MPIDI_Context_post(MPIDI_Context[0], &alltoall_post.state,
                      MPIDI_Pami_post_wrapper, (void *)&alltoall);

   TRACE_ERR("Waiting on active\n");
   MPID_PROGRESS_WAIT_WHILE(active);

   TRACE_ERR("Leaving alltoall\n");
  return PAMI_SUCCESS;
}
