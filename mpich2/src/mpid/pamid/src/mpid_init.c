/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_init.c
 * \brief Normal job startup code
 */
#include <mpidimpl.h>
#include "onesided/mpidi_onesided.h"

pami_client_t   MPIDI_Client;
pami_context_t MPIDI_Context[MPIDI_MAX_CONTEXTS];

MPIDI_Process_t  MPIDI_Process = {
  .verbose             = 0,
  .statistics          = 0,

  .avail_contexts      = MPIDI_MAX_CONTEXTS,
  .commthreads_active  = 0,
  .commthreads_enabled = USE_PAMI_COMM_THREADS,
  .context_post        = 1,
  .short_limit         = MPIDI_SHORT_LIMIT,
  .eager_limit         = MPIDI_EAGER_LIMIT,

  .rma_pending         = 1000,
  .shmem_pt2pt         = 1,

  .optimized = {
    .collectives       = 0,
    .subcomms          = 1,
  },
};


struct protocol_t
{
  pami_dispatch_p2p_function func;
  size_t                     dispatch;
  size_t                     immediate_min;
  pami_dispatch_hint_t       options;
};
static struct
{
  struct protocol_t Short;
  struct protocol_t ShortSync;
  struct protocol_t Eager;
  struct protocol_t RVZ;
  struct protocol_t Cancel;
  struct protocol_t Control;
  struct protocol_t WinCtrl;
  struct protocol_t WinAccum;
} proto_list = {
  .Short = {
    .func = MPIDI_RecvShortAsyncCB,
    .dispatch = MPIDI_Protocols_Short,
    .options = {
      .consistency     = USE_PAMI_CONSISTENCY,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
  .ShortSync = {
    .func = MPIDI_RecvShortSyncCB,
    .dispatch = MPIDI_Protocols_ShortSync,
    .options = {
      .consistency     = USE_PAMI_CONSISTENCY,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
  .Eager = {
    .func = MPIDI_RecvCB,
    .dispatch = MPIDI_Protocols_Eager,
    .options = {
      .consistency     = USE_PAMI_CONSISTENCY,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_contiguous = PAMI_HINT_ENABLE,
      .recv_copy =       PAMI_HINT_ENABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
  .RVZ = {
    .func = MPIDI_RecvRzvCB,
    .dispatch = MPIDI_Protocols_RVZ,
    .options = {
      .consistency     = USE_PAMI_CONSISTENCY,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgEnvelope),
  },
  .Cancel = {
    .func = MPIDI_ControlCB,
    .dispatch = MPIDI_Protocols_Cancel,
    .options = {
      .consistency     = PAMI_HINT_ENABLE,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
  .Control = {
    .func = MPIDI_ControlCB,
    .dispatch = MPIDI_Protocols_Control,
    .options = {
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
  .WinCtrl = {
    .func = MPIDI_WinControlCB,
    .dispatch = MPIDI_Protocols_WinCtrl,
    .options = {
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_Win_control_t),
  },
  .WinAccum = {
    .func = MPIDI_WinAccumCB,
    .dispatch = MPIDI_Protocols_WinAccum,
    .options = {
      .consistency     = PAMI_HINT_ENABLE,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgInfo),
  },
};


static void
MPIDI_PAMI_client_init(int* rank, int* size)
{
  /* ------------------------------------ */
  /*  Initialize the MPICH2->PAMI Client  */
  /* ------------------------------------ */
  pami_result_t rc;
  rc = PAMI_Client_create("MPI", &MPIDI_Client, NULL, 0);
  MPID_assert_always(rc == PAMI_SUCCESS);
  PAMIX_Initialize(MPIDI_Client);


  /* ---------------------------------- */
  /*  Get my rank and the process size  */
  /* ---------------------------------- */
  *rank = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_TASK_ID  ).value.intval;
  MPIR_Process.comm_world->rank = *rank; /* Set the rank early to make tracing better */
  *size = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_TASKS).value.intval;
}


static void
MPIDI_PAMI_context_init(int* threading)
{
  int requested_thread_level;
  requested_thread_level = *threading;


  unsigned hwthreads = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_HWTHREADS_AVAILABLE).value.intval;
  if (hwthreads == 1)
    {
      /* VNM mode imlies MPI_THREAD_SINGLE, 1 context, and no posting or commthreads. */
      MPIDI_Process.avail_contexts      = 1;
      MPIDI_Process.context_post        = 0;
      MPIDI_Process.commthreads_enabled = 0;
      *threading = MPI_THREAD_SINGLE;
    }
  else if (MPIDI_Process.context_post == 0)
    {
      /* If we aren't posting, so just use 1 context and no commthreads. */
      MPIDI_Process.avail_contexts      = 1;
      MPIDI_Process.commthreads_enabled = 0;
#if USE_PAMI_COMM_THREADS
      /* Pre-obj builds require post & hwthreads for MPI_THREAD_MULTIPLE */
      if (*threading == MPI_THREAD_MULTIPLE)
        *threading = MPI_THREAD_SERIALIZED;
#endif
    }
  else
    {
      /* ---------------------------------- */
      /*  Figure out the context situation  */
      /* ---------------------------------- */
      /*
       * avail_contexts = MIN(getenv("PAMI_MAXCONTEXTS"), MPIDI_MAX_CONTEXTS, PAMI_CONTEXTS, PAMI_HWTHREADS);
       *
       */


      if (MPIDI_Process.avail_contexts > MPIDI_MAX_CONTEXTS)
        MPIDI_Process.avail_contexts = MPIDI_MAX_CONTEXTS;


      unsigned same = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CONST_CONTEXTS).value.intval;
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


      if (MPIDI_Process.avail_contexts > hwthreads)
        MPIDI_Process.avail_contexts = hwthreads;


      /* The number of contexts must be a power-of-two, so decrement until we hit a power-of-two */
      while(MPIDI_Process.avail_contexts & (MPIDI_Process.avail_contexts-1))
        --MPIDI_Process.avail_contexts;
      MPID_assert_always(MPIDI_Process.avail_contexts);


#if USE_PAMI_COMM_THREADS
      /* Help a user who REALLY wants comm-threads by enabling them, irrespective of thread mode, when commthreads_enabled is set to 2 */
      if (MPIDI_Process.commthreads_enabled >= 2)
          *threading = MPI_THREAD_MULTIPLE;
      /* In the per-obj builds, async progress defaults to always ON, so turn it off if not in MPI_THREAD_MULTIPLE */
      if (*threading != MPI_THREAD_MULTIPLE)
        MPIDI_Process.commthreads_enabled = 0;
#endif
    }
  TRACE_ERR("Thread-level=%d, requested=%d\n", *threading, requested_thread_level);


#ifdef OUT_OF_ORDER_HANDLING
  unsigned numTasks  = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_TASKS).value.intval;
  MPIDI_In_cntr = MPIU_Calloc0(numTasks, MPIDI_In_cntr_t);
  if(MPIDI_In_cntr == NULL)
    MPID_abort();
  MPIDI_Out_cntr = MPIU_Calloc0(numTasks, MPIDI_Out_cntr_t);
  if(MPIDI_Out_cntr == NULL)
    MPID_abort();
#endif


  /* ----------------------------------- */
  /*  Create the communication contexts  */
  /* ----------------------------------- */
  pami_configuration_t config ={
    .name  = PAMI_CLIENT_CONST_CONTEXTS,
    .value = { .intval = 1, },
  };
  TRACE_ERR("Creating %d contexts\n", MPIDI_Process.avail_contexts);
  pami_result_t rc;
  rc = PAMI_Context_createv(MPIDI_Client, &config, 1, MPIDI_Context, MPIDI_Process.avail_contexts);
  MPID_assert_always(rc == PAMI_SUCCESS);
}


static void
MPIDI_PAMI_dispath_set(size_t              dispatch,
                       struct protocol_t * proto,
                       unsigned          * immediate_max)
{
  size_t im_max = 0;
  pami_dispatch_callback_function Recv = {.p2p = proto->func};
  MPID_assert_always(dispatch == proto->dispatch);

  if (MPIDI_Process.shmem_pt2pt == 0)
    proto->options.use_shmem = PAMI_HINT_DISABLE;

  PAMIX_Dispatch_set(MPIDI_Context,
                     MPIDI_Process.avail_contexts,
                     proto->dispatch,
                     Recv,
                     proto->options,
                     &im_max);
  TRACE_ERR("Immediate-max query:  dispatch=%zu  got=%zu  required=%zu\n",
            dispatch, im_max, proto->immediate_min);
  MPID_assert_always(proto->immediate_min <= im_max);
  if (immediate_max != NULL)
    *immediate_max = im_max;
}


static void
MPIDI_PAMI_dispath_init()
{
  /* ------------------------------------ */
  /*  Set up the communication protocols  */
  /* ------------------------------------ */
  unsigned pami_short_limit[2] = {MPIDI_Process.short_limit, MPIDI_Process.short_limit};
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Short,     &proto_list.Short,     pami_short_limit+0);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_ShortSync, &proto_list.ShortSync, pami_short_limit+1);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Eager,     &proto_list.Eager,     NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_RVZ,       &proto_list.RVZ,       NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Cancel,    &proto_list.Cancel,    NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Control,   &proto_list.Control,   NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_WinCtrl,   &proto_list.WinCtrl,   NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_WinAccum,  &proto_list.WinAccum,  NULL);

  /*
   * The first two protocols are our short protocols: they use
   * PAMI_Send_immediate() exclusively.  We get the short limit twice
   * because they could be different.
   *
   * - The returned value is the max amount of header+data.  We have
   *     to remove the header size.
   *
   * - We need to add one back, since we don't use "=" in the
   *     comparison.  We use "if (size < short_limit) ...".
   *
   * - We use the min of the results just to be safe.
   */
  pami_short_limit[0] -= (sizeof(MPIDI_MsgInfo) - 1);
  if (MPIDI_Process.short_limit > pami_short_limit[0])
    MPIDI_Process.short_limit = pami_short_limit[0];
  pami_short_limit[1] -= (sizeof(MPIDI_MsgInfo) - 1);
  if (MPIDI_Process.short_limit > pami_short_limit[1])
    MPIDI_Process.short_limit = pami_short_limit[1];
  TRACE_ERR("pami_short_limit[2] = [%u,%u]\n", pami_short_limit[0], pami_short_limit[1]);
}


extern char **environ;
static void
printEnvVars(char *type)
{
   printf("The following %s* environment variables were specified:\n", type);
   char **env;
   for(env = environ; *env != 0 ; env++)
   {
      if(!strncasecmp(*env, type, strlen(type)))
        printf("  %s\n", *env);
   }
}


static void
MPIDI_PAMI_init(int* rank, int* size, int* threading)
{
  MPIDI_PAMI_client_init(rank, size);


  MPIDI_PAMI_context_init(threading);


  MPIDI_PAMI_dispath_init();


  if ( (*rank == 0) && (MPIDI_Process.verbose >= MPIDI_VERBOSE_SUMMARY_0) )
    {
      printf("MPIDI_Process.*\n"
             "  verbose      : %u\n"
             "  statistics   : %u\n"
             "  contexts     : %u\n"
             "  commthreads  : %u\n"
             "  context_post : %u\n"
             "  short_limit  : %u\n"
             "  eager_limit  : %u\n"
             "  rma_pending  : %u\n"
             "  shmem_pt2pt  : %u\n"
             "  optimized.collectives : %u\n"
             "  optimized.subcomms    : %u\n",
             MPIDI_Process.verbose,
             MPIDI_Process.statistics,
             MPIDI_Process.avail_contexts,
             MPIDI_Process.commthreads_enabled,
             MPIDI_Process.context_post,
             MPIDI_Process.short_limit,
             MPIDI_Process.eager_limit,
             MPIDI_Process.rma_pending,
             MPIDI_Process.shmem_pt2pt,
             MPIDI_Process.optimized.collectives,
             MPIDI_Process.optimized.subcomms);
      printEnvVars("PAMI_");
      printEnvVars("BG_");
    }
}


static void
MPIDI_VCRT_init(int rank, int size)
{
  int i, rc;
  MPID_Comm * comm;

  /* ------------------------------- */
  /* Initialize MPI_COMM_SELF object */
  /* ------------------------------- */
  comm = MPIR_Process.comm_self;
  comm->rank = 0;
  comm->remote_size = comm->local_size = 1;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert_always(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert_always(rc == MPI_SUCCESS);
  comm->vcr[0] = rank;


  /* -------------------------------- */
  /* Initialize MPI_COMM_WORLD object */
  /* -------------------------------- */
  comm = MPIR_Process.comm_world;
  comm->rank = rank;
  comm->remote_size = comm->local_size = size;
  rc = MPID_VCRT_Create(comm->remote_size, &comm->vcrt);
  MPID_assert_always(rc == MPI_SUCCESS);
  rc = MPID_VCRT_Get_ptr(comm->vcrt, &comm->vcr);
  MPID_assert_always(rc == MPI_SUCCESS);
  for (i=0; i<size; i++)
    comm->vcr[i] = i;
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
  int rank, size;


  /* ------------------------------------ */
  /*  Get new defaults from the Env Vars  */
  /* ------------------------------------ */
  MPIDI_Env_setup();


  /* ----------------------------- */
  /* Initialize messager           */
  /* ----------------------------- */
  *provided = requested;
  MPIDI_PAMI_init(&rank, &size, provided);


  /* ------------------------- */
  /* initialize request queues */
  /* ------------------------- */
  MPIDI_Recvq_init();

  /* -------------------------------------- */
  /* Fill in some hardware structure fields */
  /* -------------------------------------- */
  extern void MPIX_Init();
  MPIX_Init();

  /* ------------------------------- */
  /* Set process attributes          */
  /* ------------------------------- */
  MPIR_Process.attrs.tag_ub = INT_MAX;
  MPIR_Process.attrs.wtime_is_global = 1;


  /* ------------------------------- */
  /* Initialize communicator objects */
  /* ------------------------------- */
  MPIDI_VCRT_init(rank, size);


  /* ------------------------------- */
  /* Setup optimized communicators   */
  /* ------------------------------- */
  TRACE_ERR("creating world geometry\n");
  pami_result_t rc;
  rc = PAMI_Geometry_world(MPIDI_Client, &MPIDI_Process.world_geometry);
  MPID_assert_always(rc == PAMI_SUCCESS);
  TRACE_ERR("calling comm_create on comm world %p\n", MPIR_Process.comm_world);
  MPIR_Process.comm_world->mpid.geometry = MPIDI_Process.world_geometry;
  MPIR_Process.comm_world->mpid.parent   = PAMI_GEOMETRY_NULL;
  MPIDI_Comm_create(MPIR_Process.comm_world);
  MPIDI_Comm_world_setup();


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
int MPID_InitCompleted()
{
  MPIDI_Progress_init();
  return MPI_SUCCESS;
}


static inline void
static_assertions()
{
  MPID_assert_static(sizeof(void*) == sizeof(size_t));
  MPID_assert_static(sizeof(uintptr_t) == sizeof(size_t));
#ifdef __BGP__
  MPID_assert_static(sizeof(MPIDI_MsgInfo) == 16);
#endif
#ifdef __BGQ__
  MPID_assert_static(sizeof(MPIDI_MsgInfo) == 16);
  MPID_assert_static(sizeof(uint64_t) == sizeof(size_t));
#endif
}
