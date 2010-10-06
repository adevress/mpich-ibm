/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_init.c
 * \brief Normal job startup code
 */
#include <mpidimpl.h>
#include "onesided/mpidi_onesided.h"

pami_client_t   MPIDI_Client;
#define MAX_CONTEXTS 16
pami_context_t MPIDI_Context[MAX_CONTEXTS];

#ifdef MPIDI_USE_OPA
OPA_int_t       MPIDI_Mutex_vector [MPIDI_MAX_MUTEXES];
__thread int    MPIDI_Mutex_counter[MPIDI_MAX_MUTEXES] = {0};
#else
pthread_mutex_t MPIDI_Mutex_lock;
#endif

MPIDI_Process_t  MPIDI_Process = {
 verbose        : 0,
 statistics     : 0,

 avail_contexts : MAX_CONTEXTS,
 comm_threads   : 0,
 context_post   : 0,
 optimized_subcomms: 1,
 short_limit    : 0,
#ifdef __BGQ__
 eager_limit    : 1234,
#else
 eager_limit    : UINT_MAX,
#endif

 rma_pending    : 1000,

 optimized : {
  collectives : 0,
  topology    : 0,
  },
};


struct protocol_t
{
  pami_dispatch_p2p_function func;
  size_t                     dispatch;
  size_t                     immediate_min;
  pami_send_hint_t           options;
};
static struct
{
  struct protocol_t Send;
  struct protocol_t SendShort;
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
      /** \todo Turn off immediate recvs when ticket #46 is finished */
      /* recv_immediate: PAMI_HINT3_FORCE_OFF */
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  SendShort: {
    func: MPIDI_RecvShortCB,
    dispatch: 1,
    options: {
      consistency:    PAMI_HINT2_ON,
      no_long_header: PAMI_HINT2_ON,
      recv_immediate: PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  RTS: {
    func: MPIDI_RecvRzvCB,
    dispatch: 2,
    options: {
      consistency:    PAMI_HINT2_ON,
      no_long_header: PAMI_HINT2_ON,
      recv_immediate: PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_MsgEnvelope),
  },
  Cancel: {
    func: MPIDI_ControlCB,
    dispatch: 3,
    options: {
      consistency:    PAMI_HINT2_ON,
      no_long_header: PAMI_HINT2_ON,
      recv_immediate: PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  Control: {
    func: MPIDI_ControlCB,
    dispatch: 4,
    options: {
      no_long_header: PAMI_HINT2_ON,
      recv_immediate: PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_MsgInfo),
  },
  WinCtrl: {
    func: MPIDI_WinControlCB,
    dispatch: 5,
    options: {
      no_long_header: PAMI_HINT2_ON,
      recv_immediate: PAMI_HINT2_ON,
      use_rdma:       PAMI_HINT3_FORCE_OFF,
      },
    immediate_min : sizeof(MPIDI_Win_control_t),
  },
  };
MPIDI_Protocol_t MPIDI_Protocols =
  {
  Send      : 0,
  SendShort : 1,
  RTS       : 2,
  Cancel    : 3,
  Control   : 4,
  WinCtrl   : 5,
  };


static inline void
MPIDI_Init_dispath(size_t              dispatch,
                   struct protocol_t * proto,
                   unsigned          * immediate_max)
{
  size_t im_max = 0;
  pami_dispatch_callback_function Recv = {p2p:proto->func};
  MPID_assert(dispatch == proto->dispatch);
  PAMIX_Dispatch_set(MPIDI_Context,
                     MPIDI_Process.avail_contexts,
                     proto->dispatch,
                     Recv,
                     proto->options,
                     &im_max);
  TRACE_ERR("Immediate-max query:  dispatch=%zu  got=%zu  required=%zu\n",
            dispatch, im_max, proto->immediate_min);
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
  PAMIX_Initialize(MPIDI_Client);

  /* ---------------------------------- */
  /*  Get my rank and the process size  */
  /* ---------------------------------- */
  *rank = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_TASK_ID  ).value.intval;
  MPIR_Process.comm_world->rank = *rank; /* Set the rank early to make tracing better */
  *size = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_TASKS).value.intval;

  /* ---------------------------------- */
  /*  Figure out the context situation  */
  /* ---------------------------------- */
  if (MPIDI_Process.avail_contexts > MAX_CONTEXTS)
    MPIDI_Process.avail_contexts = MAX_CONTEXTS;
  unsigned same  = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CONST_CONTEXTS).value.intval;
  if (same)
    {
      unsigned possible_contexts = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_CONTEXTS).value.intval;
      TRACE_ERR("PAMI allows up to %u contexts; MPICH2 allows up to %u\n",
                possible_contexts, MPIDI_Process.avail_contexts);
      if (MPIDI_Process.avail_contexts > possible_contexts)
        MPIDI_Process.avail_contexts = possible_contexts;
    }
  else
    {
      /* If PAMI didn't give all nodes the same number of contexts, all bets are off for now */
      MPIDI_Process.avail_contexts = 1;
    }


  /* Only use one context when not posting work */
  if (MPIDI_Process.context_post == 0)
    {
      MPIDI_Process.avail_contexts = 1;
      MPIDI_Process.comm_threads   = 0;
    }


  MPIDI_Process.requested_thread_level = *threading;
  /* VNM mode imlies MPI_THREAD_SINGLE, 1 context, and no posting. */
  if (PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_HWTHREADS_AVAILABLE).value.intval == 1)
    {
      *threading = MPIDI_Process.requested_thread_level = MPI_THREAD_SINGLE;
      MPIDI_Process.avail_contexts = 1;
      MPIDI_Process.context_post   = 0;
      MPIDI_Process.comm_threads   = 0;
    }
#ifdef USE_PAMI_COMM_THREADS
  if (MPIDI_Process.comm_threads == 1)
    *threading = MPI_THREAD_MULTIPLE;
#endif
  TRACE_ERR("Thread-level=%d, requested=%d\n", *threading, MPIDI_Process.requested_thread_level);

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
  MPIDI_Init_dispath(MPIDI_Protocols.RTS,       &proto_list.RTS, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.Cancel,    &proto_list.Cancel, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.Control,   &proto_list.Control, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.WinCtrl,   &proto_list.WinCtrl, NULL);
  MPIDI_Init_dispath(MPIDI_Protocols.SendShort, &proto_list.SendShort,  NULL);


  /* Fill in the world geometry */
  TRACE_ERR("creating world geometry\n");
  rc = PAMI_Geometry_world(MPIDI_Client, &MPIDI_Process.world_geometry);
  MPID_assert(rc == PAMI_SUCCESS);

  if (MPIDI_Process.verbose > 0)
    {
      printf("MPIDI_Process.*\n"
             "  verbose      : %u\n"
             "  statistics   : %u\n"
             "  contexts     : %u\n"
             "  comm_threads : %u\n"
             "  context_post : %u\n"
             "  short_limit  : %u\n"
             "  eager_limit  : %u\n"
             "  rma_pending  : %u\n"
             "  optimized.collectives : %u\n"
             "  optimized.topology    : %u\n",
             MPIDI_Process.verbose,
             MPIDI_Process.statistics,
             MPIDI_Process.avail_contexts,
             MPIDI_Process.comm_threads,
             MPIDI_Process.context_post,
             MPIDI_Process.short_limit,
             MPIDI_Process.eager_limit,
             MPIDI_Process.rma_pending,
             MPIDI_Process.optimized.collectives,
             MPIDI_Process.optimized.topology);
    }
}


/**
 * \brief Initialize MPICH2 at ADI level.
 * \param[in,out] argc Unused
 * \param[in,out] argv Unused
 * \param[in]     requested The thread model requested by the user.
 * \param[out]    provided  The thread model provided to user.  It is the same as requested, except in VNM.
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
  extern void MPIX_Init();
  MPIX_Init();

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
  comm->mpid.parent = PAMI_NULL_GEOMETRY;
  comm->rank = rank;
  comm->remote_size = comm->local_size = size;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert(rc == MPI_SUCCESS);
  for (i=0; i<size; i++)
    comm->vcr[i] = i;

  MPIDI_Comm_create(comm);

  /* basically a noop for now */
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
 */
int MPID_InitCompleted(void)
{
#if USE_PAMI_COMM_THREADS
  if (MPIDI_Process.comm_threads)
    {
      TRACE_ERR("Async advance beginning...\n");

      extern pami_result_t
        PAMI_Client_add_commthread_context(pami_client_t client,
                                           pami_context_t context);
      unsigned i;
      pami_result_t rc;

      for (i=0; i<MPIDI_Process.avail_contexts; ++i) {
        rc = PAMI_Client_add_commthread_context(MPIDI_Client, MPIDI_Context[i]);
        assert(rc == PAMI_SUCCESS);
      }

      TRACE_ERR("Async advance enabled\n");
    }
#endif

  return MPI_SUCCESS;
}
