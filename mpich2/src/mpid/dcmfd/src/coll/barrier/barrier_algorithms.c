/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/barrier/barrier_algorithms.c
 * \brief ???
 */

#include "mpido_coll.h"

#ifdef USE_CCMI_COLL

/**
 * **************************************************************************
 * \brief "Done" callback for barrier messages.
 * **************************************************************************
 */

static void
barrier_cb_done(void * clientdata, DCMF_Error_t *err)
{
  volatile unsigned * work_left = (unsigned *) clientdata;
  *work_left = 0;
  MPID_Progress_signal();
}

int
MPIDO_Barrier_dcmf(MPID_Comm * comm)
{
  int rc;

  /* use local (thread safe) active field */
  volatile unsigned active;

  DCMF_Callback_t callback = { barrier_cb_done, (void *) &active};

  /* initialize local (thread safe) active field */
  active = 1;

  /* geometry sets up proper barrier for the geometry at init time */
  rc = DCMF_Barrier(&comm->dcmf.geometry,
                    callback,
                    DCMF_MATCH_CONSISTENCY);

  if (rc == DCMF_SUCCESS)
    MPID_PROGRESS_WAIT_WHILE(* (int *) callback.clientdata);

  return rc;
}

#endif /* USE_CCMI_COLL */
