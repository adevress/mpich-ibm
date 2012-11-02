/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpid_init.c
 * \brief Normal job startup code
 */

#include <stdlib.h>
#include <string.h>

#include <mpidimpl.h>
#include "mpidi_platform.h"
#include "onesided/mpidi_onesided.h"

#if TOKEN_FLOW_CONTROL
  extern int MPIDI_mm_init(int,uint *,unsigned long *);
  extern int MPIDI_tfctrl_enabled;
#endif

#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
  pami_extension_t pe_extension;
#endif

pami_client_t   MPIDI_Client;
pami_context_t MPIDI_Context[MPIDI_MAX_CONTEXTS];

MPIDI_Process_t  MPIDI_Process = {
  .verbose               = 0,
  .statistics            = 0,

#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
  .avail_contexts        = MPIDI_MAX_CONTEXTS,
  .async_progress = {
    .active              = 0,
    .mode                = ASYNC_PROGRESS_MODE_DEFAULT,
  },
  .perobj = {
    .context_post = {
      .requested         = (ASYNC_PROGRESS_MODE_DEFAULT == ASYNC_PROGRESS_MODE_LOCKED),
      .active            = 0,
    },
  },
#else
  .avail_contexts        = 1,
  .async_progress = {
    .active              = 0,
    .mode                = ASYNC_PROGRESS_MODE_DISABLED,
  },
  .perobj = {
    .context_post = {
      .requested         = 0,
      .active            = 0,
    },
  },
#endif
  .pt2pt = {
    .limits = {
      .application = {
        .eager = {
          .remote        = MPIDI_EAGER_LIMIT,
          .local         = MPIDI_EAGER_LIMIT_LOCAL,
        },
        .immediate = {
          .remote        = MPIDI_SHORT_LIMIT,
          .local         = MPIDI_SHORT_LIMIT,
        },
      },
      .internal = {
        .eager = {
          .remote        = MPIDI_EAGER_LIMIT,
          .local         = MPIDI_EAGER_LIMIT_LOCAL,
        },
        .immediate = {
          .remote        = MPIDI_SHORT_LIMIT,
          .local         = MPIDI_SHORT_LIMIT,
        },
      },
    },
  },
  .disable_internal_eager_scale = MPIDI_DISABLE_INTERNAL_EAGER_SCALE,
#if TOKEN_FLOW_CONTROL
  .mp_buf_mem          = BUFFER_MEM_DEFAULT,
  .is_token_flow_control_on = 0,
#endif
#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
  .mp_infolevel          = 0,
  .mp_statistics         = 0,
  .mp_printenv           = 0,
#endif

  .rma_pending           = 1000,
  .shmem_pt2pt           = 1,

  .optimized = {
    .collectives         = MPIDI_OPTIMIZED_COLLECTIVE_DEFAULT,
    .subcomms            = 1,
    .select_colls        = 2,
  },

  .mpir_nbc              = 0,
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
  struct protocol_t RVZ_zerobyte;
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
  .RVZ_zerobyte = {
    .func = MPIDI_RecvRzvCB_zerobyte,
    .dispatch = MPIDI_Protocols_RVZ_zerobyte,
    .options = {
      .consistency     = USE_PAMI_CONSISTENCY,
      .long_header     = PAMI_HINT_DISABLE,
      .recv_immediate  = PAMI_HINT_ENABLE,
      .use_rdma        = PAMI_HINT_DISABLE,
    },
    .immediate_min     = sizeof(MPIDI_MsgEnvelope),
  },
};

static void
MPIDI_PAMI_client_init(int* rank, int* size, int threading)
{
  /* ------------------------------------ */
  /*  Initialize the MPICH2->PAMI Client  */
  /* ------------------------------------ */
  pami_configuration_t config;
  pami_result_t        rc = PAMI_ERROR;
  unsigned             n  = 0;

  rc = PAMI_Client_create("MPI", &MPIDI_Client, &config, n);
  MPID_assert_always(rc == PAMI_SUCCESS);
  PAMIX_Initialize(MPIDI_Client);


  /* ---------------------------------- */
  /*  Get my rank and the process size  */
  /* ---------------------------------- */
  *rank = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_TASK_ID  ).value.intval;
  MPIR_Process.comm_world->rank = *rank; /* Set the rank early to make tracing better */
  *size = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_TASKS).value.intval;

  /* --------------------------------------------------------------- */
  /* Determine if the eager point-to-point protocol for internal mpi */
  /* operations should be disabled.                                  */
  /* --------------------------------------------------------------- */
  {
    char * env = getenv("PAMID_DISABLE_INTERNAL_EAGER_TASK_LIMIT");
    if (env != NULL)
      {
        size_t i, n = strlen(env);
        char * tmp = (char *) MPIU_Malloc(n+1);
        strncpy(tmp,env,n);
        if (n>0) tmp[n]=0;

        MPIDI_atoi(tmp, &MPIDI_Process.disable_internal_eager_scale);

        MPIU_Free(tmp);
      }

    if (MPIDI_Process.disable_internal_eager_scale <= *size)
      {
        MPIDI_Process.pt2pt.limits.internal.eager.remote     = 0;
        MPIDI_Process.pt2pt.limits.internal.eager.local      = 0;
        MPIDI_Process.pt2pt.limits.internal.immediate.remote = 0;
        MPIDI_Process.pt2pt.limits.internal.immediate.local  = 0;
      }
  }
}


static void
MPIDI_PAMI_context_init(int* threading, int *size)
{
  int requested_thread_level;
  requested_thread_level = *threading;
#ifdef OUT_OF_ORDER_HANDLING
  extern int numTasks;
#endif

#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
  /*
   * ASYNC_PROGRESS_MODE_LOCKED requires context post because the async thread
   * will hold the context lock indefinitely; the only option for an application
   * thread to interact with the context is to use context post. See discussion
   * in src/mpid/pamid/src/mpid_progress.h for more information.
   *
   * There are three possible resolutions for the situation when context post is
   * disabled and async progess mode is 'locked':
   *  1. abort
   *  2. silently enable context post
   *  3. silently demote async progress mode to ASYNC_PROGRESS_MODE_TRIGGER
   *
   * For now this configuration is considered erroneous and mpi will abort.
   */
  if (MPIDI_Process.async_progress.mode == ASYNC_PROGRESS_MODE_LOCKED &&
      MPIDI_Process.perobj.context_post.requested == 0)
    MPID_Abort (NULL, 0, 1, "'locking' async progress requires context post");

#else /* MPIU_THREAD_GRANULARITY != MPIU_THREAD_GRANULARITY_PER_OBJECT */
  /*
   * ASYNC_PROGRESS_MODE_LOCKED is not applicable in the "global lock" thread
   * mode. See discussion in src/mpid/pamid/src/mpid_progress.h for more
   * information.
   *
   * This configuration is considered erroneous and mpi will abort.
   */
  if (MPIDI_Process.async_progress.mode == ASYNC_PROGRESS_MODE_LOCKED)
    MPID_Abort (NULL, 0, 1, "'locking' async progress not applicable");
#endif

  /* ----------------------------------
   *  Figure out the context situation
   * ---------------------------------- */
#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)

  /* Limit the number of requested contexts by the maximum number of contexts
   * allowed.  The default number of requested contexts depends on the mpich
   * lock mode, 'global' or 'perobj', and may be changed before this point
   * by an environment variables.
   */
  if (MPIDI_Process.avail_contexts > MPIDI_MAX_CONTEXTS)
    MPIDI_Process.avail_contexts = MPIDI_MAX_CONTEXTS;

  unsigned same = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CONST_CONTEXTS).value.intval;
  if (same)
    {
      /* Determine the maximum number of contexts supported; limit the number of
       * requested contexts by this value.
       */
      unsigned possible_contexts = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_NUM_CONTEXTS).value.intval;
      TRACE_ERR("PAMI allows up to %u contexts; MPICH2 allows up to %u\n",
                possible_contexts, MPIDI_Process.avail_contexts);
      if (MPIDI_Process.avail_contexts > possible_contexts)
        MPIDI_Process.avail_contexts = possible_contexts;
    }
  else
    {
      /* If PAMI didn't give all nodes the same number of contexts, all bets
       * are off for now */
      MPIDI_Process.avail_contexts = 1;
    }

  /* The number of contexts must be a power-of-two, as required by the
   * MPIDI_Context_hash() function. Decrement until we hit a power-of-two */
  while(MPIDI_Process.avail_contexts & (MPIDI_Process.avail_contexts-1))
    --MPIDI_Process.avail_contexts;
  MPID_assert_always(MPIDI_Process.avail_contexts);

#else /* (MPIU_THREAD_GRANULARITY != MPIU_THREAD_GRANULARITY_PER_OBJECT) */

  /* Only a single context is supported in the 'global' mpich lock mode.
   *
   * A multi-context application will always perform better with the
   * 'per object' mpich lock mode - regardless of whether async progress is
   * enabled or not. This is because all threads, application and async
   * progress, must acquire the single global lock which effectively serializes
   * the threads and negates any benefit of multiple contexts.
   *
   * This single context limitation removes code and greatly simplifies logic.
   */
  MPIDI_Process.avail_contexts = 1;

#endif

  TRACE_ERR ("Thread-level=%d, requested=%d\n", *threading, requested_thread_level);

#ifdef OUT_OF_ORDER_HANDLING
  numTasks  = *size;
  MPIDI_In_cntr = MPIU_Calloc0(numTasks, MPIDI_In_cntr_t);
  if(MPIDI_In_cntr == NULL)
    MPID_abort();
  MPIDI_Out_cntr = MPIU_Calloc0(numTasks, MPIDI_Out_cntr_t);
  if(MPIDI_Out_cntr == NULL)
    MPID_abort();
  memset((void *) MPIDI_In_cntr,0, sizeof(MPIDI_In_cntr_t));
  memset((void *) MPIDI_Out_cntr,0, sizeof(MPIDI_Out_cntr_t));
#endif

if (TOKEN_FLOW_CONTROL_ON)
  {
    #if TOKEN_FLOW_CONTROL
    int i;
    MPIDI_mm_init(numTasks,&MPIDI_Process.pt2pt.limits.application.eager.remote,&MPIDI_Process.mp_buf_mem);
    MPIDI_Token_cntr = MPIU_Calloc0(numTasks, MPIDI_Token_cntr_t);
    memset((void *) MPIDI_Token_cntr,0, (sizeof(MPIDI_Token_cntr_t) * numTasks));
    for (i=0; i < numTasks; i++)
      {
        MPIDI_Token_cntr[i].tokens=MPIDI_tfctrl_enabled;
      }
    #else
    MPID_assert_always(0);
    #endif
}

#ifdef MPIDI_TRACE
      int i; 
      MPIDI_Trace_buf = MPIU_Calloc0(numTasks, MPIDI_Trace_buf_t);
      if(MPIDI_Trace_buf == NULL) MPID_abort();
      memset((void *) MPIDI_Trace_buf,0, sizeof(MPIDI_Trace_buf_t));
      for (i=0; i < numTasks; i++) {
          MPIDI_Trace_buf[i].R=MPIU_Calloc0(N_MSGS, recv_status);
          if (MPIDI_Trace_buf[i].R==NULL) MPID_abort();
          MPIDI_Trace_buf[i].PR=MPIU_Calloc0(N_MSGS, posted_recv);
          if (MPIDI_Trace_buf[i].PR ==NULL) MPID_abort();
          MPIDI_Trace_buf[i].S=MPIU_Calloc0(N_MSGS, send_status);
          if (MPIDI_Trace_buf[i].S ==NULL) MPID_abort();
      }
#endif

  /* ----------------------------------- */
  /*  Create the communication contexts  */
  /* ----------------------------------- */
  TRACE_ERR("Creating %d contexts\n", MPIDI_Process.avail_contexts);
  pami_result_t rc = PAMI_ERROR;
  pami_configuration_t config[3];
  int  cfgval=0;
  config[cfgval].name = PAMI_CLIENT_CONST_CONTEXTS,
  config[cfgval].value.intval = 1;
  cfgval++;
#ifndef HAVE_ERROR_CHECKING
#ifdef OUT_OF_ORDER_HANDLING
  /* disable parameter checking in PAMI - fast library only */
  config[cfgval].name = PAMI_CONTEXT_CHECK_PARAM;
  config[cfgval].value.intval = 0;
  cfgval++;
#endif
#endif
  rc = PAMI_Context_createv(MPIDI_Client, config, cfgval, MPIDI_Context, MPIDI_Process.avail_contexts);

  MPID_assert_always(rc == PAMI_SUCCESS);
#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
  MPIDI_open_pe_extension();
#endif
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
  if ((immediate_max != NULL) && (im_max < *immediate_max))
    *immediate_max = im_max;
}


static void
MPIDI_PAMI_dispath_init()
{
#ifdef OUT_OF_ORDER_HANDLING
  {
    pami_configuration_t config;
    pami_result_t        rc = PAMI_ERROR;

    memset(&config, 0, sizeof(config));
    config.name = PAMI_DISPATCH_SEND_IMMEDIATE_MAX;
    rc = PAMI_Dispatch_query(MPIDI_Context[0], (size_t)0, &config, 1);
    if ( rc == PAMI_SUCCESS )
      {
        TRACE_ERR("PAMI_DISPATCH_SEND_IMMEDIATE_MAX=%d.\n", config.value.intval, rc);
        MPIDI_Process.pt2pt.limits_array[2] = config.value.intval;
      }
    else
      {
        TRACE_ERR((" Attention: PAMI_Client_query(DISPATCH_SEND_IMMEDIATE_MAX=%d) rc=%d\n", config.name, rc));
        MPIDI_Process.pt2pt.limits_array[2] = 256;
      }

    MPIDI_Process.pt2pt.limits_array[3] = MPIDI_Process.pt2pt.limits_array[2];
    MPIDI_Process.pt2pt.limits_array[6] = MPIDI_Process.pt2pt.limits_array[2];
    MPIDI_Process.pt2pt.limits_array[7] = MPIDI_Process.pt2pt.limits_array[2];
  }
#endif
  /* ------------------------------------ */
  /*  Set up the communication protocols  */
  /* ------------------------------------ */
  unsigned send_immediate_max_bytes = (unsigned) -1;
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Short,     &proto_list.Short,     &send_immediate_max_bytes);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_ShortSync, &proto_list.ShortSync, &send_immediate_max_bytes);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Eager,     &proto_list.Eager,     NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_RVZ,       &proto_list.RVZ,       NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Cancel,    &proto_list.Cancel,    NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_Control,   &proto_list.Control,   NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_WinCtrl,   &proto_list.WinCtrl,   NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_WinAccum,  &proto_list.WinAccum,  NULL);
  MPIDI_PAMI_dispath_set(MPIDI_Protocols_RVZ_zerobyte, &proto_list.RVZ_zerobyte, NULL);

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
  send_immediate_max_bytes -= (sizeof(MPIDI_MsgInfo) - 1);

  if (MPIDI_Process.pt2pt.limits.application.immediate.remote > send_immediate_max_bytes)
    MPIDI_Process.pt2pt.limits.application.immediate.remote = send_immediate_max_bytes;

  if (MPIDI_Process.pt2pt.limits.application.immediate.local > send_immediate_max_bytes)
    MPIDI_Process.pt2pt.limits.application.immediate.local = send_immediate_max_bytes;

  if (MPIDI_Process.pt2pt.limits.internal.immediate.remote > send_immediate_max_bytes)
    MPIDI_Process.pt2pt.limits.internal.immediate.remote = send_immediate_max_bytes;

  if (MPIDI_Process.pt2pt.limits.internal.immediate.local > send_immediate_max_bytes)
    MPIDI_Process.pt2pt.limits.internal.immediate.local = send_immediate_max_bytes;
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
  MPIDI_PAMI_context_init(threading, size);


  MPIDI_PAMI_dispath_init();


  if ( (*rank == 0) && (MPIDI_Process.verbose >= MPIDI_VERBOSE_SUMMARY_0) )
    {
      printf("MPIDI_Process.*\n"
             "  verbose               : %u\n"
             "  statistics            : %u\n"
             "  contexts              : %u\n"
             "  async_progress        : %u\n"
             "  context_post          : %u\n"
             "  pt2pt.limits\n"
             "    application\n"
             "      eager\n"
             "        remote, local   : %u, %u\n"
             "      short\n"
             "        remote, local   : %u, %u\n"
             "    internal\n"
             "      eager\n"
             "        remote, local   : %u, %u\n"
             "      short\n"
             "        remote, local   : %u, %u\n"
             "  rma_pending           : %u\n"
             "  shmem_pt2pt           : %u\n"
             "  disable_internal_eager_scale : %u\n"
             "  mp_buf_mem               : %u\n"
             "  is_token_flow_control_on : %u\n"
#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
             "  mp_infolevel : %u\n"
             "  mp_statistics: %u\n"
             "  mp_printenv  : %u\n"
             "  mp_interrupts: %u\n"
#endif
             "  optimized.collectives : %u\n"
             "  optimized.select_colls: %u\n"
             "  optimized.subcomms    : %u\n"
             "  mpir_nbc              : %u\n",
             MPIDI_Process.verbose,
             MPIDI_Process.statistics,
             MPIDI_Process.avail_contexts,
             MPIDI_Process.async_progress.mode,
             MPIDI_Process.perobj.context_post.requested,
             MPIDI_Process.pt2pt.limits_array[0],
             MPIDI_Process.pt2pt.limits_array[1],
             MPIDI_Process.pt2pt.limits_array[2],
             MPIDI_Process.pt2pt.limits_array[3],
             MPIDI_Process.pt2pt.limits_array[4],
             MPIDI_Process.pt2pt.limits_array[5],
             MPIDI_Process.pt2pt.limits_array[6],
             MPIDI_Process.pt2pt.limits_array[7],
             MPIDI_Process.rma_pending,
             MPIDI_Process.shmem_pt2pt,
             MPIDI_Process.disable_internal_eager_scale,
#if TOKEN_FLOW_CONTROL             
             MPIDI_Process.mp_buf_mem,
             MPIDI_Process.is_token_flow_control_on,
#else
             0,
             0,
#endif
#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
             MPIDI_Process.mp_infolevel,
             MPIDI_Process.mp_statistics,
             MPIDI_Process.mp_printenv,
             (MPIDI_Process.async_progress.mode != ASYNC_PROGRESS_MODE_DISABLED),
#endif
             MPIDI_Process.optimized.collectives,
             MPIDI_Process.optimized.select_colls,
             MPIDI_Process.optimized.subcomms,
             MPIDI_Process.mpir_nbc);
      switch (*threading)
        {
          case MPI_THREAD_MULTIPLE:
            printf("mpi thread level        : 'MPI_THREAD_MULTIPLE'\n");
            break;
          case MPI_THREAD_SERIALIZED:
            printf("mpi thread level        : 'MPI_THREAD_SERIALIZED'\n");
            break;
          case MPI_THREAD_FUNNELED:
            printf("mpi thread level        : 'MPI_THREAD_FUNNELED'\n");
            break;
          case MPI_THREAD_SINGLE:
            printf("mpi thread level        : 'MPI_THREAD_SINGLE'\n");
            break;
        }
      printf("MPIU_THREAD_GRANULARITY : '%s'\n",
             (MPIU_THREAD_GRANULARITY==MPIU_THREAD_GRANULARITY_PER_OBJECT)?"per object":"global");
#ifdef ASSERT_LEVEL
      printf("ASSERT_LEVEL            : %d\n", ASSERT_LEVEL);
#else
      printf("ASSERT_LEVEL            : not defined\n");
#endif
#ifdef MPICH2_LIBDIR
      printf("MPICH2_LIBDIR           : %s\n", MPICH2_LIBDIR);
#else
      printf("MPICH2_LIBDIR           : not defined\n");
#endif
      printEnvVars("MPICH_");
      printEnvVars("PAMID_");
      printEnvVars("PAMI_");
      printEnvVars("COMMAGENT_");
      printEnvVars("MUSPI_");
      printEnvVars("BG_");
    }
#ifdef MPIDI_BANNER
  if ((*rank == 0) && (MPIDI_Process.mp_infolevel >=1)) {
       char* buf = (char *) MPIU_Malloc(160);
       int   rc  = MPIDI_Banner(buf);
       if ( rc == 0 )
            fprintf(stderr, "%s\n", buf);
       else
            TRACE_ERR("mpid_banner() return code=%d task %d",rc,*rank);
       MPIU_Free(buf);
  }
#endif
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


  /* ------------------------------------------------------------------------------- */
  /*  Initialize the pami client to get the process rank; needed for env var output. */
  /* ------------------------------------------------------------------------------- */
  MPIDI_PAMI_client_init(&rank, &size, requested);

  /* ------------------------------------ */
  /*  Get new defaults from the Env Vars  */
  /* ------------------------------------ */
  MPIDI_Env_setup(rank, requested);

  /* ----------------------------- */
  /* Initialize messager           */
  /* ----------------------------- */
  if (MPIDI_Process.async_progress.mode == ASYNC_PROGRESS_MODE_TRIGGER)
  {
    /* The 'trigger' async progress mode requires MPI_THREAD_MULTIPLE.
     * Silently promote the thread level.
     *
     * See discussion in src/mpid/pamid/src/mpid_progress.h for more
     * information.
     */
    *provided = MPI_THREAD_MULTIPLE;
  }
  else
  {
    *provided = requested;
  }
#if (MPIDI_STATISTICS || MPIDI_PRINTENV)
   if (requested != MPI_THREAD_MULTIPLE)
       mpich_env->single_thread=1;
#endif
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
  MPIR_Comm_commit(MPIR_Process.comm_world);
  /* ------------------------------- */
  /* Initialize timer data           */
  /* ------------------------------- */
  MPID_Wtime_init();


  /* ------------------------------- */
  /* ???                             */
  /* ------------------------------- */
  *has_args = TRUE;
  *has_env  = TRUE;
#ifdef MPIDI_PRINTENV
  if (MPIDI_Process.mp_printenv) {
      MPIDI_Print_mpenv(rank,size);
  }
#endif
  return MPI_SUCCESS;
}


/*
 * \brief This is called by MPI to let us know that MPI_Init is done.
 */
int MPID_InitCompleted()
{
  MPIDI_NBC_init();
  MPIDI_Progress_init();
  return MPI_SUCCESS;
}

#if (MPIDI_PRINTENV || MPIDI_STATISTICS || MPIDI_BANNER)
void MPIDI_open_pe_extension() {
    int rc;
     /* open PE extension       */
     memset(&pe_extension,0, sizeof(pami_extension_t));
     rc = PAMI_Extension_open (MPIDI_Client, "EXT_pe_extension", &pe_extension);
     TRACE_ERR("PAMI_Extension_open: rc %d\n", rc);
     if (rc != PAMI_SUCCESS) {
         TRACE_ERR("ERROR open PAMI_Extension_open failed rc %d", rc);
         MPID_assert_always(rc == PAMI_SUCCESS);
     }
}


int MPIDI_Banner(char * bufPtr) {
    char  *cp, *level=NULL;
    char buf[30];
    char *ASC_time;
    time_t  ltime;
    char msgBuf[60];
    char type[64], ver_buf[64];
    struct  tm  *tmx,*tm1;

    /* Note: The _ibm_release_version will be expanded to a full string  */
    /*       ONLY IF this file is extracted from CMVC.                   */
    /*       If this file is cloned from GIT the the string will be just */
    /*       "%W%.                                                       */
    if ( strncmp(_ibm_release_version_, "%W", 2) ) {
       /* IBMPE's expanded version string has release name like ppe_rbarlx */
       /* and that '_' in the name is what we are looking for.             */
       /* BGQ's version string does not have a '_' in it.                  */
       level = strrchr(_ibm_release_version_, '_');
       if ( level ) {
          level -=3;

          /* The version string generated by CMVC during weekly build has a */
          /* date which is the time when the mpidi_platform.h file is last  */
          /* updated.  This date can be quite old and is better removed.    */
          bzero(ver_buf, sizeof(ver_buf));
          strncpy(ver_buf, level, sizeof(ver_buf)-1);
          if ( cp = strchr(ver_buf, ',') ) *cp = '\0';
       }
    }

#ifndef __32BIT__
    strcpy(type, "64bit (MPI over PAMI)");
#else
    strcpy(type, "32bit (MPI over PAMI)");
#endif

    sprintf(msgBuf,"MPICH2 library was compiled on");

    tmx=MPIU_Malloc(sizeof(struct tm));
    sprintf(buf,__DATE__" "__TIME__);

    if ((void *) NULL == strptime(buf, "%B %d %Y %T", tmx))
       return(1);

   /*  update isdst in tmx structure    */
    ltime=0;
    time(&ltime);
    tm1 = localtime(&ltime);
    tmx->tm_isdst=tm1->tm_isdst;

   /* localtime updates tm_wday in tmx structure  */
    ltime=mktime(tmx);
    tm1 = localtime(&ltime);
    tmx->tm_wday = tm1->tm_wday;
    ASC_time = asctime(tmx);

    if (level) {
       sprintf(bufPtr, "%s %s %s %s ", type, ver_buf, msgBuf, ASC_time);
    } else {
       sprintf(bufPtr, "%s %s %s ", type, msgBuf, ASC_time);
    }

    MPIU_Free(tmx);
    return MPI_SUCCESS;
}
#endif 


static inline void
static_assertions()
{
  MPID_assert_static(sizeof(void*) == sizeof(size_t));
  MPID_assert_static(sizeof(uintptr_t) == sizeof(size_t));
#ifdef __BGQ__
  MPID_assert_static(sizeof(MPIDI_MsgInfo) == 16);
  MPID_assert_static(sizeof(uint64_t) == sizeof(size_t));
#endif
}