/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/barrier/mpido_barrier.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Barrier = MPIDO_Barrier

/**
 * **************************************************************************
 * \brief General MPIDO_Barrier() implementation
 * **************************************************************************
 */

int MPIDO_Barrier(MPID_Comm * comm)
{
  int rc;
  DCMF_Embedded_Info_Set * properties = &(comm -> dcmf.properties);

  if (DCMF_CHECK_INFO(properties, DCMF_GLOBAL_CONTEXT, DCMF_GI,DCMF_END_ARGS))
    rc = MPIDO_Barrier_gi(comm);
  else
    rc = MPIDO_Barrier_dcmf(comm);

  if (rc != DCMF_SUCCESS)
    rc = MPIR_Barrier(comm);
  
  return rc;
}
