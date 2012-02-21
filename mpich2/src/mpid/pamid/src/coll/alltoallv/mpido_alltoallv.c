/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/alltoallv/mpido_alltoallv.c
 * \brief ???
 */
//#define TRACE_ON

#include <mpidimpl.h>

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
   volatile unsigned active = 1;
   int sndtypelen, rcvtypelen, snd_contig, rcv_contig;
   MPID_Datatype *sdt, *rdt;
   pami_type_t stype, rtype;
   MPI_Aint sdt_true_lb, rdt_true_lb;
   MPIDI_Post_coll_t alltoallv_post;
   int rc;
   int pamidt = 1;
   int tmp;

   if(MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
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

   MPIDI_Datatype_get_info(1, sendtype, snd_contig, sndtypelen, sdt, sdt_true_lb);
   MPIDI_Datatype_get_info(1, recvtype, rcv_contig, rcvtypelen, rdt, rdt_true_lb);

   pami_xfer_t alltoallv;
   pami_algorithm_t my_alltoallv;
   pami_metadata_t *my_alltoallv_md;
   int queryreq = 0;

#ifdef MPIDI_BASIC_COLLECTIVE_SELECTION
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] == MPID_COLL_SELECTED)
   {
      TRACE_ERR("Optimized alltoallv was selected\n");
      my_alltoallv = comm_ptr->mpid.opt_protocol[PAMI_XFER_ALLTOALLV_INT][0];
      my_alltoallv_md = &comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLTOALLV_INT][0];
      queryreq = comm_ptr->mpid.must_query[PAMI_XFER_ALLTOALLV_INT][0];
   }
   else
#endif
   { /* is this purely an else? or do i need to check for some other selectedvar... */
      TRACE_ERR("Alltoallv specified by user\n");
      my_alltoallv = comm_ptr->mpid.user_selected[PAMI_XFER_ALLTOALLV_INT];
      my_alltoallv_md = &comm_ptr->mpid.user_metadata[PAMI_XFER_ALLTOALLV_INT];
      queryreq = comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT];
   }
   alltoallv.algorithm = my_alltoallv;
   char *pname = my_alltoallv_md->name;


   alltoallv.cb_done = cb_alltoallv;
   alltoallv.cookie = (void *)&active;
   /* We won't bother with alltoallv since MPI is always going to be ints. */
   alltoallv.cmd.xfer_alltoallv_int.sndbuf = sendbuf+sdt_true_lb;
   alltoallv.cmd.xfer_alltoallv_int.rcvbuf = recvbuf+rdt_true_lb;
      
/* I'll delete this code once the PAMI API changes for the fix discussed below */
#ifdef PAMI_BYTES_REQUIRED 
   int *slen, *rlen;
   if(!comm_ptr->rank)
      TRACE_ERR("Mallocing arraysin alltoallv\n");
   slen = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   rlen = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   assert(slen != NULL);
   assert(rlen != NULL);
#endif

#ifdef PAMI_DISPS_ARE_BYTES
   int *sdisp, *rdisp;
   sdisp = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   rdisp = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   assert(sdisp != NULL);
   assert(rdisp != NULL);
#endif
   alltoallv.cmd.xfer_alltoallv_int.sdispls = senddispls;
   alltoallv.cmd.xfer_alltoallv_int.rdispls = recvdispls;
   alltoallv.cmd.xfer_alltoallv_int.stypecounts = sendcounts;
   alltoallv.cmd.xfer_alltoallv_int.rtypecounts = recvcounts;
   alltoallv.cmd.xfer_alltoallv_int.stype = stype;
   alltoallv.cmd.xfer_alltoallv_int.rtype = rtype;

#if defined(PAMI_DISPS_ARE_BYTES) || defined(PAMI_BYTES_REQUIRED)
   int i = 0;
   for(i = 0; i <comm_ptr->local_size; i++)
   {
      #ifdef PAMI_DISPS_ARE_BYTES 
      sdisp[i] = sndtypelen * senddispls[i];
      rdisp[i] = rcvtypelen * recvdispls[i];
      TRACE_ERR("sd[%d]: %d -> %d, rd[]: %d -> %d\n", i, senddispls[i],
         sdisp[i], recvdispls[i], rdisp[i]);
      #endif
      #ifdef PAMI_BYTES_REQUIRED 
      slen[i] = sndtypelen * sendcounts[i];
      rlen[i] = rcvtypelen * recvcounts[i];
      TRACE_ERR("sc[%d]: %d -> %d, rc[]: %d -> %d\n", i, sendcounts[i],
         slen[i], recvcounts[i], rlen[i]);
      #endif
   }
   if(!comm_ptr->rank)
      TRACE_ERR("Done mallocing\n");
   #ifdef PAMI_DISPS_ARE_BYTES
   alltoallv.cmd.xfer_alltoallv_int.sdispls = sdisp;
   alltoallv.cmd.xfer_alltoallv_int.rdispls = rdisp;
   #endif
   #ifdef PAMI_BYTES_REQUIRED
   alltoallv.cmd.xfer_alltoallv_int.stypecounts = slen;
   alltoallv.cmd.xfer_alltoallv_int.rtypecounts = rlen;
   alltoallv.cmd.xfer_alltoallv_int.stype = PAMI_TYPE_BYTE;
   alltoallv.cmd.xfer_alltoallv_int.rtype = PAMI_TYPE_BYTE;
   #endif

#endif


   if(unlikely(queryreq == MPID_COLL_ALWAYS_QUERY || queryreq == MPID_COLL_CHECK_FN_REQUIRED))
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying alltoallv protocol %s, type was %d\n", pname, queryreq);
      result = my_alltoallv_md->check_fn(&alltoallv);
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

#ifdef PAMI_BYTES_REQUIRED
   MPIU_Free(slen);
   MPIU_Free(rlen);
#endif
#ifdef PAMI_DISPS_ARE_BYTES
   MPIU_Free(sdisp);
   MPIU_Free(rdisp);
#endif

   return rc;
}
