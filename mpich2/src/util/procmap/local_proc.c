/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"

#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif

#include "../../mpi/errhan/errcodes.h"
#undef FUNCNAME
#define FUNCNAME MPIU_Find_local_and_external
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIU_Find_local_and_external(MPID_Comm *comm, int *local_size_p, int *local_rank_p, int **local_ranks_p,
                                 int *external_size_p, int *external_rank_p, int **external_ranks_p,
                                 int **intranode_table_p, int **internode_table_p)
{
#define result MPI_ERR_UNKNOWN
/*     return (errcode & ERROR_FATAL_MASK) ? TRUE : FALSE; */
/* Use a version of compile-time-assert to be sure that 1 isn't fatal */
    switch ((size_t)comm)
        {
        case 0:
            break;
        case ((result & ERROR_FATAL_MASK) == 0):
            break;
        default:
            break;
        };
    return result;
#undef result
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
    return mpi_errno;
}
                          
