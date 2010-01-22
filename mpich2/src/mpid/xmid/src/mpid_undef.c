#include <assert.h>
int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPID_Dims_create() __attribute__((alias("MPID_Dummy")));
int MPID_Finalize() __attribute__((alias("MPID_Dummy")));
int MPID_Get_processor_name() __attribute__((alias("MPID_Dummy")));
int MPIDI_Comm_create() __attribute__((alias("MPID_Dummy")));
int MPIDI_Comm_destroy() __attribute__((alias("MPID_Dummy")));
int MPID_Irecv() __attribute__((alias("MPID_Dummy")));
int MPID_Isend() __attribute__((alias("MPID_Dummy")));
int MPID_Progress_end() __attribute__((alias("MPID_Dummy")));
int MPID_Progress_start() __attribute__((alias("MPID_Dummy")));
int MPID_Progress_wait() __attribute__((alias("MPID_Dummy")));
int MPID_Recv() __attribute__((alias("MPID_Dummy")));
int MPID_Request_create() __attribute__((alias("MPID_Dummy")));
int MPID_Request_release() __attribute__((alias("MPID_Dummy")));
int MPID_Send() __attribute__((alias("MPID_Dummy")));
int MPID_Ssend() __attribute__((alias("MPID_Dummy")));
int MPIDU_dtc_free() __attribute__((alias("MPID_Dummy")));
int MPID_Wtime() __attribute__((alias("MPID_Dummy")));
int MPID_Wtime_init() __attribute__((alias("MPID_Dummy")));
int MPID_Wtime_todouble() __attribute__((alias("MPID_Dummy")));
