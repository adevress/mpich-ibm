/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/allreduce/mpido_allreduce.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void cb_allreduce(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR("callback enter, active: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}

int MPIDO_Allreduce(void *sendbuf,
                    void *recvbuf,
                    int count,
                    MPI_Datatype dt,
                    MPI_Op op,
                    MPID_Comm *comm_ptr,
                    int *mpierrno)
{
  if (sendbuf == MPI_IN_PLACE)
    return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);

   TRACE_ERR("in mpido_allreduce\n");
   MPIDI_Post_coll_t allred_post;
   pami_type_t pdt;
   pami_data_function pop;
   int mu;
   int rc;
   /* int len; */
   /* char op_str[255]; */
   /* char dt_str[255]; */
   volatile unsigned active = 1;
   pami_xfer_t allred;
   /* MPIopString(op, op_str); */
   /* PMPI_Type_get_name(dt, dt_str, &len); */
   rc = MPItoPAMI(dt, &pdt, op, &pop, &mu);
   if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
      fprintf(stderr,"rc %u, Datatype %p, op %p, mu %u, selectedvar %u != %u\n",
              rc, pdt, pop, mu, 
              (unsigned)comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE],MPID_COLL_USE_MPICH);
   /*fprintf(stderr,"type %u >= %u, fn %p\n",
           comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE],
           MPID_COLL_QUERY,
           comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].check_fn);
   */
      /* convert to metadata query */
   if(rc == MPI_SUCCESS && mu == 1 && comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] != MPID_COLL_USE_MPICH)
   {
      allred.cb_done = cb_allreduce;
      allred.cookie = (void *)&active;
      allred.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_ALLREDUCE];
      allred.cmd.xfer_allreduce.sndbuf = sendbuf;
      allred.cmd.xfer_allreduce.stype = pdt;
      allred.cmd.xfer_allreduce.rcvbuf = recvbuf;
      allred.cmd.xfer_allreduce.rtype = pdt;
      allred.cmd.xfer_allreduce.stypecount = count;
      allred.cmd.xfer_allreduce.rtypecount = count;
      allred.cmd.xfer_allreduce.op = pop;
      if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] >= MPID_COLL_QUERY)
      {
         if(comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].check_fn != NULL)
         {
            /* For now, we don't distinguish between MPID_COLL_ALWAYS_QUERY &
               MPID_COLL_CHECK_FN_REQUIRED, we just call the fn                */
            metadata_result_t result = {0};
            TRACE_ERR("querying allreduce algorithm %s, typewwas %d\n",
               comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name,
               comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE]);
            result = comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].check_fn(&allred);
            TRACE_ERR("bitmask: %#X\n", result.bitmask);
            if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
               fprintf(stderr,"check_fn result %#X\n",result.bitmask);
            /* #warning THIS NEEDS CHANGED BACK. 
             * Why does the protocol require nonlocal knowledge in the first place?
             */
            if(!result.bitmask)
            {
               if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
                  fprintf(stderr,"check_fn failed for %s.\n",
                          comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name);
               MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
               return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
            }
         }
         else /* no check_fn, manually look at the metadata fields */
         {
            /* Check if the message range if restricted */
            if(comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].check_correct.values.rangeminmax)
            {
               MPI_Aint data_true_lb;
               MPID_Datatype *data_ptr;
               int data_size, data_contig;
               MPIDI_Datatype_get_info(count, dt, data_contig, data_size, data_ptr, data_true_lb); 

               if((comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].range_lo <= data_size) &&
                  (comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].range_hi >= data_size))
                  ;
               else
               {
                  if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
                     fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                             data_size,
                             comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].range_lo,
                             comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].range_hi,
                             comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name);
                  MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
                  return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
               }
            }
            /* \todo check the rest of the metadata */
         }
      }
      /* MPIDI_Update_last_algorithm(comm_ptr, comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name); */
      if(MPIDI_Process.context_post)
      {
         allred_post.coll_struct = &allred;
         if((MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0) ||
            (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL))
            fprintf(stderr,"Using protocol %s, type %u.\n",
                    comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name,
                    comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE]);
         TRACE_ERR("posting allreduce, context: %d, algoname: %s, dt: %s, op: %s, count: %d\n", 0,
                   comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name, dt_str, op_str, count);
         rc = PAMI_Context_post(MPIDI_Context[0], &allred_post.state, 
                                MPIDI_Pami_post_wrapper, (void *)&allred_post);
         TRACE_ERR("allreduce posted, rc: %d\n", rc);
      }
      else
      {
         if((MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0) ||
            (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL))
            fprintf(stderr,"Using protocol %s, type %u.\n",
                    comm_ptr->mpid.user_metadata[PAMI_XFER_ALLREDUCE].name,
                    comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE]);
         rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&allred);
      }

      MPID_assert_always(rc == PAMI_SUCCESS);
      MPID_PROGRESS_WAIT_WHILE(active);
      TRACE_ERR("allreduce done\n");
      return MPI_SUCCESS;
   }
   else
   {
      if((MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0) ||
         (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL))
         fprintf(stderr,"Using protocol %s, type %u.\n",
                 "ALLREDUCE_MPICH",
                 comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE]);
      MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
      return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
   }
}
