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
 * \return MPI_SUCCESS, MPI_ERR_RMA_SYNC, or error returned from
 *         MPIR_Localcopy, MPID_Segment_init, or PAMI_Rput.
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

  MPIDI_Datatype_get_info(origin_count,
                          req->origin_dt.type = origin_datatype,
                          req->origin_dt.contig,
                          req->origin_dt.size,
                          req->origin_dt.pointer,
                          req->origin_dt.true_lb);
  MPIDI_Datatype_get_info(target_count,
                          req->target_dt.type = target_datatype,
                          req->target_dt.contig,
                          req->target_dt.size,
                          req->target_dt.pointer,
                          req->target_dt.true_lb);

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


  pami_task_t task = win->mpid.comm->vcr[target_rank];
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

      int origin_errno,  buf_errno;
      MPIDI_msg_sz_t count;

      MPIDI_Buffer_copy(origin_addr,
                        origin_count,
                        origin_datatype,
                        &origin_errno,
                        req->pack_buffer,
                        req->origin_dt.size,
                        MPI_CHAR,
                        &count,
                        &buf_errno);
      MPID_assert(origin_errno == MPI_SUCCESS);
      MPID_assert(buf_errno    == MPI_SUCCESS);
      MPID_assert(req->origin_dt.size == count);
    }


  size_t length_out;
  rc = PAMI_Memregion_create(MPIDI_Context[0],
                             req->pack_buffer,
                             req->origin_dt.size,
                             &length_out,
                             &req->memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(req->origin_dt.size == length_out);


  if (req->target_dt.contig)
    {
      pami_rput_simple_t params = {
      rma  : {
        dest    : req->dest,
        hints   : {
          no_long_header: PAMI_HINT2_ON,
          },
        bytes   : req->origin_dt.size,
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

      rc = PAMI_Rput(MPIDI_Context[0], &params);
      MPID_assert(rc == PAMI_SUCCESS);
    }
  else
    {
      ++win->mpid.sync.complete;
      return 1;
    }

  return mpi_errno;
}
