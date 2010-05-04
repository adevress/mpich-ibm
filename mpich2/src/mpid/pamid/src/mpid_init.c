/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_init.c
 * \brief Normal job startup code
 */
#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) fprintf x
//#define        MAX_CONTEXTS 1
size_t         _CONTEXTS;
pami_client_t  MPIDI_Client;
pami_context_t *MPIDI_Context;



MPIDI_Process_t  MPIDI_Process = {
 verbose        : 0,
 statistics     : 0,
 eager_limit    : UINT_MAX,
 use_interrupts : 0,
 rma_pending    : 1000,
 avail_contexts : 2, /* default to 2 contexts */

 optimized : {
  collectives : 0,
  topology    : 0,
  },
};

struct protocol_t
{
  pami_dispatch_p2p_fn func;
  size_t               dispatch;
  pami_send_hint_t     options;
};
static struct
{
  struct protocol_t Send;
  struct protocol_t RTS;
  struct protocol_t Cancel;
  struct protocol_t Control;
} proto_list =
  {
  Send: {
    func: MPIDI_RecvCB,
    dispatch: 0,
    options: {
      consistency:    1,
      no_long_header: 1,
      },
  },
  RTS: {
    func: MPIDI_RecvRzvCB,
    dispatch: 1,
    options: {
      consistency:    1,
      no_rdma:        1,
      },
  },
  Cancel: {
    func: MPIDI_ControlCB,
    dispatch: 2,
    options: {
      consistency:    1,
      no_long_header: 1,
      },
  },
  Control: {
    func: MPIDI_ControlCB,
    dispatch: 3,
    options: {
      high_priority:  1,
      no_rdma:        1,
      no_long_header: 1,
      },
  },
  };
MPIDI_Protocol_t MPIDI_Protocols =
  {
  Send:    0,
  RTS:     1,
  Cancel : 2,
  Control: 3,
  };


static inline void
MPIDI_Init_dispath(size_t dispatch, struct protocol_t* proto)
{
  pami_dispatch_callback_fn Recv = {p2p:proto->func};
  MPID_assert(dispatch == proto->dispatch);
  PAMIX_Dispatch_set(MPIDI_Context,
                     MPIDI_Process.avail_contexts,
                     proto->dispatch,
                     Recv,
                     proto->options);
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
  rc = PAMI_Client_create("MPICH2", &MPIDI_Client);
  MPID_assert(rc == PAMI_SUCCESS);

  /* ---------------------------------- */
  /*  Get my rank and the process size  */
  /* ---------------------------------- */
  *rank          = PAMIX_Configuration_query(MPIDI_Client, PAMI_TASK_ID       ).value.intval;
  *size          = PAMIX_Configuration_query(MPIDI_Client, PAMI_NUM_TASKS     ).value.intval;

  /* ---------------------------------- */
  /*  Figure out the context situation  */
  /* ---------------------------------- */
  unsigned same  = PAMIX_Configuration_query(MPIDI_Client, PAMI_CONST_CONTEXTS).value.intval;
   if(!same)
   {
      MPIDI_Process.avail_contexts = 1; /* all bets are off for now */
   }
   else
   {
      unsigned possible_contexts = PAMIX_Configuration_query(MPIDI_Client, PAMI_NUM_CONTEXTS).value.intval;
      if(MPIDI_Process.avail_contexts > possible_contexts)
      {
         MPIDI_Process.avail_contexts = possible_contexts;
      }
   }
   if(MPIDI_Process.avail_contexts == 1)
   {
      *threading = MPI_THREAD_SINGLE;
   }
   if(MPIDI_Process.avail_contexts > 1)
   {
      TRACE_ERR((stderr,"Num contexts :%d (>1), can't use shmem collectives\n", MPIDI_Process.avail_contexts));
      MPIDI_Process.optimized.collectives = 0;
   }

   MPIDI_Context = (pami_context_t *)malloc(sizeof(pami_context_t) * MPIDI_Process.avail_contexts);

   TRACE_ERR((stderr,"Creating %d contexts\n", (int)MPIDI_Process.avail_contexts));
         
  /* ----------------------------------- */
  /*  Create the communication contexts  */
  /* ----------------------------------- */
  pami_configuration_t config ={
  name  : PAMI_CONST_CONTEXTS,
  value : { intval : 1, },
  };
  rc = PAMI_Context_createv(MPIDI_Client, &config, 1, MPIDI_Context, MPIDI_Process.avail_contexts);
  MPID_assert(rc == PAMI_SUCCESS);


  /* -------------------------------------------------------- */
  /*  We didn't lock on the way in, but we will lock on the   */
  /*  way out if we are threaded.  Lock now to make it even.  */
  /* -------------------------------------------------------- */
  if (MPIDI_Process.avail_contexts > 1)
    {
      /** \todo Add these in when #72 is fixed */
      /* MPIR_ThreadInfo.isThreaded = 1; */
      /* MPID_CS_ENTER(); */
    }

  /* ------------------------------------ */
  /*  Set up the communication protocols  */
  /* ------------------------------------ */
  MPIDI_Init_dispath(MPIDI_Protocols.Send,    &proto_list.Send);
  MPIDI_Init_dispath(MPIDI_Protocols.RTS,     &proto_list.RTS);
  MPIDI_Init_dispath(MPIDI_Protocols.Cancel,  &proto_list.Cancel);
  MPIDI_Init_dispath(MPIDI_Protocols.Control, &proto_list.Control);

  /* Fill in the world geometry */
  TRACE_ERR((stderr, "creating world geometry\n"));
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


  /* ------------------------------------------------------ */
  /* Set process attributes.                                */
  /* ------------------------------------------------------ */
  MPIR_Process.attrs.tag_ub = INT_MAX;
  MPIR_Process.attrs.wtime_is_global = 1;
  if (MPIDI_Process.optimized.topology)
    MPIR_Process.dimsCreate = MPID_Dims_create;

  MPIDI_Process.global.rank = rank;
  MPIDI_Process.global.size = size;

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
