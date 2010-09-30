/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/onesided/mpid_win_put.c
 * \brief ???
 */
#include "mpidi_onesided.h"


/**
 * \brief MPI-PAMI glue for MPI_PUT function
 *
 * Put \e origin_count number of \e origin_datatype from \e origin_addr
 * to node \e target_rank into \e target_count number of \e target_datatype
 * into window location \e target_disp offset (window displacement units)
 *
 * \param[in] origin_addr      Source buffer
 * \param[in] origin_count     Number of datatype elements
 * \param[in] origin_datatype  Source datatype
 * \param[in] target_rank      Destination rank (target)
 * \param[in] target_disp      Displacement factor in target buffer
 * \param[in] target_count     Number of target datatype elements
 * \param[in] target_datatype  Destination datatype
 * \param[in] win              Window
 * \return MPI_SUCCESS or error returned from MPIR_Localcopy
 */
int
MPID_Put(void         *origin_addr,
         int           origin_count,
         MPI_Datatype  origin_datatype,
         int           target_rank,
         MPI_Aint      target_disp,
         int           target_count,
         MPI_Datatype  target_datatype,
         MPID_Win     *win)
{
  int mpi_errno = MPI_SUCCESS;
  pami_result_t rc;

  MPIDI_Win_request *req = MPIU_Calloc0(1, MPIDI_Win_request);
  req->win          = win;
  req->type         = MPIDI_WIN_REQUEST_PUT;
  req->ops_complete = 0;
  req->ops_started  = 1;

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
      return mpi_errno;
    }

  /* If the get is a local operation, do it here */
  if (target_rank == win->mpid.comm->rank)
    {
      MPIU_Free(req);
      return MPIR_Localcopy(origin_addr,
                            origin_count,
                            origin_datatype,
                            win->base + offset,
                            target_count,
                            target_datatype);
    }


  pami_task_t task = MPID_VCR_GET_LPID(win->mpid.comm->vcr, target_rank);
  rc = PAMI_Endpoint_create(MPIDI_Client, task, 0, &req->dest);
  MPID_assert(rc == PAMI_SUCCESS);

  ++win->mpid.sync.started;

  if (req->origin_dt.contig)
    {
      req->pack_buffer = origin_addr + req->origin_dt.true_lb;
      req->pack_free = 0;
    }
  else
    {
      req->pack_free = 1;
      req->pack_buffer = MPIU_Malloc(req->origin_dt.size);
      MPID_assert(req->pack_buffer != NULL);

      int mpi_errno = 0;
      mpi_errno = MPIR_Localcopy(origin_addr,
                                 origin_count,
                                 origin_datatype,
                                 req->pack_buffer,
                                 req->origin_dt.size,
                                 MPI_CHAR);
      MPID_assert(mpi_errno == MPI_SUCCESS);
    }


  size_t length_out;
  rc = PAMI_Memregion_create(MPIDI_Context[0],
                             req->pack_buffer,
                             req->origin_dt.size,
                             &length_out,
                             &req->memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(req->origin_dt.size == length_out);


  MPIDI_Win_datatype_map(&req->target_dt);
  req->ops_started = req->target_dt.num_contig;

  pami_rput_simple_t params = {
  rma  : {
    dest    : req->dest,
    hints   : {
      no_long_header: PAMI_HINT2_ON,
      },
    bytes   : 0,
    cookie  : req,
    done_fn : NULL,
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
  put : {
    rdone_fn : MPIDI_DoneCB,
  },
  };

  int index;
  TRACE_ERR("Start       num=%d  l-addr=%p  r-base=%p  r-offset=%zu\n",
            req->ops_started, req->pack_buffer, win->mpid.info[target_rank].base_addr, offset);
  for (index=0; index < req->ops_started; ++index) {
    MPID_PROGRESS_WAIT_WHILE(index > req->ops_complete + MPIDI_Process.rma_pending);

    params.rma.bytes          = req->target_dt.map[index].DLOOP_VECTOR_LEN;
    params.rdma.remote.offset = offset + (size_t)req->target_dt.map[index].DLOOP_VECTOR_BUF;

#ifdef TRACE_ON
    unsigned* buf = (unsigned*)(req->pack_buffer + params.rdma.local.offset);
#endif
    TRACE_ERR("  Sub     index=%d  bytes=%zu  l-offset=%zu  r-offset=%zu  buf=%p  *(int*)buf=0x%08x\n",
              index, params.rma.bytes, params.rdma.local.offset, params.rdma.remote.offset, buf, *buf);
    rc = PAMI_Rput(MPIDI_Context[0], &params);
    MPID_assert(rc == PAMI_SUCCESS);

    params.rdma.local.offset += params.rma.bytes;
  }

  MPIDI_Win_datatype_unmap(&req->target_dt);


  return mpi_errno;
}
