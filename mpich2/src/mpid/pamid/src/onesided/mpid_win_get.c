/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_get.c
 * \brief ???
 */
#include "mpidi_onesided.h"


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
  pami_result_t rc;

  MPIDI_Win_request *req = MPIU_Calloc0(1, MPIDI_Win_request);
  req->win          = win;
  req->type         = MPIDI_WIN_REQUEST_GET;

  size_t offset = target_disp * win->mpid.info[target_rank].disp_unit;

  MPIDI_Win_datatype_basic(origin_count,
                           origin_datatype,
                           &req->origin_dt);
  MPIDI_Win_datatype_basic(target_count,
                           target_datatype,
                           &req->target_dt);
  MPID_assert(req->origin_dt.size == req->target_dt.size);

  if ( (req->origin_dt.size == 0) ||
       (target_rank == MPI_PROC_NULL))
    {
      MPIU_Free(req);
      return MPI_SUCCESS;
    }

  /* If the get is a local operation, do it here */
  if (target_rank == win->comm_ptr->rank)
    {
      MPIU_Free(req);
      return MPIR_Localcopy(win->base + offset,
                            target_count,
                            target_datatype,
                            origin_addr,
                            origin_count,
                            origin_datatype);
    }


  pami_task_t task = MPID_VCR_GET_LPID(win->comm_ptr->vcr, target_rank);
  rc = PAMI_Endpoint_create(MPIDI_Client, task, 0, &req->dest);
  MPID_assert(rc == PAMI_SUCCESS);

  if (req->origin_dt.contig)
    {
      req->buffer_free = 0;
      req->buffer      = origin_addr + req->origin_dt.true_lb;
    }
  else
    {
      req->buffer_free = 1;
      req->buffer      = MPIU_Malloc(req->origin_dt.size);
      MPID_assert(req->buffer != NULL);

      MPID_Datatype_add_ref(req->origin_dt.pointer);
      req->origin.addr  = origin_addr;
      req->origin.count = origin_count;
      req->origin.datatype = origin_datatype;
    }


  size_t length_out;
  rc = PAMI_Memregion_create(MPIDI_Context[0],
                             req->buffer,
                             req->origin_dt.size,
                             &length_out,
                             &req->memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(req->origin_dt.size == length_out);


  MPIDI_Win_datatype_map(&req->target_dt);
  win->mpid.sync.total += req->target_dt.num_contig;

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
      mr     : &req->memregion,
      offset : 0,
    },
    remote : {
      mr     : &win->mpid.info[target_rank].memregion,
      offset : offset,
    },
  },
  };

  int index;
  TRACE_ERR("Start       num=%d  l-addr=%p  r-base=%p  r-offset=%zu\n",
            req->target_dt.num_contig, req->buffer, win->mpid.info[target_rank].base_addr, offset);
  for (index=0; index < req->target_dt.num_contig; ++index) {
    MPID_PROGRESS_WAIT_WHILE(index > win->mpid.sync.started - win->mpid.sync.complete + MPIDI_Process.rma_pending);
    ++win->mpid.sync.started;

    params.rma.bytes          = req->target_dt.map[index].DLOOP_VECTOR_LEN;
    params.rdma.remote.offset = offset + (size_t)req->target_dt.map[index].DLOOP_VECTOR_BUF;

#ifdef TRACE_ON
    unsigned* buf = (unsigned*)(req->buffer + params.rdma.local.offset);
#endif
    TRACE_ERR("  Sub     index=%d  bytes=%zu  l-offset=%zu  r-offset=%zu  buf=%p  *(int*)buf=0x%08x\n",
              index, params.rma.bytes, params.rdma.local.offset, params.rdma.remote.offset, buf, *buf);
    rc = PAMI_Rget(MPIDI_Context[0], &params);
    MPID_assert(rc == PAMI_SUCCESS);

    params.rdma.local.offset += params.rma.bytes;
  }

  MPIDI_Win_datatype_unmap(&req->target_dt);


  return MPI_SUCCESS;
}
