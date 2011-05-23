/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_get.c
 * \brief ???
 */
#include "mpidi_onesided.h"


static pami_result_t
MPIDI_Get(pami_context_t   context,
          void           * _req)
{
  MPIDI_Win_request *req = (MPIDI_Win_request*)_req;
  pami_result_t rc;

  pami_task_t task = MPID_VCR_GET_LPID(req->win->comm_ptr->vcr, req->target.rank);
  rc = PAMI_Endpoint_create(MPIDI_Client, task, 0, &req->dest);
  MPID_assert(rc == PAMI_SUCCESS);

  pami_rget_simple_t params = {
  rma  : {
    dest    : req->dest,
    hints   : {
      buffer_registered: PAMI_HINT_ENABLE,
      use_rdma:          PAMI_HINT_ENABLE,
      },
    bytes   : 0,
    cookie  : req,
    done_fn : MPIDI_Win_DoneCB,
  },
  rdma : {
    local  : {
      mr     : &req->origin.memregion,
      offset : 0,
    },
    remote : {
      mr     : &req->win->mpid.info[req->target.rank].memregion,
      offset : req->offset,
    },
  },



  };

  int index;
  struct MPIDI_Win_sync* sync = &req->win->mpid.sync;
  TRACE_ERR("Start       num=%d  l-addr=%p  r-base=%p  r-offset=%zu\n",
            req->target.dt.num_contig, req->buffer, req->win->mpid.info[req->target.rank].base_addr, req->offset);
  for (index=0; index < req->target.dt.num_contig; ++index) {
    MPID_PROGRESS_WAIT_WHILE(index > sync->started - sync->complete + MPIDI_Process.rma_pending);
    ++sync->started;

    params.rma.bytes          = req->target.dt.map[index].DLOOP_VECTOR_LEN;
    params.rdma.remote.offset = req->offset + (size_t)req->target.dt.map[index].DLOOP_VECTOR_BUF;

#ifdef TRACE_ON
    unsigned* buf = (unsigned*)(req->buffer + params.rdma.local.offset);
#endif
    TRACE_ERR("  Sub     index=%d  bytes=%zu  l-offset=%zu  r-offset=%zu  buf=%p  *(int*)buf=0x%08x\n",
              index, params.rma.bytes, params.rdma.local.offset, params.rdma.remote.offset, buf, *buf);
    rc = PAMI_Rget(context, &params);
    MPID_assert(rc == PAMI_SUCCESS);

    params.rdma.local.offset += params.rma.bytes;
  }

  MPIDI_Win_datatype_unmap(&req->target.dt);

  return PAMI_SUCCESS;
}


/**
 * \brief MPI-PAMI glue for MPI_GET function
 *
 * Get \e target_count number of \e target_datatype from \e target_rank
 * from window location \e target_disp offset (window displacement units)
 * into \e origin_count number of \e origin_datatype at \e origin_addr
 *
 * \param[in] origin_addr      Source buffer
 * \param[in] origin_count     Number of datatype elements
 * \param[in] origin_datatype  Source datatype
 * \param[in] target_rank      Destination rank (target)
 * \param[in] target_disp      Displacement factor in target buffer
 * \param[in] target_count     Number of target datatype elements
 * \param[in] target_datatype  Destination datatype
 * \param[in] win              Window
 * \return MPI_SUCCESS
 */
int
MPID_Get(void         *origin_addr,
         int           origin_count,
         MPI_Datatype  origin_datatype,
         int           target_rank,
         MPI_Aint      target_disp,
         int           target_count,
         MPI_Datatype  target_datatype,
         MPID_Win     *win)
{
  MPIDI_Win_request *req = MPIU_Calloc0(1, MPIDI_Win_request);
  req->win          = win;
  req->type         = MPIDI_WIN_REQUEST_GET;

  req->offset = target_disp * win->mpid.info[target_rank].disp_unit;

  MPIDI_Win_datatype_basic(origin_count,
                           origin_datatype,
                           &req->origin.dt);
  MPIDI_Win_datatype_basic(target_count,
                           target_datatype,
                           &req->target.dt);
  MPID_assert(req->origin.dt.size == req->target.dt.size);

  if ( (req->origin.dt.size == 0) ||
       (target_rank == MPI_PROC_NULL))
    {
      MPIU_Free(req);
      return MPI_SUCCESS;
    }

  /* If the get is a local operation, do it here */
  if (target_rank == win->comm_ptr->rank)
    {
      MPIU_Free(req);
      return MPIR_Localcopy(win->base + req->offset,
                            target_count,
                            target_datatype,
                            origin_addr,
                            origin_count,
                            origin_datatype);
    }
  req->target.rank = target_rank;


  if (req->origin.dt.contig)
    {
      req->buffer_free = 0;
      req->buffer      = origin_addr + req->origin.dt.true_lb;
    }
  else
    {
      req->buffer_free = 1;
      req->buffer      = MPIU_Malloc(req->origin.dt.size);
      MPID_assert(req->buffer != NULL);

      MPID_Datatype_add_ref(req->origin.dt.pointer);
      req->origin.addr  = origin_addr;
      req->origin.count = origin_count;
      req->origin.datatype = origin_datatype;




    }


  size_t length_out;
  pami_result_t rc;
  rc = PAMI_Memregion_create(MPIDI_Context[0],
                             req->buffer,
                             req->origin.dt.size,
                             &length_out,
                             &req->origin.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(req->origin.dt.size == length_out);


  MPIDI_Win_datatype_map(&req->target.dt);
  win->mpid.sync.total += req->target.dt.num_contig;


  MPIDI_Get(MPIDI_Context[0], req);


  return MPI_SUCCESS;
}
