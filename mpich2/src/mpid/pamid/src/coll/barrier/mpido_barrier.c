/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/coll/barrier/mpido_barrier.c
 * \brief ???
 */


#include "mpidimpl.h"

static void cb_barrier(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR("callback. enter: %d\n", (*active));
   (*active)--;
}

int MPIDO_Barrier(MPID_Comm *comm_ptr)
{
   TRACE_ERR("in mpido_barrier\n");
   int rc;
   volatile unsigned active=1;
   pami_xfer_t barrier;
   barrier.cb_done = cb_barrier;
   barrier.cookie = (void *)&active;
   barrier.algorithm = comm_ptr->mpid.barriers[0];

   TRACE_ERR("posting barrier\n");
   MPIDI_Update_last_algorithm(comm_ptr, comm_ptr->mpid.barrier_metas[0].name);
   rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&barrier);
   TRACE_ERR("barrier posted rc: %d\n", rc);
   assert(rc == PAMI_SUCCESS);

   TRACE_ERR("advance spinning\n");
   while(active)
   {
      TRACE_ERR("active: %d\n", active);
      rc = PAMI_Context_advance(MPIDI_Context[0], 1);
   }
   TRACE_ERR("exiting mpido_barrier\n");
   return rc;
}
