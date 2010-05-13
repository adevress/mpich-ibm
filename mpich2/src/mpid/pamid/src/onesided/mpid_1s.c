/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/onesided/mpid_1s.c
 * \brief ???
 */
#include "mpidi_onesided.h"


#if JRATT_OLD_1S
/**
 * ******************************************************************
 * \brief MPI Onesided operation device declarations (!!! not used)
 * Is here only because mpiimpl.h needs it.
 * ******************************************************************
 */
typedef struct MPIDI_RMA_ops {
    struct MPIDI_RMA_ops *next;  /* pointer to next element in list */
    int type;  /* MPIDI_RMA_PUT, MPID_REQUEST_GET,
                  MPIDI_RMA_ACCUMULATE, MPIDI_RMA_LOCK */
    void *origin_addr;
    int origin_count;
    MPI_Datatype origin_datatype;
    int target_rank;
    MPI_Aint target_disp;
    int target_count;
    MPI_Datatype target_datatype;
    MPI_Op op;  /* for accumulate */
    int lock_type;  /* for win_lock */
} MPIDI_RMA_ops;


/* to send derived datatype across in RMA ops */
typedef struct MPIDI_RMA_dtype_info
{
  int      is_contig;
  int      n_contig_blocks;
  int      size;
  MPI_Aint extent;
  int      dataloop_size;
  void    *dataloop;
  int      dataloop_depth;
  int      eltype;
  MPI_Aint ub;
  MPI_Aint lb;
  MPI_Aint true_ub;
  MPI_Aint true_lb;
  int      has_sticky_ub;
  int      has_sticky_lb;
  int      unused0;
  int      unused1;
}
MPIDI_RMA_dtype_info;


/**
 * \brief One-sided Epoch Types
 */
enum
  {
    MPIDI_WIN_EPOCH_NONE=0,     /**< No epoch in affect */
    MPIDI_WIN_EPOCH_FENCE,      /**< MPI_Win_fence access/exposure epoch */
    MPIDI_WIN_EPOCH_LOCK,       /**< MPI_Win_lock access epoch */
    MPIDI_WIN_EPOCH_START,      /**< MPI_Win_start access epoch */
    MPIDI_WIN_EPOCH_POST,       /**< MPI_Win_post exposure epoch */
    MPIDI_WIN_EPOCH_POSTSTART,  /**< MPI_Win_post+MPI_Win_start access/exposure epoch */
    MPIDI_WIN_EPOCH_REFENCE,    /**< MPI_Win_fence possible access/exposure epoch */
  };
#endif
