/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/onesided/mpid_1s_unimpl.c
 * \brief Delete this file when the fuctions are all implemented
 */
#include <assert.h>

int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPID_Accumulate()   __attribute__((alias("MPID_Dummy")));
int MPID_Get()          __attribute__((alias("MPID_Dummy")));
int MPID_Put()          __attribute__((alias("MPID_Dummy")));
int MPID_Win_complete() __attribute__((alias("MPID_Dummy")));
int MPID_Win_create()   __attribute__((alias("MPID_Dummy")));
int MPID_Win_fence()    __attribute__((alias("MPID_Dummy")));
int MPID_Win_free()     __attribute__((alias("MPID_Dummy")));
int MPID_Win_lock()     __attribute__((alias("MPID_Dummy")));
int MPID_Win_post()     __attribute__((alias("MPID_Dummy")));
int MPID_Win_start()    __attribute__((alias("MPID_Dummy")));
int MPID_Win_test()     __attribute__((alias("MPID_Dummy")));
int MPID_Win_unlock()   __attribute__((alias("MPID_Dummy")));
int MPID_Win_wait()     __attribute__((alias("MPID_Dummy")));
