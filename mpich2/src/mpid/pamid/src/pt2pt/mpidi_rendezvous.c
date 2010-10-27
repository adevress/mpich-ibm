/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_rendezvous.c
 * \brief Provide for a flow-control rendezvous-based send
 */
#include "mpidimpl.h"

void
MPIDI_RendezvousTransfer(pami_context_t context,
                         MPID_Request * rreq)
{
  void *rcvbuf;
  unsigned rcvlen;

  /* -------------------------------------- */
  /* calculate message length for reception */
  /* calculate receive message "count"      */
  /* -------------------------------------- */
  unsigned dt_contig, dt_size;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_true_lb;
  MPIDI_Datatype_get_info(rreq->mpid.userbufcount,
                          rreq->mpid.datatype,
                          dt_contig,
                          dt_size,
                          dt_ptr,
                          dt_true_lb);

  /* -------------------------------------- */
  /* test for truncated message.            */
  /* -------------------------------------- */
  if (rreq->mpid.envelope.length > dt_size)
    {
      rcvlen = dt_size;
      rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;
      rreq->status.count = rcvlen;
    }
  else
    {
      rcvlen = rreq->mpid.envelope.length;
    }

  /* -------------------------------------- */
  /* if buffer is contiguous ...            */
  /* -------------------------------------- */
  if (dt_contig)
    {
      MPIDI_Request_setCA(rreq, MPIDI_CA_COMPLETE);
      rreq->mpid.uebuf = NULL;
      rreq->mpid.uebuflen = 0;
      rcvbuf = rreq->mpid.userbuf + dt_true_lb;
    }

  /* --------------------------------------------- */
  /* buffer is non-contiguous. we need to allocate */
  /* a temporary buffer, and unpack later.         */
  /* --------------------------------------------- */
  else
    {
      MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
      rcvbuf = MPIU_Malloc(rcvlen);
      MPID_assert(rcvbuf != NULL);
      rreq->mpid.uebuf    = rcvbuf;
      rreq->mpid.uebuflen = rcvlen;
    }

  /* ---------------------------------------------------------------- */
  /* Get the data from the origin node.                               */
  /* ---------------------------------------------------------------- */

  pami_result_t rc;
  pami_endpoint_t dest;
  MPIDI_Context_endpoint(rreq, &dest);

#ifdef USE_PAMI_RDMA
  size_t rcvlen_out;
  rc = PAMI_Memregion_create(context,
                             rcvbuf,
                             rcvlen,
                             &rcvlen_out,
                             &rreq->mpid.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(rcvlen == rcvlen_out);

  TRACE_ERR("RZV Xfer for req=%p addr=%p *addr[0]=%#016llx *addr[1]=%#016llx\n",
            rreq,
            rcvbuf,
            *(((unsigned long long*)rcvbuf)+0),
            *(((unsigned long long*)rcvbuf)+1));

  pami_rget_simple_t params = {
  rma  : {
    dest    : dest,
    hints   : {
      use_rdma:       1,
      no_long_header: 1,
      },
    bytes   : rreq->mpid.envelope.length,
    cookie  : rreq,
    done_fn : MPIDI_RecvRzvDoneCB,
  },
  rdma : {
    local  : {
      mr     : &rreq->mpid.memregion,
      offset : 0,
    },
    remote : {
      mr     : &rreq->mpid.envelope.memregion,
      offset : 0,
    },
  },
  };

  MPIU_THREAD_CS_ENTER(PAMI,);
  rc = PAMI_Rget(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
  MPIU_THREAD_CS_EXIT(PAMI,);
#else
  pami_get_simple_t params = {
  rma  : {
    dest    : dest,
    hints   : {
      use_rdma:       1,
      no_long_header: 1,
      },
    bytes   : rreq->mpid.envelope.length,
    cookie  : rreq,
    done_fn : MPIDI_RecvRzvDoneCB,
  },
  addr : {
    local   : rcvbuf,
    remote  : rreq->mpid.envelope.data,
  },
  };

  MPIU_THREAD_CS_ENTER(PAMI,);
  rc = PAMI_Get(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
  MPIU_THREAD_CS_EXIT(PAMI,);
#endif
}
