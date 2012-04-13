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
#else
#ifdef MPIDI_SINGLE_CONTEXT_ASYNC_PROGRESS
  if (MPIDI_Process.async_progress_enabled)
    {
      PAMIX_Progress_register(MPIDI_Context[0],MPIDI_Progress_async_poll,NULL,NULL,NULL);
      PAMIX_Progress_enable(MPIDI_Context[0], PAMIX_PROGRESS_ALL);
    }
#endif
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

void
MPIDI_Progress_async_poll()
{
  int rc, lock_held;
  int loop_count=100;

  lock_held = MPIDI_Mutex_try_acquire(0);

  if (lock_held == 0)
    {
      rc = PAMI_Context_advancev(MPIDI_Context, MPIDI_Process.avail_contexts, loop_count);
      MPID_assert( (rc == PAMI_SUCCESS) || (rc == PAMI_EAGAIN) );
      if (rc == PAMI_EAGAIN)
        {
          MPIDI_CS_SCHED_YIELD(0);
        }
      else
        MPIU_THREAD_CS_YIELD(ALLFUNC,);
      MPIDI_Mutex_release(0);
    }
  else
    return;
}

