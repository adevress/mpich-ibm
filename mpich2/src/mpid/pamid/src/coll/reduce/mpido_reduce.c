/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/gather/mpido_reduce.c
 * \brief ???
 */

//#define TRACE_ON
#include <mpidimpl.h>

static void reduce_cb_done(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_reduce enter, active: %u\n", (*active));
   (*active)--;
}

int MPIDO_Reduce(void *sendbuf, 
                 void *recvbuf, 
                 int count, 
                 MPI_Datatype datatype,
                 MPI_Op op, 
                 int root, 
                 MPID_Comm *comm_ptr, 
                 int *mpierrno)

{
   MPID_Datatype *dt_null = NULL;
   MPI_Aint true_lb = 0;
   int dt_contig, tsize;
   int mu;
   char *sbuf, *rbuf;
   pami_data_function pop;
   pami_type_t pdt;
   int rc;
   int alg_selected = 0;

   rc = MPIDI_Datatype_to_pami(datatype, &pdt, op, &pop, &mu);
   if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
      fprintf(stderr,"reduce - rc %u, dt: %p, op: %p, mu: %u, selectedvar %u != %u (MPICH)\n",
         rc, pdt, pop, mu, 
         (unsigned)comm_ptr->mpid.user_selectedvar[PAMI_XFER_REDUCE], MPID_COLL_USE_MPICH);


   pami_xfer_t reduce;
   volatile unsigned reduce_active = 1;

   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_REDUCE] == MPID_COLL_USE_MPICH || rc != MPI_SUCCESS)
   {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && comm_ptr->rank == 0)
         fprintf(stderr,"Using MPICH reduce algorithm\n");
      return MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm_ptr, mpierrno);
   }

   MPIDI_Datatype_get_info(count, datatype, dt_contig, tsize, dt_null, true_lb);
   rbuf = recvbuf + true_lb;
   if(sendbuf == MPI_IN_PLACE) 
   {
      sbuf = rbuf;
   }
   else
      sbuf = sendbuf + true_lb;

   reduce.cb_done = reduce_cb_done;
   reduce.cookie = (void *)&reduce_active;
   reduce.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_REDUCE];
   reduce.cmd.xfer_reduce.sndbuf = sbuf;
   reduce.cmd.xfer_reduce.rcvbuf = rbuf;
   reduce.cmd.xfer_reduce.stype = pdt;
   reduce.cmd.xfer_reduce.rtype = pdt;
   reduce.cmd.xfer_reduce.stypecount = count;
   reduce.cmd.xfer_reduce.rtypecount = count;
   reduce.cmd.xfer_reduce.op = pop;
   reduce.cmd.xfer_reduce.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);


   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_REDUCE] == MPID_COLL_ALWAYS_QUERY ||
      comm_ptr->mpid.user_selectedvar[PAMI_XFER_REDUCE] == MPID_COLL_CHECK_FN_REQUIRED)
   {
      if(comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].check_fn != NULL)
      {
         metadata_result_t result = {0};
         TRACE_ERR("Querying reduce protocol %s, type was %d\n",
            comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].name,
            comm_ptr->mpid.user_selectedvar[PAMI_XFER_REDUCE]);
         result = comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].check_fn(&reduce);
         TRACE_ERR("Bitmask: %#X\n", result.bitmask);
         if(!result.bitmask)
         {
            fprintf(stderr,"Query failed for %s.\n",
               comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].name);
         }
         else alg_selected = 1;
      }
      else
      {
         /* No check function, but check required */
         /* look at meta data */
         /* assert(0);*/
      }
   }

   if(alg_selected)
   {
      if(MPIDI_Process.context_post)
      {
         TRACE_ERR("Posting reduce, context %d, algoname: %s, exflag: %d\n", 0,
            comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].name, exflag);
         MPIDI_Update_last_algorithm(comm_ptr,
            comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].name);
         MPIDI_Post_coll_t reduce_post;
         reduce_post.coll_struct = &reduce;
         rc = PAMI_Context_post(MPIDI_Context[0], &reduce_post.state, MPIDI_Pami_post_wrapper, (void *)&reduce_post);
         TRACE_ERR("Reduce posted, rc: %d\n", rc);
      }
      else
      {
         TRACE_ERR("Calling PAMI_Collective with reduce structure\n");
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && comm_ptr->rank == 0)
            fprintf(stderr,"Using protocol %s for reduce\n", comm_ptr->mpid.user_metadata[PAMI_XFER_REDUCE].name);
         rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&reduce);
      }
   }
   else
   {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && comm_ptr->rank == 0)
         fprintf(stderr,"Using MPICH reduce algorithm\n");
      return MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm_ptr, mpierrno);
   }

   MPID_PROGRESS_WAIT_WHILE(reduce_active);
   TRACE_ERR("Reduce done\n");
   return rc;
}
