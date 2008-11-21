/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
#include "mpidimpl.h" /* FIXME this is including a ch3 include file!
                         Implement these functions at the device level. */
#include "pmi.h"
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif

char MPIU_hostname[MAX_HOSTNAME_LEN] = "_UNKNOWN_"; /* '_' is an illegal char for a hostname so this will never match */

static int local_procs_initialized = 0; /* flag indicating whether the below fields are valid */
static int  g_num_global;       /* number of processes in comm world */
static int  g_num_local;        /* number of local processes */
static int *g_local_procs;      /* array of global ranks of local processes */
static int  g_local_rank;       /* local rank of this process */
static int  g_num_nodes;        /* number of local nodes */
static int *g_node_ids;         /* g_node_ids[rank] gives the id of the node where rank is running */

static int get_local_procs_nolocal(int global_rank, int num_global);


/* MPIU_Get_local_procs() determines which processes are local and
  should use shared memory
 
 This uses PMI to get all of the processes that have the same
 hostname, and puts them into local_procs sorted by global rank.

 If an output variable pointer is NULL, it won't be set.

 Caller should NOT free any returned buffers.

 Note that this is really only a temporary solution as it only
 calculates these values for processes MPI_COMM_WORLD, i.e., not for
 spawned or attached processes.

 Algorithm:

 Each process kvs_puts its hostname and stores the total number of
 processes (g_num_global).

 Each process determines local rank (g_local_rank), the number of
 processes local to this node (g_num_local) and creates a list of the
 local processes (g_local_procs[]):
 
   The process kvs_gets the hostname of every other process, and
   compares each to its own hostname.  When it finds a match, we
   assume that the matched process is on the same node as this one, so
   the process records the global rank of the matching process in the
   procs[] array.  If the matching process turns out to be the process
   itself, it now knows its local rank, and sets it appropriately.

   Note: There is an additional clause for matching hostnames when
   "odd_even_cliques" is enabled which checks that the local process
   and the matched process are either both even or both odd.

 Each process determines the number of nodes (g_num_nodes) and assigns
 a node id to each process (g_node_ids[]):
 
   For each hostname the process seaches the list of unique nodes
   names (node_names[]) for a match.  If a match is found, the node id
   is recorded for that matching process.  Otherwise, the hostname is
   added to the list of node names.
 
 */

#undef FUNCNAME
#define FUNCNAME get_local_procs_nolocal
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static int get_local_procs_nolocal(int global_rank, int num_global)
{
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPIU_CHKPMEM_DECL(2);

    g_num_local = 1;
    g_local_rank = 0;
    g_num_nodes = num_global;
    g_num_global = num_global;

    MPIU_CHKPMEM_MALLOC(g_local_procs, int *, g_num_local * sizeof(int), mpi_errno, "local proc array");
    *g_local_procs = global_rank;

    MPIU_CHKPMEM_MALLOC(g_node_ids, int *, num_global * sizeof(int), mpi_errno, "node_ids array");
    for (i = 0; i < num_global; ++i)
        g_node_ids[i] = i;

    MPIU_CHKPMEM_COMMIT();
fn_exit:
    return mpi_errno;
fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    MPIU_CHKPMEM_REAP();
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

/* MPIU_Find_local_and_external -- from the list of processes in comm,
   builds a list of local processes, i.e., processes on this same
   node, and a list of external processes, i.e., one process from each
   node.

   Note that this will not work correctly for spawned or attached
   processes.

   external processes: For a communicator, there is one external
                       process per node.  You can think of this as the
                       root or master process for that node.

   OUT:
     local_size_p      - number of processes on this node
     local_rank_p      - rank of this processes among local processes
     local_ranks_p     - (*local_ranks_p)[i] = the rank in comm
                         of the process with local rank i.
                         This is of size (*local_size_p)
     external_size_p   - number of external processes
     external_rank_p   - rank of this process among the external
                         processes, or -1 if this process is not external
     external_ranks_p  - (*external_ranks_p)[i] = the rank in comm
                         of the process with external rank i.
                         This is of size (*external_size_p)
     intranode_table_p - (*internode_table_p)[i] gives the rank in
                         *local_ranks_p of rank i in comm or -1 if not
                         applicable.  It is of size comm->remote_size.
     internode_table_p - (*internode_table_p)[i] gives the rank in
                         *external_ranks_p of the root of the node
                         containing rank i in comm.  It is of size
                         comm->remote_size.
*/

#undef FUNCNAME
#define FUNCNAME MPIU_Find_local_and_external
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIU_Find_local_and_external(MPID_Comm *comm, int *local_size_p, int *local_rank_p, int **local_ranks_p,
                                 int *external_size_p, int *external_rank_p, int **external_ranks_p,
                                 int **intranode_table_p, int **internode_table_p)
{
    int mpi_errno = MPI_SUCCESS;
    int *nodes;
    int external_size;
    int external_rank;
    int *external_ranks;
    int local_size;
    int local_rank;
    int *local_ranks;
    int *internode_table;
    int *intranode_table;
    int i;
    int node_id;
    int my_node_id;
    int lpid;
    MPIU_CHKLMEM_DECL(1);
    MPIU_CHKPMEM_DECL(4);

    MPIU_Assert(local_procs_initialized);

    /* Scan through the list of processes in comm and add one
       process from each node to the list of "external" processes.  We
       add the first process we find from each node.  nodes[] is an
       array where we keep track of whether we have already added that
       node to the list. */
    
    /* these two will be realloc'ed later to the appropriate size (currently unknown) */
    MPIU_CHKPMEM_MALLOC (external_ranks, int *, sizeof(int) * comm->remote_size, mpi_errno, "external_ranks");
    MPIU_CHKPMEM_MALLOC (local_ranks, int *, sizeof(int) * comm->remote_size, mpi_errno, "local_ranks");

    MPIU_CHKPMEM_MALLOC (internode_table, int *, sizeof(int) * comm->remote_size, mpi_errno, "internode_table");
    MPIU_CHKPMEM_MALLOC (intranode_table, int *, sizeof(int) * comm->remote_size, mpi_errno, "intranode_table");

    MPIU_CHKLMEM_MALLOC (nodes, int *, sizeof(int) * g_num_nodes, mpi_errno, "nodes");
    
    /* nodes maps node_id to rank in external_ranks of leader for that node */
    for (i = 0; i < g_num_nodes; ++i)
        nodes[i] = -1;

    for (i = 0; i < comm->remote_size; ++i)
        intranode_table[i] = -1;
    
    external_size = 0;
    mpi_errno = MPID_VCR_Get_lpid(comm->vcr[comm->rank], &lpid);
    if (mpi_errno) MPIU_ERR_POP (mpi_errno);
    MPIU_Assert(lpid < g_num_global);

    my_node_id = g_node_ids[lpid];
    MPIU_Assert(my_node_id >= 0);
    MPIU_Assert(my_node_id < g_num_nodes);

    local_size = 0;
    local_rank = -1;
    external_rank = -1;
    
    for (i = 0; i < comm->remote_size; ++i)
    {
        mpi_errno = MPID_VCR_Get_lpid(comm->vcr[i], &lpid);
        if (mpi_errno) MPIU_ERR_POP (mpi_errno);

        MPIU_ERR_CHKANDJUMP(lpid >= g_num_global, mpi_errno, MPIR_ERR_RECOVERABLE, "**dynamic_node_ids");

        node_id = g_node_ids[lpid];

        MPIU_Assert(node_id >= 0);
        MPIU_Assert(node_id < g_num_nodes);

        /* build list of external processes */
        if (nodes[node_id] == -1)
        {
            if (i == comm->rank)
                external_rank = external_size;
            nodes[node_id] = external_size;
            external_ranks[external_size] = i;
            ++external_size;
        }

        /* build the map from rank in comm to rank in external_ranks */
        internode_table[i] = nodes[node_id];

        /* build list of local processes */
        if (node_id == my_node_id)
        {
             if (i == comm->rank)
                 local_rank = local_size;

             intranode_table[i] = local_size;
             local_ranks[local_size] = i;
             ++local_size;
        }
    }

    /*
    printf("------------------------------------------------------------------------\n");
    printf("comm = %p\n", comm);
    printf("comm->size = %d\n", comm->remote_size); 
    printf("comm->rank = %d\n", comm->rank); 
    printf("local_size = %d\n", local_size); 
    printf("local_rank = %d\n", local_rank); 
    printf("local_ranks = %p\n", local_ranks);
    for (i = 0; i < local_size; ++i) 
        printf("  local_ranks[%d] = %d\n", i, local_ranks[i]); 
    printf("external_size = %d\n", external_size); 
    printf("external_rank = %d\n", external_rank); 
    printf("external_ranks = %p\n", external_ranks);
    for (i = 0; i < external_size; ++i) 
        printf("  external_ranks[%d] = %d\n", i, external_ranks[i]); 
    printf("intranode_table = %p\n", intranode_table);
    for (i = 0; i < comm->remote_size; ++i) 
        printf("  intranode_table[%d] = %d\n", i, intranode_table[i]); 
    printf("internode_table = %p\n", internode_table);
    for (i = 0; i < comm->remote_size; ++i) 
        printf("  internode_table[%d] = %d\n", i, internode_table[i]); 
    printf("nodes = %p\n", nodes);
    for (i = 0; i < g_num_nodes; ++i) 
        printf("  nodes[%d] = %d\n", i, nodes[i]); 
     */

    *local_size_p = local_size;
    *local_rank_p = local_rank;
    *local_ranks_p =  MPIU_Realloc (local_ranks, sizeof(int) * local_size);
    MPIU_ERR_CHKANDJUMP (*local_ranks_p == NULL, mpi_errno, MPI_ERR_OTHER, "**nomem2");

    *external_size_p = external_size;
    *external_rank_p = external_rank;
    *external_ranks_p = MPIU_Realloc (external_ranks, sizeof(int) * external_size);
    MPIU_ERR_CHKANDJUMP (*external_ranks_p == NULL, mpi_errno, MPI_ERR_OTHER, "**nomem2");

    /* no need to realloc */
    if (intranode_table_p)
        *intranode_table_p = intranode_table;
    if (internode_table_p)
        *internode_table_p = internode_table;
    
    MPIU_CHKPMEM_COMMIT();

 fn_exit:
    MPIU_CHKLMEM_FREEALL();
    return mpi_errno;
 fn_fail:
    MPIU_CHKPMEM_REAP();
    goto fn_exit;
}

/* maps rank r in comm_ptr to the rank of the leader for r's node in
   comm_ptr->node_roots_comm and returns this value.
  
   This function does NOT use mpich2 error handling.
 */
#undef FUNCNAME
#define FUNCNAME MPIU_Get_internode_rank
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIU_Get_internode_rank(MPID_Comm *comm_ptr, int r)
{
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm_valid_ptr( comm_ptr, mpi_errno );
    MPIU_Assert(mpi_errno == MPI_SUCCESS);
    MPIU_Assert(r < comm_ptr->remote_size);
    MPIU_Assert(comm_ptr->comm_kind == MPID_INTRACOMM);
    MPIU_Assert(comm_ptr->internode_table != NULL);

    return comm_ptr->internode_table[r];
}

/* maps rank r in comm_ptr to the rank in comm_ptr->node_comm or -1 if r is not
   a member of comm_ptr->node_comm.
  
   This function does NOT use mpich2 error handling.
 */
#undef FUNCNAME
#define FUNCNAME MPIU_Get_intranode_rank
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIU_Get_intranode_rank(MPID_Comm *comm_ptr, int r)
{
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm_valid_ptr( comm_ptr, mpi_errno );
    MPIU_Assert(mpi_errno == MPI_SUCCESS);
    MPIU_Assert(r < comm_ptr->remote_size);
    MPIU_Assert(comm_ptr->comm_kind == MPID_INTRACOMM);
    MPIU_Assert(comm_ptr->intranode_table != NULL);

    /* FIXME this could/should be a list of ranks on the local node, which
       should take up much less space on a typical thin(ish)-node system. */
    return comm_ptr->intranode_table[r];
}

#undef FUNCNAME
#define FUNCNAME MPIU_Local_procs_finalize
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIU_Local_procs_finalize()
{
    int mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPIU_LOCAL_PROCS_FINALIZE);
    
    MPIDI_FUNC_ENTER(MPID_STATE_MPIU_LOCAL_PROCS_FINALIZE);

    if (local_procs_initialized)
    {
        MPIU_Free(g_local_procs);
        MPIU_Free(g_node_ids);
    }
    
    MPIDI_FUNC_EXIT(MPID_STATE_MPIU_LOCAL_PROCS_FINALIZE);
    return mpi_errno;
}
                          
