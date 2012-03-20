/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_progress.c
 * \brief Maintain the state and rules of the MPI progress semantics
 */
#include <mpidimpl.h>


void
MPIDI_Progress_init()
{
#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
  if (MPIDI_Process.async_progress_enabled)
    {
      TRACE_ERR("Async advance beginning...\n");

      MPIDI_Process.async_progress_active = 1;
      unsigned i;
      for (i=0; i<MPIDI_Process.avail_contexts; ++i) {
        PAMIX_Progress_register(MPIDI_Context[i], NULL, MPIDI_Progress_async_end, MPIDI_Progress_async_start, NULL);
        PAMIX_Progress_enable(MPIDI_Context[i], PAMIX_PROGRESS_ALL);
      }

      TRACE_ERR("Async advance enabled\n");
    }
#endif
}


void
MPIDI_Progress_async_start(pami_context_t context, void *cookie)
{
  MPIDI_Process.async_progress_active = 1;
}


void
MPIDI_Progress_async_end  (pami_context_t context, void *cookie)
{
  MPIDI_Process.async_progress_active = 0;
}
