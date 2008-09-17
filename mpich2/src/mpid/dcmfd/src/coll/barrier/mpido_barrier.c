/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/barrier/mpido_barrier.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Barrier = MPIDO_Barrier

#ifdef USE_CCMI_COLL

/**
 * **************************************************************************
 * \brief General MPIDO_Barrier() implementation
 * **************************************************************************
 */

int MPIDO_Barrier(MPID_Comm * comm)
{
  int rc;
  DCMF_Embedded_Info_Set * properties = &(comm->dcmf.properties);
  
  if(DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_BARRIER))
    return MPIR_Barrier(comm);

  if (DCMF_INFO_ISSET(properties, DCMF_USE_GI_BARRIER))
    rc = MPIDO_Barrier_gi(comm);
  //else if (DCMF_INFO_ISSET(properties, DCMF_USE_RECT_BARRIER))
  //  rc = MPIDO_Barrier_rect(comm);
  else
    rc = MPIDO_Barrier_dcmf(comm);

  if (rc != DCMF_SUCCESS)
    rc = MPIR_Barrier(comm);
  
  return rc;
}

#else /* !USE_CCMI_COLL */

int MPIDO_Barrier(MPID_Comm *comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
