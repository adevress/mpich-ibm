/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/onesided/mpid_1s.c
 * \brief ???
 */
#include "mpidi_onesided.h"


void
MPIDI_DoneCB(pami_context_t  context,
             void          * cookie,
             pami_result_t   result)
{
  MPIDI_Win_request *req = (MPIDI_Win_request*)cookie;
  ++req->ops_complete;

  if (req->ops_started == req->ops_complete)
    {
      ++req->win->mpid.sync.complete;

      if (req->pack_free)
        {
          if (req->type == MPIDI_WIN_REQUEST_GET)
            {
              int origin_errno,  buf_errno;
              MPIDI_msg_sz_t count;

              MPIDI_Buffer_copy(req->pack_buffer,
                                req->origin_dt.size,
                                MPI_CHAR,
                                &buf_errno,
                                req->origin.addr,
                                req->origin.count,
                                req->origin.datatype,
                                &count,
                                &origin_errno);
              MPID_Datatype_release(req->origin_dt.pointer);
              MPID_assert(origin_errno == MPI_SUCCESS);
              MPID_assert(buf_errno    == MPI_SUCCESS);
              MPID_assert(req->origin_dt.size == count);
            }
          MPIU_Free(req->pack_buffer);
        }

      MPIU_Free(req);
      MPIDI_Progress_signal();
    }
}


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
