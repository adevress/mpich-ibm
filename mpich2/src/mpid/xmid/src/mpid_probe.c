/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/misc/mpid_probe.c
 * \brief ???
 */
#include "mpidimpl.h"

int
MPID_Probe(int source,
           int tag,
           MPID_Comm * comm,
           int context_offset,
           MPI_Status * status)
{
  int found;
  MPID_Progress_state state;
  const int context = comm->recvcontext_id + context_offset;

  if (source == MPI_PROC_NULL)
    {
        MPIR_Status_set_procnull(status);
        return MPI_SUCCESS;
    }
  for(;;)
    {
      found = MPIDI_Recvq_FU(source, tag, context, status);
      if (found)
        return MPI_SUCCESS;
      MPID_Progress_wait(&state);
    }
  return MPI_SUCCESS;
}
