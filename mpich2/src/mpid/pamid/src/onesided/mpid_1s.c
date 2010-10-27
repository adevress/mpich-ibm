/*  (C)Copyright IBM Corp.  2007, 2011  */
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
              int mpi_errno = 0;
              mpi_errno = MPIR_Localcopy(req->pack_buffer,
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
