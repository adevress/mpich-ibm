/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/misc/mpid_finalize.c
 * \brief ???
 */
#include "mpidimpl.h"

void MPIDU_dtc_free(MPID_Datatype *dt)
{
}
void MPIDI_Comm_create(MPID_Comm *comm)
{
}
void MPIDI_Comm_destroy(MPID_Comm *comm)
{
}



int MPID_Dummy()
{
  assert(0);
  return 1;
}
int MPID_Win_create()  __attribute__((alias("MPID_Dummy")));
int MPID_Win_free()    __attribute__((alias("MPID_Dummy")));
