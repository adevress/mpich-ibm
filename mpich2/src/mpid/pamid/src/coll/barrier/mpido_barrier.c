/*  (C)Copyright IBM Corp.  2007, 2010  */
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

int MPIDO_Barrier(MPID_Comm *comm_ptr)
{
   TRACE_ERR("in mpido_barrier\n");
   int rc;
   volatile unsigned active=1;
   MPIDI_Post_coll_t barrier_post;
   pami_xfer_t barrier;

   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] == 0)
      return MPIR_Barrier(comm_ptr);

   barrier.cb_done = cb_barrier;
   barrier.cookie = (void *)&active;
   barrier.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_BARRIER];

   /* TODO Name needs fixed somehow */
   MPIDI_Update_last_algorithm(comm_ptr, comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][0].name);
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
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&barrier);
   }
   assert(rc == PAMI_SUCCESS);

   TRACE_ERR("advance spinning\n");
   MPID_PROGRESS_WAIT_WHILE(active);
   TRACE_ERR("exiting mpido_barrier\n");
   return rc;
}
