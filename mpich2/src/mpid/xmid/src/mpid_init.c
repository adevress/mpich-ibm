/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/misc/mpid_init.c
 * \brief Normal job startup code
 */
#include "mpidimpl.h"
#include <limits.h>

MPIDI_Protocol_t MPIDI_Protocols =
  {
  Send:0
  };
MPIDI_Process_t  MPIDI_Process;

const size_t     NUM_CONTEXTS = 1;
xmi_client_t     MPIDI_Client;
xmi_context_t    MPIDI_Context[1];


void MPIDI_Init(int* rank, int* size)
{
  xmi_result_t rc;
  xmi_configuration_t query;
  xmi_send_hint_t options = {consistency:1};

  /* ----------------------------- */
  /* Initialize messager           */
  /* ----------------------------- */
  rc = XMI_Client_initialize("MPICH2", &MPIDI_Client);
  MPID_assert(rc == XMI_SUCCESS);
  rc = XMI_Context_createv(MPIDI_Client, NULL, 0, MPIDI_Context, NUM_CONTEXTS);
  MPID_assert(rc == XMI_SUCCESS);


  /* ---------------------------------------- */
  /*  Get my rank and the process size        */
  /* ---------------------------------------- */
  query.name = XMI_TASK_ID;
  rc = XMI_Configuration_query (MPIDI_Client, &query);
  MPID_assert(rc == XMI_SUCCESS);
  *rank = query.value.intval;

  query.name = XMI_NUM_TASKS;
  rc = XMI_Configuration_query (MPIDI_Client, &query);
  MPID_assert(rc == XMI_SUCCESS);
  *size = query.value.intval;


  xmi_dispatch_callback_fn Recv = {p2p:MPIDI_RecvCB};
  size_t i;
  for (i=0; i<NUM_CONTEXTS; ++i) {
    rc = XMI_Dispatch_set(MPIDI_Context[i],
                          MPIDI_Protocols.Send,
                          Recv,
                          (void*)i,
                          options);
    MPID_assert(rc == XMI_SUCCESS);
  }
}




/**
 * \brief Initialize MPICH2 at ADI level.
 * \param[in,out] argc Unused
 * \param[in,out] argv Unused
 * \param[in]     requested The thread model requested by the user.
 * \param[out]    provided  The thread model provided to user.  This will be the same as requested, except in VNM.
 * \param[out]    has_args  Set to TRUE
 * \param[out]    has_env   Set to TRUE
 * \return MPI_SUCCESS
 */
int MPID_Init(int * argc,
              char *** argv,
              int   requested,
              int * provided,
              int * has_args,
              int * has_env)
{
  int rank, size, i, rc;
  MPID_Comm * comm;


  /* ----------------------------- */
  /* Initialize messager           */
  /* ----------------------------- */
  MPIDI_Init(&rank, &size);


  /* ------------------------- */
  /* initialize request queues */
  /* ------------------------- */
  MPIDI_Recvq_init();


  /* ------------------------------------------------------ */
  /* Set process attributes.                                */
  /* ------------------------------------------------------ */
  MPIR_Process.attrs.tag_ub = INT_MAX;
  MPIR_Process.attrs.wtime_is_global = 1;
  /* if (MPIDI_Process.optimized.topology) */
  /*   MPIR_Process.dimsCreate = MPID_Dims_create; */


  /* -------------------------------- */
  /* Initialize MPI_COMM_WORLD object */
  /* -------------------------------- */
  comm = MPIR_Process.comm_world;
  comm->rank = rank;
  comm->remote_size = comm->local_size = size;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert(rc == MPI_SUCCESS);
  for (i=0; i<size; i++)
    comm->vcr[i] = i;

  /* comm_create for MPI_COMM_WORLD needs this information to ensure no
   * barriers are done in dual mode with multithreading
   * We don't get the thread_provided updated until AFTER MPID_Init is
   * finished so we need to know the requested thread level in comm_create
   */
  /* MPIDI_Comm_create(comm); */

  /* ------------------------------- */
  /* Initialize MPI_COMM_SELF object */
  /* ------------------------------- */

  comm = MPIR_Process.comm_self;
  comm->rank = 0;
  comm->remote_size = comm->local_size = 1;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert(rc == MPI_SUCCESS);
  comm->vcr[0] = rank;

  /* ------------------------------- */
  /* Initialize timer data           */
  /* ------------------------------- */
  MPID_Wtime_init();

  /* ------------------------------- */
  *has_args = TRUE;
  *has_env  = TRUE;

  return MPI_SUCCESS;
}

/*
 * \brief This is called by MPI to let us know that MPI_Init is done.
 * We don't care.
 */
int MPID_InitCompleted(void)
{
  return MPI_SUCCESS;
}
