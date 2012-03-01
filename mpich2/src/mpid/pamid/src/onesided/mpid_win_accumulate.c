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
  MPIDI_Datatype_to_pami(msginfo->type, &pami_type, msginfo->op, &pami_op, &null);

#ifdef TRACE_ON
  void    *  buf = msginfo->addr;
  unsigned* ibuf = (unsigned*)buf;
  double  * dbuf = (double  *)buf;
  TRACE_ERR("New accum msg:  len=%zu  type=%x  op=%x  l-buf=%p  *(int*)buf=0x%08x  *(double*)buf=%g\n", sndlen, msginfo->type, msginfo->op, buf, *ibuf, *dbuf);
  TRACE_ERR("                PAMI:    type=%p  op=%p\n", pami_type, pami_op);
#endif

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
  TRACE_ERR("Start       index=%u/%d  l-addr=%p  r-base=%p  r-offset=%zu (sync->started=%u  sync->complete=%u)\n",
            req->state.index, req->target.dt.num_contig, req->buffer, req->win->mpid.info[req->target.rank].base_addr, req->offset, sync->started, sync->complete);
  while (req->state.index < req->target.dt.num_contig) {
    if (sync->started > sync->complete + MPIDI_Process.rma_pending)
      {
        TRACE_ERR("Bailing out;  index=%u/%d  sync->started=%u  sync->complete=%u\n",
                req->state.index, req->target.dt.num_contig, sync->started, sync->complete);
        return PAMI_EAGAIN;
      }
    ++sync->started;


    params.send.header.iov_base = &req->accum_headers[req->state.index];
    params.send.data.iov_len    = req->target.dt.map[req->state.index].DLOOP_VECTOR_LEN;
    params.send.data.iov_base   = req->buffer + req->state.local_offset;

#ifdef TRACE_ON
    void    *  buf = params.send.data.iov_base;
    unsigned* ibuf = (unsigned*)buf;
    double  * dbuf = (double  *)buf;
    TRACE_ERR("  Sub     index=%u  bytes=%zu  l-offset=%zu  r-addr=%p  l-buf=%p  *(int*)buf=0x%08x  *(double*)buf=%g\n",
              req->state.index, params.send.data.iov_len, req->state.local_offset, req->accum_headers[req->state.index].addr, buf, *ibuf, *dbuf);
#endif
    rc = PAMI_Send(context, &params);
    MPID_assert(rc == PAMI_SUCCESS);


    req->state.local_offset += params.send.data.iov_len;
    ++req->state.index;
  }

  MPIDI_Win_datatype_unmap(&req->target.dt);

  return PAMI_SUCCESS;
}


/**
 * \brief MPI-PAMI glue for MPI_ACCUMULATE function
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
 * \return MPI_SUCCESS
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

  if (origin_datatype == MPI_DOUBLE_INT)
    {
      MPIDI_Win_datatype_basic(origin_count*2,
                               MPI_DOUBLE,
                               &req->origin.dt);
      MPIDI_Win_datatype_basic(target_count*2,
                               MPI_DOUBLE,
                               &req->target.dt);
    }
  else if (origin_datatype == MPI_LONG_DOUBLE_INT)
    {
      MPIDI_Win_datatype_basic(origin_count*2,
                               MPI_LONG_DOUBLE,
                               &req->origin.dt);
      MPIDI_Win_datatype_basic(target_count*2,
                               MPI_LONG_DOUBLE,
                               &req->target.dt);
    }
  else if (origin_datatype == MPI_LONG_INT)
    {
      MPIDI_Win_datatype_basic(origin_count*2,
                               MPI_LONG,
                               &req->origin.dt);
      MPIDI_Win_datatype_basic(target_count*2,
                               MPI_LONG,
                               &req->target.dt);
    }
  else if (origin_datatype == MPI_SHORT_INT)
    {
      MPIDI_Win_datatype_basic(origin_count*2,
                               MPI_INT,
                               &req->origin.dt);
      MPIDI_Win_datatype_basic(target_count*2,
                               MPI_INT,
                               &req->target.dt);
    }
  else
    {
      MPIDI_Win_datatype_basic(origin_count,
                               origin_datatype,
                               &req->origin.dt);
      MPIDI_Win_datatype_basic(target_count,
                               target_datatype,
                               &req->target.dt);
    }

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


  pami_result_t rc;
  pami_task_t task = MPID_VCR_GET_LPID(win->comm_ptr->vcr, target_rank);
  rc = PAMI_Endpoint_create(MPIDI_Client, task, 0, &req->dest);
  MPID_assert(rc == PAMI_SUCCESS);


  MPIDI_Win_datatype_map(&req->target.dt);
  win->mpid.sync.total += req->target.dt.num_contig;


  {
    MPI_Datatype basic_type = MPI_DATATYPE_NULL;
    MPID_Datatype_get_basic_type(origin_datatype, basic_type);
    /* MPID_Datatype_get_basic_type() doesn't handle the struct types */
    if ((origin_datatype == MPI_FLOAT_INT)  ||
        (origin_datatype == MPI_DOUBLE_INT) ||
        (origin_datatype == MPI_LONG_INT)   ||
        (origin_datatype == MPI_SHORT_INT)  ||
        (origin_datatype == MPI_LONG_DOUBLE_INT))
      {
        MPID_assert(basic_type == MPI_DATATYPE_NULL);
        basic_type = origin_datatype;
      }
    MPID_assert(basic_type != MPI_DATATYPE_NULL);

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


  PAMI_Context_post(MPIDI_Context[0], &req->post_request, MPIDI_Accumulate, req);


  return MPI_SUCCESS;
}
