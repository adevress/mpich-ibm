/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_init.c
 * \brief Normal job startup code
 */
#include "mpidimpl.h"
#include "mpidi_onesided.h"

pami_client_t   MPIDI_Client;
#define MAX_CONTEXTS 16
pami_context_t MPIDI_Context[MAX_CONTEXTS];


MPIDI_Process_t  MPIDI_Process = {
 verbose        : 0,
 statistics     : 0,
 short_limit    : 0,
#ifdef __BGQ__
 eager_limit    : 1234,
#else
 eager_limit    : UINT_MAX,
#endif
 use_interrupts : 0,
 rma_pending    : 1000,
 avail_contexts : MAX_CONTEXTS,

 optimized : {
  collectives : 0,
  topology    : 0,
  },
};


struct protocol_t
{
  pami_dispatch_p2p_fn func;
  size_t               dispatch;
  size_t               immediate_min;
  pami_send_hint_t     options;
};
static struct
{
  struct protocol_t Send;
  struct protocol_t RTS;
  struct protocol_t Cancel;
  struct protocol_t Control;
  struct protocol_t WinCtrl;
} proto_list =
  {
  Send: {
    func: MPIDI_RecvCB,
    dispatch: 0,
    options: {
      consistency:    PAMI_HINT2_ON,
      no_long_header: PAMI_HINT2_ON,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  RTS: {
    func: MPIDI_RecvRzvCB,
    dispatch: 1,
    options: {
      consistency:    PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_MsgEnvelope),
  },
  Cancel: {
    func: MPIDI_ControlCB,
    dispatch: 2,
    options: {
      consistency:    PAMI_HINT2_ON,
      no_long_header: PAMI_HINT2_ON,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  Control: {
    func: MPIDI_ControlCB,
    dispatch: 3,
    options: {
      high_priority:  PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      no_long_header: PAMI_HINT2_ON,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  WinCtrl: {
    func: MPIDI_WinControlCB,
    dispatch: 4,
    options: {
      high_priority:  PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      no_long_header: PAMI_HINT2_ON,
      },
    immediate_min : sizeof(MPIDI_Win_control_t),
  },
  };
MPIDI_Protocol_t MPIDI_Protocols =
  {
  Send:    0,
  RTS:     1,
  Cancel : 2,
  Control: 3,
  WinCtrl: 4,
  };


static inline void
MPIDI_Init_dispath(size_t              dispatch,
                   struct protocol_t * proto,
                   unsigned          * immediate_max)
{
  size_t im_max = 0;
  pami_dispatch_callback_fn Recv = {p2p:proto->func};
  MPID_assert(dispatch == proto->dispatch);
  PAMIX_Dispatch_set(MPIDI_Context,
                     MPIDI_Process.avail_contexts,
                     proto->dispatch,
                     Recv,
                     proto->options,
                     &im_max);
  TRACE_ERR("Immediate-max query:  dispatch=%zu  got=%zu  required=%zu\n", dispatch, im_max, proto->immediate_min);
  MPID_assert(proto->immediate_min <= im_max);
  if (immediate_max != NULL)
    *immediate_max = im_max;
}


static inline void
MPIDI_Init(int* rank, int* size, int* threading)
{
  pami_result_t rc;

  /* ------------------------------------ */
  /*  Get new defaults from the Env Vars  */
  /* ------------------------------------ */
  MPIDI_Env_setup();

  /* ------------------------------------ */
  /*  Initialize the MPICH2->PAMI Client  */
  /* ------------------------------------ */
  rc = PAMI_Client_create("MPICH2", &MPIDI_Client, NULL, 0);
  MPID_assert(rc == PAMI_SUCCESS);

  /* ---------------------------------- */
  /*  Get my rank and the process size  */
  /* ---------------------------------- */
  *rank = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_TASK_ID  ).value.intval;
  MPIR_Process.comm_world->rank = *rank; /* Set the rank early to make tracing better */
  *size = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_TASKS).value.intval;

  /* ---------------------------------- */
  /*  Figure out the context situation  */
  /* ---------------------------------- */
  unsigned same  = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CONST_CONTEXTS).value.intval;
  if(same)
    {
      unsigned possible_contexts = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_CONTEXTS).value.intval;
      TRACE_ERR("PAMI allows up to %u contexts; MPICH2 allows up to %u\n", possible_contexts, MPIDI_Process.avail_contexts);
      if(MPIDI_Process.avail_contexts > possible_contexts)
        MPIDI_Process.avail_contexts = possible_contexts;
    }
  else
    {
      MPIDI_Process.avail_contexts = 1; /* If PAMI didn't give all nodes the same number of contexts, all bets are off for now */
    }

  /* -------------------------------------------------------- */
  /*  We didn't lock on the way in, but we will lock on the   */
  /*  way out if we are threaded.  Lock now to make it even.  */
  /* -------------------------------------------------------- */
  if (PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_HWTHREADS_AVAILABLE).value.intval > 1)
    {
      /** \todo Add these in when trac #72 is fixed */
#if 0
      MPIR_ThreadInfo.isThreaded = 1;
      MPID_CS_ENTER();
#else
      *threading = MPI_THREAD_SINGLE;
#endif
    }
  else
    {
      *threading = MPI_THREAD_SINGLE;
    }

  /* ----------------------------------- */
  /*  Create the communication contexts  */
  /* ----------------------------------- */
  pami_configuration_t config ={
  name  : PAMI_CLIENT_CONST_CONTEXTS,
  value : { intval : 1, },
  };
  TRACE_ERR("Creating %d contexts\n", MPIDI_Process.avail_contexts);
  rc = PAMI_Context_createv(MPIDI_Client, &config, 1, MPIDI_Context, MPIDI_Process.avail_contexts);
  MPID_assert(rc == PAMI_SUCCESS);


  /* ------------------------------------ */
  /*  Set up the communication protocols  */
  /* ------------------------------------ */
  MPIDI_Init_dispath(MPIDI_Protocols.Send,    &proto_list.Send, &MPIDI_Process.short_limit);
  MPIDI_Process.short_limit -= sizeof(MPIDI_MsgInfo);
  TRACE_ERR("short_limit = %u\n", MPIDI_Process.short_limit);
  MPIDI_Init_dispath(MPIDI_Protocols.RTS,     &proto_list.RTS, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.Cancel,  &proto_list.Cancel, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.Control, &proto_list.Control, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.WinCtrl, &proto_list.WinCtrl, NULL);


  /* Fill in the world geometry */
  TRACE_ERR("creating world geometry\n");
  rc = PAMI_Geometry_world(MPIDI_Client, &MPIDI_Process.world_geometry);
  MPID_assert(rc == PAMI_SUCCESS);

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
  *provided = requested;
  MPIDI_Init(&rank, &size, provided);


  /* ------------------------- */
  /* initialize request queues */
  /* ------------------------- */
  MPIDI_Recvq_init();

  /* -------------------------------------- */
  /* FIll in some hardware structure fields */
  /* -------------------------------------- */

  MPIDI_HW_Init(&mpid_hw);

  /* ------------------------------------------------------ */
  /* Set process attributes.                                */
  /* ------------------------------------------------------ */
  MPIR_Process.attrs.tag_ub = INT_MAX;
  MPIR_Process.attrs.wtime_is_global = 1;
  if (MPIDI_Process.optimized.topology)
    MPIR_Process.dimsCreate = MPID_Dims_create;

  /* -------------------------------- */
  /* Initialize MPI_COMM_WORLD object */
  /* -------------------------------- */
  comm = MPIR_Process.comm_world;
  comm->mpid.geometry = MPIDI_Process.world_geometry;
  comm->rank = rank;
  comm->remote_size = comm->local_size = size;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert(rc == MPI_SUCCESS);
  for (i=0; i<size; i++)
    comm->vcr[i] = i;

   /* basically a noop for now */
  MPIDI_Comm_create(comm);

  MPIDI_Comm_world_setup();


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
  /* ???                             */
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
