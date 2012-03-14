/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_1s.c
 * \brief ???
 */
#include "mpidi_onesided.h"


void
MPIDI_Win_DoneCB(pami_context_t  context,
                 void          * cookie,
                 pami_result_t   result)
{
  MPIDI_Win_request *req = (MPIDI_Win_request*)cookie;
  ++req->win->mpid.sync.complete;

  if ((req->buffer_free) && (req->type == MPIDI_WIN_REQUEST_GET))
    {
      ++req->origin.completed;
      if (req->origin.completed == req->target.dt.num_contig)
        {
          int mpi_errno;
          mpi_errno = MPIR_Localcopy(req->buffer,
                                     req->origin.dt.size,
                                     MPI_CHAR,
                                     req->origin.addr,
                                     req->origin.count,
                                     req->origin.datatype);
          MPID_assert(mpi_errno == MPI_SUCCESS);
          MPID_Datatype_release(req->origin.dt.pointer);
          MPIU_Free(req->buffer);
          req->buffer_free = 0;
        }
    }

  if (req->win->mpid.sync.total == req->win->mpid.sync.complete)
    {
      if (req->buffer_free)
        MPIU_Free(req->buffer);
      if (req->accum_headers)
        MPIU_Free(req->accum_headers);
      MPIU_Free(req);
    }
  MPIDI_Progress_signal();
}


void
MPIDI_Win_datatype_basic(int              count,
                         MPI_Datatype     datatype,
                         MPIDI_Datatype * dt)
{
  MPIDI_Datatype_get_info(dt->count = count,
                          dt->type  = datatype,
                          dt->contig,
                          dt->size,
                          dt->pointer,
                          dt->true_lb);
  TRACE_ERR("DT=%08x  DTp=%p  count=%d  contig=%d  size=%zu  true_lb=%zu\n",
            dt->type, dt->pointer, dt->count, dt->contig, (size_t)dt->size, (size_t)dt->true_lb);
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
      unsigned map_size = dt->pointer->max_contig_blocks*dt->count + 1;
      dt->num_contig = map_size;
      dt->map = (DLOOP_VECTOR*)MPIU_Malloc(map_size * sizeof(DLOOP_VECTOR));
      MPID_assert(dt->map != NULL);

      DLOOP_Offset last = dt->pointer->size*dt->count;
      MPID_Segment seg;
      MPID_Segment_init(NULL, dt->count, dt->type, &seg, 0);
      MPID_Segment_pack_vector(&seg, 0, &last, dt->map, &dt->num_contig);
      MPID_assert((unsigned)dt->num_contig <= map_size);
#ifdef TRACE_ON
      TRACE_ERR("dt->pointer->size=%d  num_contig:  orig=%u  new=%d\n", dt->pointer->size, map_size, dt->num_contig);
      int i;
      for(i=0; i<dt->num_contig; ++i)
        TRACE_ERR("     %d:  BUF=%zu  LEN=%zu\n", i, (size_t)dt->map[i].DLOOP_VECTOR_BUF, (size_t)dt->map[i].DLOOP_VECTOR_LEN);
#endif
    }
}


void
MPIDI_Win_datatype_unmap(MPIDI_Datatype * dt)
{
  if (dt->map != &dt->__map)
    MPIU_Free(dt->map);;
}
