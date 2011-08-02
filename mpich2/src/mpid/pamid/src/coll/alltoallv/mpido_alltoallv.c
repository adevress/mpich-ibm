/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/alltoallv/mpido_alltoallv.c
 * \brief ???
 */
//#define TRACE_ON

#include <mpidimpl.h>

#define PAMIBYTEREQUIRED

static void cb_alltoallv(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *)clientdata;
   TRACE_ERR("alltoallv callback enter, active: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}


int MPIDO_Alltoallv(void *sendbuf,
                   int *sendcounts,
                   int *senddispls,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int *recvcounts,
                   int *recvdispls,
                   MPI_Datatype recvtype,
                   MPID_Comm *comm_ptr,
                   int *mpierrno)
{
   if(comm_ptr->rank == 0)
      TRACE_ERR("Entering MPIDO_Alltoallv\n");
   char *pname = comm_ptr->mpid.user_metadata[PAMI_XFER_ALLTOALLV_INT].name;
   volatile unsigned active = 1;
   int tsndlen, trcvlen, snd_contig, rcv_contig;
   MPID_Datatype *sdt, *rdt;
   pami_type_t stype, rtype;
   MPI_Aint sdt_true_lb, rdt_true_lb;
   MPIDI_Post_coll_t alltoallv_post;
   int rc;
   int pamidt = 1;
   int tmp;

   if(MPItoPAMI(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(MPItoPAMI(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;

   if((sendbuf == MPI_IN_PLACE) || 
      (comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] == 
            MPID_COLL_USE_MPICH) ||
       pamidt == 0)
   {
      if(!comm_ptr->rank)
         TRACE_ERR("Using MPICH alltoallv\n");
      return MPIR_Alltoallv(sendbuf, sendcounts, senddispls, sendtype,
                            recvbuf, recvcounts, recvdispls, recvtype,
                            comm_ptr, mpierrno);
   }
   if(!comm_ptr->rank)
      TRACE_ERR("Using %s for alltoallv protocol\n", pname);

   MPIDI_Datatype_get_info(1, sendtype, snd_contig, tsndlen, sdt, sdt_true_lb);
   MPIDI_Datatype_get_info(1, recvtype, rcv_contig, trcvlen, rdt, rdt_true_lb);

   pami_xfer_t alltoallv;
   alltoallv.cb_done = cb_alltoallv;
   alltoallv.cookie = (void *)&active;
   /* We won't bother with alltoallv since MPI is always going to be ints. */
   alltoallv.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_ALLTOALLV_INT];
   alltoallv.cmd.xfer_alltoallv_int.sndbuf = sendbuf+sdt_true_lb;
   alltoallv.cmd.xfer_alltoallv_int.rcvbuf = recvbuf+rdt_true_lb;



#ifdef PAMIBYTEREQUIRED
   int *slen, *sdisp, *rlen, *rdisp;
   if(!comm_ptr->rank)
      TRACE_ERR("Mallocing arrays\n");
   slen = malloc(sizeof(int) * comm_ptr->local_size);
   sdisp = malloc(sizeof(int) * comm_ptr->local_size);
   rlen = malloc(sizeof(int) * comm_ptr->local_size);
   rdisp = malloc(sizeof(int) * comm_ptr->local_size);
   assert(slen != NULL);
   assert(sdisp != NULL);
   assert(rlen != NULL);
   assert(rdisp != NULL);
   int i = 0;
   for(i = 0; i <comm_ptr->local_size; i++)
   {
      slen[i] = tsndlen * sendcounts[i];
      sdisp[i] = tsndlen * senddispls[i];
      rlen[i] = trcvlen * recvcounts[i];
      rdisp[i] = trcvlen * recvdispls[i];
      TRACE_ERR("sc[%d]: %d -> %d, sd[]: %d -> %d, rc[]: %d -> %d, rd[]: %d -> %d\n",
            i, sendcounts[i], slen[i], senddispls[i], sdisp[i], recvcounts[i], rlen[i], recvdispls[i], rdisp[i]);
   }
   if(!comm_ptr->rank)
      TRACE_ERR("Done mallocing\n");
   alltoallv.cmd.xfer_alltoallv_int.stypecounts = slen;
   alltoallv.cmd.xfer_alltoallv_int.sdispls = sdisp;
   alltoallv.cmd.xfer_alltoallv_int.rtypecounts = rlen;
   alltoallv.cmd.xfer_alltoallv_int.rdispls = rdisp;
   alltoallv.cmd.xfer_alltoallv_int.stype = PAMI_TYPE_BYTE;
   alltoallv.cmd.xfer_alltoallv_int.rtype = PAMI_TYPE_BYTE;

#else

   alltoallv.cmd.xfer_alltoallv_int.stypecounts = sendcounts;
   alltoallv.cmd.xfer_alltoallv_int.sdispls = senddispls;
   alltoallv.cmd.xfer_alltoallv_int.rtypecounts = recvcounts;
   alltoallv.cmd.xfer_alltoallv_int.rdispls = recvdispls;
   alltoallv.cmd.xfer_alltoallv_int.stype = stype;
   alltoallv.cmd.xfer_alltoallv_int.rtype = rtype;

#endif 

   if(unlikely(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] >= MPID_COLL_QUERY))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying alltoallv protocol %s, type was %d\n", pname,
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT]);
      result = comm_ptr->mpid.user_metadata[PAMI_XFER_ALLTOALLV_INT].check_fn(&alltoallv);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"Query failed for %s\n", pname);
      }
   }

   if(MPIDI_Process.context_post)
   {
      if(!comm_ptr->rank)
         TRACE_ERR("Posting alltoallv\n");
      alltoallv_post.coll_struct = &alltoallv;
      rc = PAMI_Context_post(MPIDI_Context[0], &alltoallv_post.state, 
            MPIDI_Pami_post_wrapper, (void *)&alltoallv_post);
      if(!comm_ptr->rank)
         TRACE_ERR("Alltoallv posted, rc: %d\n", rc);
   }
   else
   {
      if(!comm_ptr->rank)
         TRACE_ERR("Calling PAMI_Collective\n");
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&alltoallv);
   }

   TRACE_ERR("%d waiting on active %d\n", comm_ptr->rank, active);
   MPID_PROGRESS_WAIT_WHILE(active);

   TRACE_ERR("Leaving alltoallv\n");
#ifdef PAMIBYTEREQUIRED
   free(slen);
   free(sdisp);
   free(rlen);
   free(rdisp);
#endif
   return rc;
}
