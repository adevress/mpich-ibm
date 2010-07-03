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
              int mpi_errno =
              MPIR_Localcopy(req->pack_buffer,
                             req->origin_dt.size,
                             MPI_CHAR,
                             req->origin.addr,
                             req->origin.count,
                             req->origin.datatype);
              MPID_assert(mpi_errno == MPI_SUCCESS);
              MPID_Datatype_release(req->origin_dt.pointer);
            }
          MPIU_Free(req->pack_buffer);
        }

      MPIU_Free(req);
    }
  MPIDI_Progress_signal();
}


void
MPIDI_Win_datatype_basic(int              count,
                         MPI_Datatype     datatype,
                         MPIDI_Datatype * dt)
{
  MPIDI_Datatype_get_info(count,
                          dt->type = datatype,
                          dt->contig,
                          dt->size,
                          dt->pointer,
                          dt->true_lb);
}


void
MPIDI_Win_datatype_map(MPIDI_Datatype * dt)
{
  if (dt->contig)
    {
      dt->num_contig = 1;
      dt->map = &dt->__map;
      dt->map[0].DLOOP_VECTOR_BUF = (void*)(size_t)dt->true_lb;
      dt->map[0].DLOOP_VECTOR_LEN = dt->size;
    }
  else
    {
      dt->num_contig = dt->pointer->max_contig_blocks + 1;
      dt->map = (DLOOP_VECTOR*)MPIU_Malloc(dt->num_contig * sizeof(DLOOP_VECTOR));
      MPID_assert(dt->map != NULL);

      DLOOP_Offset last = dt->pointer->size;
      MPID_Segment seg;
      MPID_Segment_init(NULL, 1, dt->type, &seg, 0);
      MPID_Segment_pack_vector(&seg, 0, &last, dt->map, &dt->num_contig);
      MPID_assert(dt->num_contig <= dt->pointer->max_contig_blocks);
    }
}


void
MPIDI_Win_datatype_unmap(MPIDI_Datatype * dt)
{
  if (dt->map != &dt->__map)
    MPIU_Free(dt->map);;
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
