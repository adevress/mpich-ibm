/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_probe.c
 * \brief ???
 */
#include <mpidimpl.h>

int
MPID_Probe(int source,
           int tag,
           MPID_Comm * comm,
           int context_offset,
           MPI_Status * status)
{
  const int context = comm->recvcontext_id + context_offset;

  if (source == MPI_PROC_NULL)
    {
        MPIR_Status_set_procnull(status);
        return MPI_SUCCESS;
    }
  MPID_PROGRESS_WAIT_WHILE(!MPIDI_Recvq_FU(source, tag, context, status));
  return MPI_SUCCESS;
}
