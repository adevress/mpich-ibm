/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/onesided/mpid_win_accumulate.c
 * \brief ???
 */
#include "mpidi_onesided.h"


void
MPIDI_WinAccumCB(pami_context_t    context,
                 void            * cookie,
                 const void      * _msginfo,
                 size_t            msginfo_size,
                 const void      * sndbuf,
                 size_t            sndlen,
                 pami_endpoint_t   sender,
                 pami_recv_t     * recv)
{
  MPID_assert(recv   != NULL);
  MPID_assert(sndbuf == NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_Win_MsgInfo));
  MPID_assert(_msginfo != NULL);
  const MPIDI_Win_MsgInfo * msginfo = (const MPIDI_Win_MsgInfo*)_msginfo;

  int null=0;
  pami_type_t         pami_type;
  pami_data_function  pami_op;
  MPItoPAMI(msginfo->type, &pami_type, msginfo->op, &pami_op, &null);

  TRACE_ERR("New accum msg:  addr=%p  len=%zu  type=%d  op=%d\n", msginfo->addr, sndlen, msginfo->type, msginfo->op);
  TRACE_ERR("                         PAMI:    type=%p  op=%p\n", pami_type, pami_op);

  recv->cookie      = NULL;
  recv->local_fn    = NULL;
  recv->addr        = msginfo->addr;
  recv->type        = pami_type;
  recv->offset      = 0;
  recv->data_fn     = pami_op;
  recv->data_cookie = NULL;
}


static pami_result_t
MPIDI_Accumulate(pami_context_t   context,
                 void           * _req)
{
  MPIDI_Win_request *req = (MPIDI_Win_request*)_req;
  pami_result_t rc;

  pami_send_t params = {
    .send = {
      .header = {
        .iov_len = sizeof(MPIDI_Win_MsgInfo),
      },
      .dispatch = MPIDI_Protocols_WinAccum,
      .dest     = req->dest,
    },
    .events = {
      .cookie    = req,
      .remote_fn = MPIDI_Win_DoneCB,
    },
  };

  struct MPIDI_Win_sync* sync = &req->win->mpid.sync;
  TRACE_ERR("Start       num=%d  l-addr=%p  r-base=%p  r-offset=%zu\n",
            req->target.dt.num_contig, req->buffer, req->win->mpid.info[req->target.rank].base_addr, req->offset);
  for (; req->state.index < req->target.dt.num_contig; ++req->state.index) {
    MPID_PROGRESS_WAIT_WHILE(req->state.index > sync->started - sync->complete + MPIDI_Process.rma_pending);
    ++sync->started;

    params.send.header.iov_base = &req->accum_headers[req->state.index];
    params.send.data.iov_len    = req->target.dt.map[req->state.index].DLOOP_VECTOR_LEN;
    params.send.data.iov_base   = req->buffer + req->state.local_offset;

#ifdef TRACE_ON
    unsigned* buf = (unsigned*)params.send.data.iov_base;
#endif
    TRACE_ERR("  Sub     index=%zu  bytes=%zu  l-offset=%zu  r-addr=%p  l-buf=%p  *(int*)buf=0x%08x\n",
              req->state.index, params.send.data.iov_len, req->state.local_offset, req->accum_headers[req->state.index].addr, buf, *buf);
    rc = PAMI_Send(context, &params);
    MPID_assert(rc == PAMI_SUCCESS);

    req->state.local_offset += params.send.data.iov_len;
  }

  MPIDI_Win_datatype_unmap(&req->target.dt);

  return PAMI_SUCCESS;
}


/**
 * \brief MPI-PAMI glue for MPI_ACCUMULATE function
 *
 * Perform DEST = DEST (op) SOURCE for \e origin_count number of
 * \e origin_datatype at \e origin_addr
 * to node \e target_rank into \e target_count number of \e target_datatype
 * into window location \e target_disp offset (window displacement units)
 *
 * According to the MPI Specification:
 *
 *        Each datatype argument must be a predefined datatype or
 *        a derived datatype, where all basic components are of the
 *        same predefined datatype. Both datatype arguments must be
 *        constructed from the same predefined datatype.
 *
 * \param[in] origin_addr      Source buffer
 * \param[in] origin_count     Number of datatype elements
 * \param[in] origin_datatype  Source datatype
 * \param[in] target_rank      Destination rank (target)
 * \param[in] target_disp      Displacement factor in target buffer
 * \param[in] target_count     Number of target datatype elements
 * \param[in] target_datatype  Destination datatype
 * \param[in] op               Operand to perform
 * \param[in] win              Window
 * \return MPI_SUCCES
 *
 * \ref msginfo_usage\n
 * \ref accum_design
 */
int
MPID_Accumulate(void         *origin_addr,
                int           origin_count,
                MPI_Datatype  origin_datatype,
                int           target_rank,
                MPI_Aint      target_disp,
                int           target_count,
                MPI_Datatype  target_datatype,
                MPI_Op        op,
                MPID_Win     *win)
{
  MPIDI_Win_request *req = MPIU_Calloc0(1, MPIDI_Win_request);
  req->win          = win;
  req->type         = MPIDI_WIN_REQUEST_ACCUMULATE;

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

      int mpi_errno = 0;
      mpi_errno = MPIR_Localcopy(origin_addr,
                                 origin_count,
                                 origin_datatype,
                                 req->buffer,
                                 req->origin.dt.size,
                                 MPI_CHAR);
      MPID_assert(mpi_errno == MPI_SUCCESS);
    }


  MPIDI_Win_datatype_map(&req->target.dt);
  win->mpid.sync.total += req->target.dt.num_contig;


  {
    MPI_Datatype basic_type;
    MPID_Datatype_get_basic_type(origin_datatype, basic_type);

    unsigned index;
    MPIDI_Win_MsgInfo * headers = MPIU_Calloc0(req->target.dt.num_contig, MPIDI_Win_MsgInfo);
    req->accum_headers = headers;
    for (index=0; index < req->target.dt.num_contig; ++index) {
     headers[index].addr = win->mpid.info[target_rank].base_addr + req->offset +
                           (size_t)req->target.dt.map[index].DLOOP_VECTOR_BUF;
     headers[index].req  = req;
     headers[index].win  = win;
     headers[index].type = basic_type;
     headers[index].op   = op;
    }

  }

  pami_task_t task = MPID_VCR_GET_LPID(win->comm_ptr->vcr, target_rank);
  pami_result_t rc;
  rc = PAMI_Endpoint_create(MPIDI_Client, task, 0, &req->dest);
  MPID_assert(rc == PAMI_SUCCESS);


  MPIDI_Accumulate(MPIDI_Context[0], req);


  return MPI_SUCCESS;
}
