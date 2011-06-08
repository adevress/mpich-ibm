/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_progress.c
 * \brief Maintain the state and rules of the MPI progress semantics
 */
#include <mpidimpl.h>


void
MPIDI_Progress_init()
{
#ifdef USE_PAMI_COMM_THREADS
  if (MPIDI_Process.commthreads_enabled)
    {
      TRACE_ERR("Async advance beginning...\n");

      MPIDI_Process.commthreads_active = 1;
      unsigned i;
      for (i=0; i<MPIDI_Process.avail_contexts; ++i) {
        PAMIX_progress_register(MPIDI_Context[i], NULL, MPIDI_Progress_async_end, MPIDI_Progress_async_start, NULL);
        PAMIX_progress_enable(MPIDI_Context[i], PAMIX_PROGRESS_ALL);
      }

      TRACE_ERR("Async advance enabled\n");
    }
#endif
}


void
MPIDI_Progress_async_start(pami_context_t context, void *cookie)
{
  MPIDI_Process.commthreads_active = 1;
}


void
MPIDI_Progress_async_end  (pami_context_t context, void *cookie)
{
  MPIDI_Process.commthreads_active = 0;
}
