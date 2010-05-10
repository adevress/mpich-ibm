#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) //fprintf x

#if 0
static void cb_allreduce(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR((stderr,"callback enter, active: %d\n", (*active)));
   (*active)--;
}
#endif

int MPIDO_Allreduce(void *sendbuf,
                    void *recvbuf,
                    int count,
                    MPI_Datatype datatype,
                    MPI_Op op,
                    MPID_Comm *comm_ptr)
{
   TRACE_ERR((stderr,"in mpido_allreduce\n"));
   return MPIR_Allreduce(sendbuf, recvbuf, count, datatype, op, comm_ptr);
}
