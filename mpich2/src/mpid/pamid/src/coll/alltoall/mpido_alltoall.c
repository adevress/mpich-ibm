/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/alltoall/mpido_alltoall.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void cb_alltoall(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *)clientdata;
   TRACE_ERR("alltoall callback enter, active: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}


int MPIDO_Alltoall(void *sendbuf,
                   int sendcount,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   MPID_Comm *comm_ptr,
                   int *mpierrno)
{
   TRACE_ERR("Entering MPIDO_Alltoall\n");
   char *pname = comm_ptr->mpid.user_metadata[PAMI_XFER_ALLTOALL].name;
   volatile unsigned active = 1;
   MPID_Datatype *sdt, *rdt;
   pami_type_t stype, rtype;
   MPI_Aint sdt_true_lb, rdt_true_lb;
   MPIDI_Post_coll_t alltoall_post;
   int rc, sndlen, rcvlen, snd_contig, rcv_contig, pamidt=1;
   int tmp;

   MPIDI_Datatype_get_info(1, sendtype, snd_contig, sndlen, sdt, sdt_true_lb);
   MPIDI_Datatype_get_info(1, recvtype, rcv_contig, rcvlen, rdt, rdt_true_lb);

   /* Alltoall is much simpler if bytes are required because we don't need to
    * malloc displ/count arrays and copy things
    */


   /* Is it a built in type? If not, send to MPICH */
   if(MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(!snd_contig) pamidt = 0;
   if(!rcv_contig) pamidt = 0;

   if((sendbuf == MPI_IN_PLACE) ||
      (comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] == MPID_COLL_USE_MPICH) ||
      pamidt == 0)
   {
      TRACE_ERR("Using MPICH alltoall\n");
      return MPIR_Alltoall_intra(sendbuf, sendcount, sendtype,
                      recvbuf, recvcount, recvtype,
                      comm_ptr, mpierrno);

   }

   pami_xfer_t alltoall;

   alltoall.cb_done = cb_alltoall;
   alltoall.cookie = (void *)&active;
   alltoall.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_ALLTOALL];
   alltoall.cmd.xfer_alltoall.sndbuf = sendbuf + sdt_true_lb;
   alltoall.cmd.xfer_alltoall.rcvbuf = recvbuf + rdt_true_lb;

   alltoall.cmd.xfer_alltoall.stypecount = sendcount;
   alltoall.cmd.xfer_alltoall.rtypecount = recvcount;
   alltoall.cmd.xfer_alltoall.stype = stype;
   alltoall.cmd.xfer_alltoall.rtype = rtype;

   if(unlikely(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] >= MPID_COLL_QUERY))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying alltoall protocol %s, type was %d\n", pname,
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL]);
      result = comm_ptr->mpid.user_metadata[PAMI_XFER_ALLTOALL].check_fn(&alltoall);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"Query failed for %s\n", pname);
      }
   }

   if(MPIDI_Process.context_post)
   {
      TRACE_ERR("Posting alltoall\n");
      alltoall_post.coll_struct = &alltoall;
      rc = PAMI_Context_post(MPIDI_Context[0], &alltoall_post.state,
            MPIDI_Pami_post_wrapper, (void *)&alltoall_post);
      TRACE_ERR("Alltoall posted, rc: %d\n", rc);
   }
   else
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&alltoall);

   TRACE_ERR("Waiting on active\n");
   MPID_PROGRESS_WAIT_WHILE(active);

   TRACE_ERR("Leaving alltoall\n");
   return rc;
}
