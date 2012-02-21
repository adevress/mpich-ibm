/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/barrier/mpido_barrier.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void cb_barrier(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR("callback. enter: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}

int MPIDO_Barrier(MPID_Comm *comm_ptr, int *mpierrno)
{
   TRACE_ERR("Entering MPIDO_Barrier\n");
   int rc;
   volatile unsigned active=1;
   MPIDI_Post_coll_t barrier_post;
   pami_xfer_t barrier;
   pami_algorithm_t my_barrier;
   pami_metadata_t *my_barrier_md;
   int queryreq = 0;

   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] == MPID_COLL_USE_MPICH)
   {
      TRACE_ERR("Using MPICH Barrier\n");
      return MPIR_Barrier(comm_ptr, mpierrno);
   }

   barrier.cb_done = cb_barrier;
   barrier.cookie = (void *)&active;
#ifdef MPIDI_BASIC_COLLECTIVE_SELECTION
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] == MPID_COLL_SELECTED)
   {
      TRACE_ERR("Optimized barrier (%s) was pre-selected\n", comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BARRIER][0].name);
      my_barrier = comm_ptr->mpid.opt_protocol[PAMI_XFER_BARRIER][0];
      my_barrier_md = &comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BARRIER][0];
      queryreq = comm_ptr->mpid.must_query[PAMI_XFER_BARRIER][0];
   }
   else
#endif
   {
      TRACE_ERR("Barrier (%s) was specified by user\n", comm_ptr->mpid.user_metadata[PAMI_XFER_BARRIER].name);
      my_barrier = comm_ptr->mpid.user_selected[PAMI_XFER_BARRIER];
      my_barrier_md = &comm_ptr->mpid.user_metadata[PAMI_XFER_BARRIER];
      queryreq = comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER];
   }

   barrier.algorithm = my_barrier;
   /* There is no support for query-required barrier protocols here */
   MPID_assert_always(queryreq != MPID_COLL_ALWAYS_QUERY);
   MPID_assert_always(queryreq != MPID_COLL_CHECK_FN_REQUIRED);

   /* TODO Name needs fixed somehow */
   MPIDI_Update_last_algorithm(comm_ptr, my_barrier_md->name);
   if(MPIDI_Process.context_post)
   {

      TRACE_ERR("posting barrier\n");
      barrier_post.coll_struct = &barrier;
      rc = PAMI_Context_post(MPIDI_Context[0], &barrier_post.state, 
                              MPIDI_Pami_post_wrapper, (void *)&barrier_post);
      TRACE_ERR("barrier posted rc: %d\n", rc);
   }
   else
   {
     if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL)
       fprintf(stderr,"Using protocol %s.\n", my_barrier_md->name);
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&barrier);
   }
   MPID_assert_always(rc == PAMI_SUCCESS);

   TRACE_ERR("advance spinning\n");
   MPID_PROGRESS_WAIT_WHILE(active);
   TRACE_ERR("exiting mpido_barrier\n");
   return rc;
}
