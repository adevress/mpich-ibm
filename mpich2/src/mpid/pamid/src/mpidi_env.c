/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/mpidi_env.c
 * \brief Read Env Vars
 */

#include <mpidimpl.h>

/** \page env_vars Environment Variables
 ***************************************************************************
 *                             Pt2Pt Options                               *
 ***************************************************************************
 *
 * - PAMI_EAGER -
 * - PAMI_RZV -
 * - PAMI_RVZ - Sets the cutoff for the switch to the rendezvous protocol.
 *   All three options are identical.  This takes an argument, in bytes,
 *   to switch from the eager protocol to the rendezvous protocol for point-to-point
 *   messaging.  Increasing the limit might help for larger partitions
 *   and if most of the communication is nearest neighbor.
 *   - Default is 2049 bytes.
 *
 * - PAMI_EAGER_LOCAL -
 * - PAMI_RZV_LOCAL -
 * - PAMI_RVZ_LOCAL - Sets the cutoff for the switch to the rendezvous protocol
 *   when the destination rank is local. All three options are identical.  This
 *   takes an argument, in bytes, to switch from the eager protocol to the
 *   rendezvous protocol for point-to-point messaging.  The default value
 *   effectively disables the eager protocol for local transfers.
 *   - Default is 0 bytes.
 *
 * - PAMI_RMA_PENDING - Maximum outstanding RMA requests.
 *   Limits number of PAMI_Request objects allocated by MPI Onesided operations.
 *   - Default is 1000.
 *
 * - PAMI_SHMEM_PT2PT - Determines if intranode point-to-point communication will
 *   use the optimized shared memory protocols.
 *   - Default is 1.
 ***************************************************************************
 *                               High-level Options                        *
 ***************************************************************************
 *
 * - PAMI_COLLECTIVE -
 * - PAMI_COLLECTIVES - Controls whether optimized collectives are used.
 *   Possible values:
 *   - 0 - Optimized collectives are not used.
 *   - 1 - Optimized collectives are used.
 *   If this is set to 0, only MPICH point-to-point based collectives will be used.
 *
 * - PAMI_COLLECTIVE_SELECTION -
 * - PAMI_COLLECTIVES_SELECTION - Turns on optimized collective selection. If this
 *   is not on, only the generic PGAS "always works" algorithms will be selected. This
 *   can still be better than PAMI_COLLECTIVES=0. Additionally, setting this off
 *   still allows environment variable selection of individual collectives protocols.
 *   - 0 - Optimized collective selection is not used.
 *   - 1 - Optimized collective selection is used. (default)
 *
 * - PAMI_VERBOSE - Increases the amount of information dumped during an
 *   MPI_Abort() call and during varoius MPI function calls.  Possible values:
 *   - 0 - No additional information is dumped.
 *   - 1 - Useful information is printed from MPI_Init(). This option does
 *   NOT impact performance (other than a tiny bit during MPI_Init()) and
 *   is highly recommended for all applications. It will display which PAMI_
 *   and BG_ env vars were used. However, it does NOT guarantee you typed the
 *   env vars in correctly :)
 *   - 2 - This turns on a lot of verbose output for collective selection,
 *   including a list of which protocols are viable for each communicator
 *   creation. This can be a lot of output, but typically only at 
 *   communicator creation. Additionally, if PAMI_STATISTICS are on,
 *   queue depths for point-to-point operations will be printed for each node 
 *   during MPI_Finalize();
 *   - 3 - This turns on a lot of per-node information (plus everything 
 *   at the verbose=2 level). This can be a lot of information and is
 *   rarely recommended.
 *   - Default is 0. However, setting to 1 is recommended.
 *
 * - PAMI_STATISTICS - Turns on statistics printing for the message layer
 *   such as the maximum receive queue depth.  Possible values:
 *   - 0 - No statistics are printedcmf_bcas.
 *   - 1 - Statistics are printed.
 *   - Default is 0.
 *
 ***************************************************************************
 ***************************************************************************
 **                           Collective Options                          **
 ***************************************************************************
 ***************************************************************************
 *
 ***************************************************************************
 *                            "Safety" Options                             *
 ***************************************************************************
 *
 * - PAMI_SAFEALLGATHER - Some optimized allgather protocols require
 *   contiguous datatypes and similar datatypes on all nodes.  To verify
 *   this is true, we must do an allreduce at the beginning of the
 *   allgather call.  If the application uses "well behaved" datatypes, you can
 *   set this option to skip over the allreduce.  This is most useful in
 *   irregular subcommunicators where the allreduce can be expensive and in
 *   applications calling MPI_Allgather() with simple/regular datatypes.
 *   Datatypes must also be 16-byte aligned to use the optimized
 *   broadcast-based allgather for larger allgather sendcounts. See
 *   PAMI_PREALLREDUCE for more options
 *   Possible values:
 *   - N - Perform the preallreduce.
 *   - Y - Skip the preallreduce.  Setting this with "unsafe" datatypes will
 *         yield unpredictable results, usually hangs.
 *   - Default is N.
 *
 * - PAMI_SAFEALLGATHERV - The optimized allgatherv protocols require
 *   contiguous datatypes and similar datatypes on all nodes.  Allgatherv
 *   also requires continuous displacements.  To verify
 *   this is true, we must do an allreduce at the beginning of the
 *   allgatherv call.  If the application uses "well behaved" datatypes and
 *   displacements, you can set this option to skip over the allreduce.
 *   This is most useful in irregular subcommunicators where the allreduce
 *   can be expensive and in applications calling MPI_Allgatherv() with
 *   simple/regular datatypes.
 *   Datatypes must also be 16-byte aligned to use the optimized
 *   broadcast-based allgather for larger allgather sendcounts. See
 *   PAMI_PREALLREDUCE for more options
 *   Possible values:
 *   - N - Perform the allreduce.
 *   - Y - Skip the allreduce.  Setting this with "unsafe" datatypes will
 *         yield unpredictable results, usually hangs.
 *   - Default is N.
 *
 * - PAMI_SAFESCATTERV - The optimized scatterv protocol requires
 *   contiguous datatypes and similar datatypes on all nodes.  It
 *   also requires continuous displacements.  To verify
 *   this is true, we must do an allreduce at the beginning of the
 *   scatterv call.  If the application uses "well behaved" datatypes and
 *   displacements, you can set this option to skip over the allreduce.
 *   This is most useful in irregular subcommunicators where the allreduce
 *   can be expensive. We have seen more applications with strange datatypes
 *   passed to scatterv than allgather/allgatherv/bcast/allreduce, so it is
 *   more likely you need to leave this at the default setting. However, we
 *   encourage you to try turning this off and seeing if your application
 *   completes successfully.
 *   Possible values:
 *   - N - Perform the allreduce.
 *   - Y - Skip the allreduce.  Setting this with "unsafe" datatypes will
 *         yield unpredictable results, usually hangs.
 *   - Default is N.
 *
 * - PAMI_PREALLREDUCE - Controls the protocol used for the pre-allreducing
 *   employed by allgather(v), and scatterv. This option
 *   is independant from PAMI_ALLREDUCE. The available protocols can be 
 *   determined with PAMI_VERBOSE=2. If collective selection is on, we will
 *   attempt to use the fastest protocol for the given communicator.
 *
 ***************************************************************************
 *                       General Collectives Options                       *
 ***************************************************************************
 *
 * - PAMI_NUMREQUESTS - Sets the number of outstanding asynchronous
 *   broadcasts to have before a barrier is called.  This is mostly
 *   used in allgather/allgatherv using asynchronous broadcasts.
 *   Higher numbers can help on larger partitions and larger
 *   message sizes. This is also used for asynchronous broadcasts.
 *   After every {PAMI_NUMREQUESTS} async bcasts, the "glue" will call
 *   a barrier. See PAMI_BCAST and PAMI_ALLGATHER(V) for more information
 *   - Default is 32.
 *
 *
 ***************************************************************************
 *                      Specific Collectives Switches                      *
 ***************************************************************************
 *
 * - PAMI_ALLGATHER - Controls the protocol used for allgather.
 *   Possible values:
 *   - MPICH - Turn off all optimizations for allgather and use the MPICH
 *     point-to-point protocol. This can be very bad on larger partitions.
 *  - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLGATHER=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_ALLGATHERV - Controls the protocol used for allgatherv.
 *   Possible values:
 *   - MPICH - Turn off all optimizations for allgather and use the MPICH
 *     point-to-point protocol. This can be very bad on larger partitions.
 *  - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLGATHERV=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_ALLREDUCE - Controls the protocol used for allreduce.
 *   Possible values:
 *   - MPICH - Turn off all optimizations for allreduce and use the MPICH
 *     point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   Note: Many allreduce algorithms are in the "must query" category and 
 *   might or might not be available for a specific callsite.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLREDUCE=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_ALLTOALL -
 * - PAMI_ALLTOALLV - Controls the protocol used for
 *   alltoall/alltoallv  Possible values:
 *   - MPICH - Turn off all optimizations and use the MPICH
 *     point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLTOALL=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 *
 * - PAMI_BARRIER - Controls the protocol used for barriers.
 *   Possible values:
 *   - MPICH - Turn off optimized barriers and use the MPICH
 *     point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_BARRIER=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_BCAST - Controls the protocol used for broadcast.  Possible values:
 *   - MPICH - Turn off all optimizations for broadcast and use the MPICH
 *     point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_BCAST=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_GATHER - Controls the protocol used for gather.
 *   Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_GATHER=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_REDUCE - Controls the protocol used for reduce.
 *   Possible values:
 *   - MPICH - Turn off all optimizations and use the MPICH point-to-point
 *     protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_REDUCE=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_REDUCESCATTER - Controls the protocol used for reduce_scatter
 *   operations.  The options for PAMI_SCATTERV and PAMI_REDUCE can change the
 *   behavior of reduce_scatter.  Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_REDUCESCATTER=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_SCATTER - Controls the protocol used for scatter.
 *   Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_SCATTER=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 * - PAMI_SCATTERV - Controls the protocol used for scatterv.
 *   Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Other options can be determined by running with PAMI_VERBOSE=2.
 *   TODO: Implement the NO options
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_SCATTERV=NO{specific protocol name} will prevent the 
 *   use of the specified protocol.
 *
 *
 */

#define ENV_Unsigned(a, b) ENV_Unsigned__(a, b, #b)
static inline void
ENV_Unsigned__(char* name[], unsigned* val, char* string)
{
  char * env;

  unsigned i=0;
  for (;; ++i) {
    if (name[i] == NULL)
      return;
    env = getenv(name[i]);
    if (env != NULL)
      break;
  }

  *val = atoi(env);
  if (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL)
    fprintf(stderr, "%s = %u\n", string, *val);
}

/** \brief Checks the Environment variables at initialization and stores the results */
void
MPIDI_Env_setup()
{
  /* Set the verbosity level.  When set, this will print information at finalize. */
  {
    char* names[] = {"PAMI_VERBOSE", NULL};
    ENV_Unsigned(names, &MPIDI_Process.verbose);
  }

  /* Enable statistics collection. */
  {
    char* names[] = {"PAMI_STATISTICS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.statistics);
  }

  /* Set the upper-limit of number of PAMI Contexts. */
  {
    char *names[] = {"PAMI_MAXCONTEXT", "PAMI_MAXCONTEXTS", "PAMI_MAX_CONTEXT", "PAMI_MAX_CONTEXTS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.avail_contexts);
    TRACE_ERR("MPIDI_Process.avail_contexts=%u\n", MPIDI_Process.avail_contexts);
  }

  /* Do not use the PAMI_Context_post interface; call the work function directly.
   * As coded, this has the side-effect of only using a single context. */
  {
    char *names[] = {"PAMI_CONTEXT_POST", "PAMI_CONTEXTPOST", NULL};
    ENV_Unsigned(names, &MPIDI_Process.context_post);
    TRACE_ERR("MPIDI_Process.context_post=%u\n", MPIDI_Process.context_post);
  }

  /* "Globally" set the optimization flag for low-level collectives in geometry creation.
   * This is probably temporary. metadata should set this flag likely.
   */
  {
    char *names[] = {"PAMI_OPTIMIZED_SUBCOMMS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.optimized.subcomms);
    TRACE_ERR("MPIDI_Process.optimized.subcomms=%u\n", MPIDI_Process.optimized.subcomms);
  }

#if USE_PAMI_COMM_THREADS
  /* Enable/Disable commthreads for asynchronous communication. */
  {
    char *names[] = {"PAMI_COMMTHREAD", "PAMI_COMMTHREADS", "PAMI_COMM_THREAD", "PAMI_COMM_THREADS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.commthreads_enabled);
    TRACE_ERR("MPIDI_Process.commthreads_enabled=%u\n", MPIDI_Process.commthreads_enabled);
  }
#endif

  /* Determine short limit */
  {
    char* names[] = {"PAMI_SHORT", "MP_S_SHORT_LIMIT", NULL};
    ENV_Unsigned(names, &MPIDI_Process.short_limit);
  }

  /* Determine eager limit */
  {
    char* names[] = {"PAMI_RVZ", "PAMI_RZV", "PAMI_EAGER", "MP_EAGER_LIMIT", NULL};
    ENV_Unsigned(names, &MPIDI_Process.eager_limit);
  }

  /* Determine 'local' eager limit */
  {
    char* names[] = {"PAMI_RVZ_LOCAL", "PAMI_RZV_LOCAL", "PAMI_EAGER_LOCAL", "MP_EAGER_LIMIT_LOCAL", NULL};
    ENV_Unsigned(names, &MPIDI_Process.eager_limit_local);
  }

  /* Set the maximum number of outstanding RDMA requests */
  {
    char* names[] = {"PAMI_RMA_PENDING", "MP_RMA_PENDING", NULL};
    ENV_Unsigned(names, &MPIDI_Process.rma_pending);
  }

  /* Set the status of the optimized collectives */
  {
    char* names[] = {"PAMI_COLLECTIVE", "PAMI_COLLECTIVES", NULL};
    ENV_Unsigned(names, &MPIDI_Process.optimized.collectives);
    TRACE_ERR("MPIDI_Process.optimized.collectives=%u\n", MPIDI_Process.optimized.collectives);
  }
   /* Set the status for optimized selection of collectives */
   {
      char* names[] = {"PAMI_COLLECTIVE_SELECTION", "PAMI_COLLECTIVES_SELECTION", NULL};
      ENV_Unsigned(names, &MPIDI_Process.optimized.select_colls);
      TRACE_ERR("MPIDI_Process.optimized.select_colls=%u\n", MPIDI_Process.optimized.select_colls);
   }


  /* Set the status of the optimized shared memory point-to-point functions */
  {
    char* names[] = {"PAMI_SHMEM_PT2PT", "MP_SHMEM_PT2PT", NULL};
    ENV_Unsigned(names, &MPIDI_Process.shmem_pt2pt);
  }
}
