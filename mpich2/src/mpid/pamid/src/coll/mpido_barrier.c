#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) //fprintf x

static void cb_barrier(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR((stderr,"callback. enter: %d\n", (*active)));
   (*active)--;
}

int MPIDO_Barrier(MPID_Comm *comm_ptr)
{
   TRACE_ERR((stderr,"in mpido_barrier\n"));
   int rc;
   volatile unsigned active=1;
   pami_xfer_t barrier;
   barrier.cb_done = cb_barrier;
   barrier.cookie = (void *)&active;
   barrier.algorithm = comm_ptr->mpid.barriers[0];

   TRACE_ERR((stderr,"posting barrier\n"));
   rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&barrier);
   TRACE_ERR((stderr,"barrier posted rc: %d\n", rc));
   assert(rc == PAMI_SUCCESS);

   TRACE_ERR((stderr,"advance spinning\n"));
   while(active)
   {
      TRACE_ERR((stderr,"active: %d\n", active));
      rc = PAMI_Context_advance(MPIDI_Context[0], 1);
   }
   TRACE_ERR((stderr,"exiting mpido_barrier\n"));
   return rc;
}
