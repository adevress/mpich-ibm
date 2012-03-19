/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/gather/mpido_scan.c
 * \brief ???
 */

//#define TRACE_ON
#include <mpidimpl.h>

static void scan_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_scan enter, active: %u\n", (*active));
   (*active)--;
}
int MPIDO_Doscan(void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno, int exflag);


int MPIDO_Scan(void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno)
{
   return MPIDO_Doscan(sendbuf, recvbuf, count, datatype,
                op, comm_ptr, mpierrno, 0);
}

   
int MPIDO_Exscan(void *sendbuf, void *recvbuf, 
               int count, MPI_Datatype datatype,
               MPI_Op op, MPID_Comm * comm_ptr, int *mpierrno)
{
   return MPIDO_Doscan(sendbuf, recvbuf, count, datatype,
                op, comm_ptr, mpierrno, 1);
}

int MPIDO_Doscan(void *sendbuf, void *recvbuf, 
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

   rc = MPIDI_Datatype_to_pami(datatype, &pdt, op, &pop, &mu);
   if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
      fprintf(stderr,"rc %u, dt: %p, op: %p, mu: %u, selectedvar %u != %u (MPICH)\n",
         rc, pdt, pop, mu, 
         (unsigned)comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCAN], MPID_COLL_USE_MPICH);


   pami_xfer_t scan;
   volatile unsigned scan_active = 1;

   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCAN] == MPID_COLL_USE_MPICH || rc != MPI_SUCCESS)
      
   {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && comm_ptr->rank == 0)
         fprintf(stderr,"Using MPICH scan algorithm\n");
      if(exflag)
         return MPIR_Exscan(sendbuf, recvbuf, count, datatype, op, comm_ptr, mpierrno);
      else
         return MPIR_Scan(sendbuf, recvbuf, count, datatype, op, comm_ptr, mpierrno);
   }

   MPIDI_Datatype_get_info(count, datatype, dt_contig, tsize, dt_null, true_lb);
   rbuf = recvbuf + true_lb;
   if(sendbuf == MPI_IN_PLACE) 
   {
      sbuf = rbuf;
   }
   else
      sbuf = sendbuf + true_lb;

   scan.cb_done = scan_cb_done;
   scan.cookie = (void *)&scan_active;
   scan.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_SCAN];
   scan.cmd.xfer_scan.sndbuf = sbuf;
   scan.cmd.xfer_scan.rcvbuf = rbuf;
   scan.cmd.xfer_scan.stype = pdt;
   scan.cmd.xfer_scan.rtype = pdt;
   scan.cmd.xfer_scan.stypecount = count;
   scan.cmd.xfer_scan.rtypecount = count;
   scan.cmd.xfer_scan.op = pop;
   scan.cmd.xfer_scan.exclusive = exflag;


   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCAN] == MPID_COLL_ALWAYS_QUERY ||
      comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCAN] == MPID_COLL_CHECK_FN_REQUIRED)
   {
      metadata_result_t result = {0};
      TRACE_ERR("Querying scan protocol %s, type was %d\n",
         comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].name,
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_SCAN]);
      result = comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].check_fn(&scan);
      TRACE_ERR("Bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"Query failed for %s.\n",
            comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].name);
      }
   }
   
   if(MPIDI_Process.context_post)
   {
      TRACE_ERR("Posting scan, context %d, algoname: %s, exflag: %d\n", 0,
         comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].name, exflag);
      MPIDI_Update_last_algorithm(comm_ptr,
         comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].name);
      MPIDI_Post_coll_t scan_post;
      scan_post.coll_struct = &scan;
      rc = PAMI_Context_post(MPIDI_Context[0], &scan_post.state, MPIDI_Pami_post_wrapper, (void *)&scan_post);
      TRACE_ERR("Scan posted, rc: %d\n", rc);
   }
   else
   {
      TRACE_ERR("Calling PAMI_Collective with scan structure\n");
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL)
         fprintf(stderr,"Using protocol %s for scan\n", comm_ptr->mpid.user_metadata[PAMI_XFER_SCAN].name);
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&scan);
   }

   MPID_PROGRESS_WAIT_WHILE(scan_active);
   TRACE_ERR("Scan done\n");
   return rc;
}
