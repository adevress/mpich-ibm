/*  (C)Copyright IBM Corp.  2007, 2008  */
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
 *   - Default is 1200 bytes.
 *
 * - PAMI_OPTRVZ -
 * - PAMI_OPTRZV - Determines the optimized rendezvous limit. Both options
 *   are identical.  This takes an argument, in bytes.  The
 *   optimized rendezvous protocol will be used if:
 *   eager_limit <= message_size < (eager_limit + PAMI_OPTRZV).
 *   For sending, one of three protocols will be used depending on the message
 *   size:  The eager protocol for small messages, the optimized rendezvous
 *   protocol for medium messages, and the default rendezvous protocol for
 *   large messages. The optimized rendezvous protocol generally has less
 *   latency than the default rendezvous protocol, but does not wait for a
 *   receive to be posted first.  Therefore, unexpected messages in this size
 *   range may be received, consuming storage until the receives are issued.
 *   The default rendezvous protocol waits for a receive to be posted first.
 *   Therefore, no unexpected messages in this size range will be received.
 *   The optimized rendezvous protocol also avoids filling injection fifos
 *   which can cause delays while larger fifos are allocated.  For example,
 *   alltoall on large subcommunicators with thread mode multiple will
 *   benefit from optimized rendezvous.
 *   - Default is 0 bytes, meaning that optimized rendezvous is not used.
 *
 * - PAMI_RMA_PENDING - Maximum outstanding RMA requests.
 *   Limits number of PAMI_Request objects allocated by MPI Onesided operations.
 *   - Default is 1000.
 *
 ***************************************************************************
 *                               High-level Options                        *
 ***************************************************************************
 *
 * - PAMI_TOPOLOGY - Turns on optimized topology
 *   creation functions when using MPI_Cart_create with the
 *   reorder flag.  We attempt to create communicators similar
 *   to those requested, that match physical hardware as much
 *   as possible.  Possible values:
 *   - 0 - Optimized topology creation functions are not used.
 *   - 1 - Optimized topology creation functions are used.
 *   - Default is 1.
 *
 * - PAMI_COLLECTIVE -
 * - PAMI_COLLECTIVES - Controls whether optimized collectives are used.
 *   Possible values:
 *   - 0 - Optimized collectives are not used.
 *   - 1 - Optimized collectives are used.
 *   - NOTREE.  Only collective network optimizations are not used.
 *   - Default is 1.
 *
 * - PAMI_INTERRUPT -
 * - PAMI_INTERRUPTS - Turns on interrupt driven communications. This
 *   can be beneficial to some applications and is required if you are
 *   using Global Arrays or ARMCI.  (They force this on, regardless of
 *   the environment setting).  Possible values:
 *   - 0 - Interrupt driven communications is not used.
 *   - 1 - Interrupt driven communications is used.
 *   - Default is 0.
 *
 *
 * - PAMI_VERBOSE - Increases the amount of information dumped during an
 *   MPI_Abort() call.  Possible values:
 *   - 0 - No additional information is dumped.
 *   - 1 - Additional information is dumped.
 *   - Default is 0.
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
 * - PAMI_SAFEALLGATHER - The optimized allgather protocols require
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
 * - PAMI_SAFEALLREDUCE - The direct put allreduce bandwidth optimization
 *   protocols require the send and recv buffers to be 16-byte aligned on all
 *   nodes. Unfortunately, root's buffer can be misaligned from the rest of
 *   the nodes. Therefore, by default we must do an 2-byte allreduce before dput
 *   allreduces to ensure all nodes have the same alignment. If you know all of
 *   your buffers are 16 byte aligned, turning on this option will skip the
 *   allreduce step and improve performance. Setting this to "Y" is highly
 *   recommended for the majority of applications. We have not seen an
 *   application yet where "N" was unsafe, however it is technically incorrect
 *   to assume the alignment is the same on all nodes and therefore this must
 *   be set to "N" by default.
 *   Possible values:
 *   - N - Perform the allreduce
 *   - Y - Bypass the allreduce. If you have mismatched alignment, you will
 *         likely get weird behavior (hangs/crashes) or asserts.
 *   - Default is N.
 *
 * - PAMI_SAFEBCAST - The rectangle direct put bcast bandwidth optimization
 *   protocol requires the bcast buffers to be 16-byte aligned on all nodes.
 *   Unfortunately, you can have root's buffer be misaligned from the rest of
 *   the nodes. Therefore, by default we must do an allreduce before dput
 *   bcasts to ensure all nodes have the same alignment. If you know all of
 *   your buffers are 16 byte aligned, turning on this option will skip the
 *   allreduce step. Setting this to "Y" is highly recommended for the majority
 *   of applications. We have seen one mixed C/Fortran code where the alignment
 *   on root's buffer was different that on nonroot nodes. However, that is the
 *   only application we've seen that had a problem.
 *   Possible values:
 *   - N - Perform the allreduce
 *   - Y - Bypass the allreduce. If you have mismatched alignment, you will
 *         likely get weird behavior or asserts.
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
 *   employed by bcast, allreduce, allgather(v), and scatterv. This option
 *   is independant from PAMI_ALLREDUCE. Possible values are:
 *   - MPIDO - Just call MPIDO_Allreduce and let the existing logic determine
 *             what allreduce to use. This can be expensive, but it is the only
 *             guaranteed option, and it is the only way to get MPICH for the
 *             pre-allreduce.
 *   - SRECT - Use the short rectangle protocol. If you set this and do not
 *             have a rectangular (sub)communicator, you'll get the MPIDO
 *             option. This is the default selection for rectangular subcomms.
 *   - SBINOM - Use the short binomial protocol. This is the default for
 *             irregular subcomms.
 *   - ARING - Use the async rectangular ring protocol
 *   - ARECT - Use the async rectangle protocol
 *   - ABINOM - Use the async binomial protocol
 *   - RING - Use the rectangular ring protocol
 *   - RECT - Use the rectangle protocol
 *   - TDPUT - Use the tree dput protocol. This is the default in virtual
 *             node mode on MPI_COMM_WORLD
 *   - TREE - Use the tree. This is the default in SMP mode on MPI_COMM_WORLD
 *   - DPUT - Use the rectangular direct put protocol
 *   - PIPE - Use the pipelined CCMI tree protocol
 *   - BINOM - Use a binomial protocol
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
 * - PAMI_NUMCOLORS - Controls how many colors are used for rectangular
 *   broadcasts.  See PAMI_BCAST for more information. Possible values:
 *   - 0 - Let the lower-level messaging system decide.
 *   - 1, 2, or 3.
 *   - Default is 0.
 *
 * - PAMI_ASYNCCUTOFF - Changes the cutoff point between
 *   asynchronous and synchronous rectangular/binomial broadcasts.
 *   This can be highly application dependent.
 *   - Default is 128k.
 *
 * - PAMI_ALLTOALL_PREMALLOC -
 * - PAMI_ALLTOALLV_PREMALLOC -
 * - PAMI_ALLTOALLW_PREMALLOC - These are equivalent options.
 *   The alltoall protocols require 6 arrays to be setup
 *   before communication begins.  These 6 arrays are each of size
 *   (comm_size) so can be sizeable on large machines.  If your application
 *   does not use alltoall, or you need as much memory as possible, you can
 *   turn off pre-allocating these arrays.  By default, we allocate them
 *   once per communicator creation.  There is only one set, regardless of
 *   whether you are using alltoall, alltoallv, or alltoallw. Turning this off
 *   can free up a sizeable amount of per-communicator memory, especially if
 *   you aren't using alltoall(vw) on those communicators.
 *   Possible values:
 *   - Y - Premalloc the arrays.
 *   - N - Malloc and free on every alltoall operation.
 *   - Default is Y.
 *
 * - PAMI_ALLREDUCE_REUSE_STORAGE - This allows the lower
 *   level protcols to reuse some storage instead of malloc/free
 *   on every allreduce call.
 *   Possible values:
 *   - Y - Does not malloc/free on every allreduce call.  This improves
 *         performance, but retains malloc'd memory between allreduce calls.
 *   - N - Malloc/free on every allreduce call.  This frees up storage for
 *         use between allreduce calls.
 *   - Default is Y.
 *
 * - PAMI_ALLREDUCE_REUSE_STORAGE_LIMIT - This specifies the upper limit
 *   of storage to save and reuse across allreduce calls when
 *   PAMI_ALLREDUCE_REUSE_STORAGE is set to Y. (This environment variable
 *   is processed within the PAMI_Allreduce_register() API, not in
 *   MPIDI_Env_setup().)
 *   - Default is 1048576 bytes.
 *
 * - PAMI_REDUCE_REUSE_STORAGE - This allows the lower
 *   level protcols to reuse some storage instead of malloc/free
 *   on every reduce call.
 *   Possible values:
 *   - Y - Does not malloc/free on every reduce call.  This improves
 *         performance, but retains malloc'd memory between reduce calls.
 *   - N - Malloc/free on every reduce call.  This frees up storage for
 *         use between reduce calls.
 *   - Default is Y.
 *
 * - PAMI_REDUCE_REUSE_STORAGE_LIMIT - This specifies the upper limit
 *   of storage to save and reuse across allreduce calls when
 *   PAMI_REDUCE_REUSE_STORAGE is set to Y. (This environment variable
 *   is processed within the PAMI_Reduce_register() API, not in
 *   MPIDI_Env_setup().)
 *   - Default is 1048576 bytes.
 *
 ***************************************************************************
 *                      Specific Collectives Switches                      *
 ***************************************************************************
 *
 * - PAMI_ALLGATHER - Controls the protocol used for allgather.
 Possible values:
 *   - MPICH - Turn off all optimizations for allgather and use the MPICH
 *     point-to-point protocol. This can be very bad on larger partitions.
 *   - ALLREDUCE - Use a collective network based allreduce.  This is the
 *     default on MPI_COMM_WORLD for smaller messages.
 *   - ALLTOALL - Use an all-to-all based algorithm.  This is the default on
 *     irregular communicators.  It works very well for larger messages,
 *     especially on larger communicators.
 *   - BCAST - Use a broadcast.  This will use a collective network broadcast
 *     on MPI_COMM_WORLD. It is the default for larger messages on
 *     MPI_COMM_WORLD.  This can work well on rectangular subcommunicators for
 *     medium-sized messages.
 *   - ASYNC - Use an async broadcast.  This will use a series of asynchronous
 *     broadcasts (one per node in the allgather call) to do the allgather.
 *     After every {PAMI_NUMREQUESTS}, a barrier is called. This is the default
 *     option for small messages on rectangular or irregular subcommunicators.
 *   - Default varies based on the communicator.  See above.
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLGATHER=NOASYNC will prevent the use of async bcast
 *   based allgathers.
 *
 * - PAMI_ALLGATHERV - Controls the protocol used for allgatherv.
 Possible values:
 *   - MPICH - Turn off all optimizations for allgather and use the MPICH
 *     point-to-point protocol. This can be very bad on larger partitions.
 *   - ALLREDUCE - Use a collective network based allreduce.  This is the
 *     default on MPI_COMM_WORLD for smaller messages.
 *   - ALLTOALL - Use an all-to-all based algorithm.  This is the default on
 *     irregular communicators.  It works very well for larger messages and
 *     larger communicators.
 *   - BCAST - Use a broadcast.  This will use a series of broadcasts.
 *     It is the default for larger messages on MPI_COMM_WORLD.
 *     This can work well on rectangular subcommunicators for medium to
 *     large messages (where it is the default).
 *   - ASYNC - Use a series of async broadcast, one per node. After every
 *     {PAMI_NUMREQUESTS} we do a barrier. This is the default for small
 *     messages on rectangular or irregular subcommunicators.
 *   - Default varies based on the communicator.  See above.
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLGATHERV=NOASYNC will prevent the use of async bcast
 *   based allgathervs.
 *
 * - PAMI_ALLREDUCE - Controls the protocol used for allreduce.
 *   Possible values:
 *   - MPICH - Turn off all optimizations for allreduce and use the MPICH
 *     point-to-point protocol.
 *   - RING - Use a rectangular ring protocol.  This is the default for
 *     rectangular subcommunicators.
 *   - RECT - Use a rectangular/binomial protocol.  This is off by default.
 *   - BINOM - Use a binomial protocol.  This is the default for irregular
 *     subcommunicators when not using floating point for MPI_SUM/MPI_PROD
 *     operations. The protocol can produce different results on different
 *     nodes because of floating point non-associativity. This can be
 *     overridden with PAMI_ALLREDUCE_ND=1.
 *   - TREE - Use the collective network.  This is the default for
 *       MPI_COMM_WORLD and duplicates of MPI_COMM_WORLD in MPI_THREAD_SINGLE
 *       mode.
 *   - PIPE - Use the pipelined CCMI collective network protocol. This is off
 *       by default.
 *   - ARECT - Enable the asynchronous rectangle protocol
 *   - ABINOM - Enable the async binomial protocol
 *   - ARING - Enable the asynchronous version of the rectangular ring protocol.
 *   - TDPUT - Use the tree+direct put protocol. This is the default for VNM
 *       on MPI_COMM_WORLD
 *   - SR - Use the short async rectangular protocol. This protocol only works
 *       with up to 208 bytes. It is very fast for short messages however.
 *   - SB - Use the short async binomial protocol. This protocol only works
 *       with up to 208 bytes. It is very fast for short messages on irregular
 *       subcommunicators. However, this protocol can produce slightly
 *       different results on different nodes because of floating point
 *       addition being nonassociative. This is still useable on integers
 *       and floating point operations beyond MPI_SUM and MPI_PROD. To get this
 *       protocol for MPI_SUM and MPI_PROD, use the PAMI_ALLREDUCE_ND=1 option.
 *   - DPUT - Use the rectangular direct put protocol. This is the default for
 *       large messages on rectangular subcomms and MPI_COMM_WORLD
 *   - TDPUT - Use the tree plus direct put protocol. Only usable on
 *       MPI_COMM_WORLD.
 *   - GLOBAL - Use the global tree protocol. Only usable on MPI_COMM_WORLD.
 *   - Default varies based on the communicator and message size and if the
 *     operation/datatype pair is supported on the tree hardware.
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_ALLREDUCE=NOTREE will prevent the use of the tree
 *   protocol for allreduce.
 *
 * - PAMI_ALLTOALL -
 * - PAMI_ALLTOALLV -
 * - PAMI_ALLTOALLW - Controls the protocol used for
 *   alltoall/alltoallv/alltoallw.  Possible values:
 *   - MPICH - Turn off all optimizations and use the MPICH
 *     point-to-point protocol.
 *   - Default (or if anything else is specified)
 *     is to use an optimized alltoall/alltoallv/alltoallw.
 *
 * - PAMI_BARRIER - Controls the protocol used for barriers.
 *   Possible values:
 *   - MPICH - Turn off optimized barriers and use the MPICH
 *     point-to-point protocol.
 *   - BINOM - Use the binomial barrier.  This is the default for all
 *     subcommunicators.
 *   - GI - Use the GI network.  This is the default for MPI_COMM_WORLD and
 *     duplicates of MPI_COMM_WORLD in MPI_THREAD_SINGLE mode.
 *   - CCMI - Use the CCMI GI network protocol.  This is off by
 *     default.
 *   - Default varies based on the communicator.  See above.
 *   NOTE: Individual options may be turned off by prefixing them with "NO".
 *   For example, PAMI_BARRIER=NOGI will prevent the use of the global
 *   interrupts.
 *
 * - PAMI_BCAST - Controls the protocol used for broadcast.  Possible values:
 *   - MPICH - Turn off all optimizations for broadcast and use the MPICH
 *     point-to-point protocol.
 *   - TREE - Use the collective network. This is the default on MPI_COMM_WORLD
 *     and duplicates of MPI_COMM_WORLD in MPI_THREAD_SINGLE mode.  This
 *     provides the fastest possible broadcast for small messages.
 *   - CCMI - Use the CCMI collective network protocol.  This is off by default.
 *   - CDPUT - Use the CCMI collective network protocol with
 *     DPUT. This is off by default.
 *   - AR - Use the asynchronous rectangle protocol. This is the default
 *     for small messages on rectangular subcommunicators.  The cutoff between
 *     async and sync can be controlled with PAMI_ASYNCCUTOFF.
 *   - AB - Use the asynchronous binomial protocol.  This is the default
 *     for irregularly shaped subcommunicators.  The cutoff between
 *     async and sync can be controlled with PAMI_ASYNCCUTOFF.
 *   - RECT - Use the rectangle protocol. This is the default for rectangularly
 *     shaped subcommunicators for large messages.  This disables the
 *     asynchronous protocol.
 *   - BINOM - Use the binomial protocol. This is the default for irregularly
 *     shaped subcommunicators for large messages.  This disables the
 *     asynchronous protocol.
 *   - Default varies based on the communicator.  See above.
 *   NOTE: Any option may be prefixed with "NO" to turn OFF just that protocol.
 *   For example, PAMI_BCAST=NOTREE will prevent us from using the tree for
 *   broadcasts.
 *
 * - PAMI_GATHER - Controls the protocol used for gather.
 *   Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Default (or if anything else is specified)
 *     is to use a reduce-based algorithm for larger message sizes.
 *
 * - PAMI_REDUCE - Controls the protocol used for reduce.
 *   Possible values:
 *   - MPICH - Turn off all optimizations and use the MPICH point-to-point
 *     protocol.
 *   - RECT - Use a rectangular/binomial protocol.  This is the default for
 *     rectangular subcommunicators.
 *   - BINOM - Use a binomial protocol.  This is the default for irregular
 *     subcommunicators.
 *   - TREE - Use the collective network.  This is the default for
 *     MPI_COMM_WORLD and duplicates of MPI_COMM_WORLD in MPI_THREAD_SINGLE
 *     mode.
 *   - CCMI - Use the CCMI collective network protocol.  This is off by default.
 *   - ALLREDUCE - Use an allreduce protocl. This is off by default but can
 *     actually provide better results than a reduce at the expense of some
 *     application memory.
 *   - Default varies based on the communicator.  See above.
 *   NOTE: Any option may be prefixed with "NO" to turn OFF just that protocol.
 *   For example, PAMI_REDUCE=NOTREE will prevent us from using the tree for
 *   reduce.
 *
 * - PAMI_REDUCESCATTER - Controls the protocol used for reduce_scatter
 *   operations.  The options for PAMI_SCATTERV and PAMI_REDUCE can change the
 *   behavior of reduce_scatter.  Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Default (or if anything else is specified)
 *     is to use an optimized reduce followed by an optimized scatterv.
 *     This works well for larger messages.
 *
 * - PAMI_SCATTER - Controls the protocol used for scatter.
 *   Possible values:
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Default (or if anything else is specified)
 *     is to use a broadcast-based scatter at a 2k or larger
 *     message size.
 *
 * - PAMI_SCATTERV - Controls the protocol used for scatterv.
 *   Possible values:
 *   - ALLTOALL - Use an all-to-all based protocol when the
 *     message size is above 2k. This is optimal for larger messages
 *     and larger partitions.
 *   - BCAST - Use a broadcast-based scatterv.  This works well
 *     for small messages.
 *   - MPICH - Use the MPICH point-to-point protocol.
 *   - Default is ALLTOALL.
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
  if (MPIDI_Process.verbose >= 2)
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
    char *names[] = {"PAMI_MAXCONTEXTS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.avail_contexts);
    TRACE_ERR("MPIDI_Process.avail_contexts=%u\n", MPIDI_Process.avail_contexts);
  }

  /* Do not use the PAMI_Context_post interface; call the work function directly.
   * As coded, this has the side-effect of only using a single context. */
  {
    char *names[] = {"PAMI_CONTEXT_POST", NULL};
    ENV_Unsigned(names, &MPIDI_Process.context_post);
    TRACE_ERR("MPIDI_Process.context_post=%u\n", MPIDI_Process.context_post);
  }
  /* "Globally" set the optimization flag for low-level collectives in geometry creation.
   * This is probably temporary. metadata should set this flag likely.
   */
  {
   char *names[] = {"PAMI_OPTIMIZED_SUBCOMMS", NULL};
   ENV_Unsigned(names, &MPIDI_Process.optimized_subcomms);
   TRACE_ERR("MPIDI_Process.optimized_subcomms=%u\n", MPIDI_Process.optimized_subcomms);
  }

#ifdef USE_PAMI_COMM_THREADS
  /* Enable/Disable commthreads for asynchronous communication. */
  {
    MPIDI_Process.comm_threads = 1;
    char *names[] = {"PAMI_COMM_THREADS", NULL};
    ENV_Unsigned(names, &MPIDI_Process.comm_threads);
    TRACE_ERR("MPIDI_Process.comm_threads=%u\n", MPIDI_Process.comm_threads);
  }
#endif

  /* Determine eager limit */
  {
    char* names[] = {"PAMI_RVZ", "PAMI_RZV", "PAMI_EAGER", NULL};
    ENV_Unsigned(names, &MPIDI_Process.eager_limit);
  }

  /* Set the maximum number of outstanding RDMA requests */
  {
    char* names[] = {"PAMI_RMA_PENDING", NULL};
    ENV_Unsigned(names, &MPIDI_Process.rma_pending);
  }

  /* Set the status of the optimized topology functions */
  {
    char* names[] = {"PAMI_TOPOLOGY", NULL};
    ENV_Unsigned(names, &MPIDI_Process.optimized.topology);
  }

  /* Set the status of the optimized collectives */
  {
    char* names[] = {"PAMI_COLLECTIVE", "PAMI_COLLECTIVES", NULL};
    ENV_Unsigned(names, &MPIDI_Process.optimized.collectives);
    TRACE_ERR("MPIDI_Process.optimized.collectives=%u\n", MPIDI_Process.optimized.collectives);
  }
}
