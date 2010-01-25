#include <assert.h>
int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPID_Irecv() __attribute__((alias("MPID_Dummy")));
int MPID_Isend() __attribute__((alias("MPID_Dummy")));
int MPID_Recv() __attribute__((alias("MPID_Dummy")));
int MPID_Request_create() __attribute__((alias("MPID_Dummy")));
int MPID_Request_release() __attribute__((alias("MPID_Dummy")));
int MPID_Send() __attribute__((alias("MPID_Dummy")));
int MPID_Ssend() __attribute__((alias("MPID_Dummy")));
