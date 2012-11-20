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
 * \file src/coll/gather/mpido_scan.c
 * \brief ???
 */

/* #define TRACE_ON */
#include <mpidimpl.h>

static void scan_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_scan enter, active: %u\n", (*active));
   (*active)--;
}
int MPIDO_Doscan(const void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno, int exflag);


int MPIDO_Scan(const void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno)
{
   return MPIDO_Doscan(sendbuf, recvbuf, count, datatype,
                op, comm_ptr, mpierrno, 0);
}

int MPIDO_Iscan(const void *sendbuf, void *recvbuf,
                int count, MPI_Datatype datatype,
                MPI_Op op, MPID_Comm * comm_ptr, MPID_Request **request)
{
   /*if (unlikely((data_size == 0) || (user_selected_type == MPID_COLL_USE_MPICH)))*/
   {
      /*
       * If the mpich mpir non-blocking collectives are enabled, return without
       * first constructing the MPID_Request. This signals to the
       * MPIR_Iscan_impl() function to invoke the mpich nbc
       * implementation of MPI_Iscan().
       */
      if (MPIDI_Process.mpir_nbc != 0)
       return 0;

      /*
       * MPIR_* nbc implementation is not enabled. Fake a non-blocking
       * MPIR_Iscan() with a blocking MPIR_Scan().
       */
      if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && comm_ptr->rank == 0))
         fprintf(stderr,"Using MPICH blocking scan_algorithm\n");

      int mpierrno = 0;
      int rc = MPIR_Scan_impl(sendbuf, recvbuf, count, datatype,
                              op, comm_ptr, &mpierrno);

      /*
       * The blocking scan has completed - create and complete a
       * MPID_Request object so the MPIR_Iscan_impl() function
       * does not perform an additional iscan.
       */
      MPID_Request * mpid_request = MPID_Request_create_inline();
      mpid_request->kind = MPID_COLL_REQUEST;
      *request = mpid_request;
      MPIDI_Request_complete_norelease_inline(mpid_request);

      return rc;
   }

   return 0;
}
   
int MPIDO_Exscan(const void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno)
{
   return MPIDO_Doscan(sendbuf, recvbuf, count, datatype,
                op, comm_ptr, mpierrno, 1);
}

int MPIDO_Doscan(const void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno, int exflag)
{
   MPID_Datatype *dt_null = NULL;
   MPI_Aint true_lb = 0;
   int dt_contig, tsize;
   int mu;
   char *sbuf, *rbuf;
   pami_data_function pop;
   pami_type_t pdt;
   int rc;
   const pami_metadata_t *my_md;
#if ASSERT_LEVEL==0
   /* We can't afford the tracing in ndebug/performance libraries */
    const unsigned verbose = 0;
#else
    const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && (comm_ptr->rank == 0);
#endif
   const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
   const int selected_type = mpid->user_selected_type[PAMI_XFER_SCAN];

   rc = MPIDI_Datatype_to_pami(datatype, &pdt, op, &pop, &mu);
   if(unlikely(verbose))
      fprintf(stderr,"rc %u, dt: %p, op: %p, mu: %u, selectedvar %u != %u (MPICH)\n",
         rc, pdt, pop, mu, 
         (unsigned)selected_type, MPID_COLL_USE_MPICH);

   pami_xfer_t scan;
   volatile unsigned scan_active = 1;

   if((sendbuf == MPI_IN_PLACE) || /* Disable until ticket #627 is fixed */
      (selected_type == MPID_COLL_USE_MPICH || rc != MPI_SUCCESS))
   {
      if(unlikely(verbose))
         fprintf(stderr,"Using MPICH scan algorithm (exflag %d)\n",exflag);
      if(exflag)
         return MPIR_Exscan(sendbuf, recvbuf, count, datatype, op, comm_ptr, mpierrno);
      else
         return MPIR_Scan(sendbuf, recvbuf, count, datatype, op, comm_ptr, mpierrno);
   }

   MPIDI_Datatype_get_info(count, datatype, dt_contig, tsize, dt_null, true_lb);
   rbuf = (char *)recvbuf + true_lb;
   if(sendbuf == MPI_IN_PLACE) 
   {
      if(unlikely(verbose))
         fprintf(stderr,"scan MPI_IN_PLACE buffering\n");
      sbuf = rbuf;
   }
   else
   {
      sbuf = (char *)sendbuf + true_lb;
   }

   scan.cb_done = scan_cb_done;
   scan.cookie = (void *)&scan_active;
   if(selected_type == MPID_COLL_OPTIMIZED)
   {
      scan.algorithm = mpid->opt_protocol[PAMI_XFER_SCAN][0];
      my_md = &mpid->opt_protocol_md[PAMI_XFER_SCAN][0];
   }
   else
   {
      scan.algorithm = mpid->user_selected[PAMI_XFER_SCAN];
      my_md = &mpid->user_metadata[PAMI_XFER_SCAN];
   }
   scan.cmd.xfer_scan.sndbuf = sbuf;
   scan.cmd.xfer_scan.rcvbuf = rbuf;
   scan.cmd.xfer_scan.stype = pdt;
   scan.cmd.xfer_scan.rtype = pdt;
   scan.cmd.xfer_scan.stypecount = count;
   scan.cmd.xfer_scan.rtypecount = count;
   scan.cmd.xfer_scan.op = pop;
   scan.cmd.xfer_scan.exclusive = exflag;


   if(selected_type == MPID_COLL_ALWAYS_QUERY ||
      selected_type == MPID_COLL_CHECK_FN_REQUIRED)
   {
      metadata_result_t result = {0};
      TRACE_ERR("Querying scan protocol %s, type was %d\n",
         my_md->name,
         selected_type);
      result = my_md->check_fn(&scan);
      TRACE_ERR("Bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"Query failed for %s.\n",
            my_md->name);
      }
   }
   
   if(unlikely(verbose))
   {
      unsigned long long int threadID;
      MPIU_Thread_id_t tid;
      MPIU_Thread_self(&tid);
      threadID = (unsigned long long int)tid;
      fprintf(stderr,"<%llx> Using protocol %s for scan on %u (exflag %d)\n",
              threadID,
              my_md->name,
              (unsigned) comm_ptr->context_id,
              exflag);
   }
   MPIDI_Post_coll_t scan_post;
   MPIDI_Context_post(MPIDI_Context[0], &scan_post.state,
                      MPIDI_Pami_post_wrapper, (void *)&scan);
   TRACE_ERR("Scan %s\n", MPIDI_Process.context_post.active>0?"posted":"invoked");
   MPIDI_Update_last_algorithm(comm_ptr, my_md->name);
   MPID_PROGRESS_WAIT_WHILE(scan_active);
   TRACE_ERR("Scan done\n");
   return rc;
}
