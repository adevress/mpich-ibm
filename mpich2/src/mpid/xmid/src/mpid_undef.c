#include <assert.h>
int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPIDI_Buffer_copy()      __attribute__((alias("MPID_Dummy")));
int MPID_Request_complete()  __attribute__((alias("MPID_Dummy")));
int MPID_Request_create()    __attribute__((alias("MPID_Dummy")));
int MPID_Request_release()   __attribute__((alias("MPID_Dummy")));
