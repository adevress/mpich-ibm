/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/gather/mpido_gatherv.c
 * \brief ???
 */

//#define TRACE_ON
#include <mpidimpl.h>

static void cb_gatherv(void *ctxt, void *clientdata, pami_result_t err)
{
   unsigned *active = (unsigned *)clientdata;
   TRACE_ERR("cb_gatherv enter, active: %u\n", (*active));
   (*active)--;
}

int MPIDO_Gatherv(void *sendbuf, 
                  int sendcount, 
                  MPI_Datatype sendtype,
                  void *recvbuf, 
                  int *recvcounts, 
                  int *displs, 
                  MPI_Datatype recvtype,
                  int root, 
                  MPID_Comm * comm_ptr, 
                  int *mpierrno)

{
   TRACE_ERR("Entering MPIDO_Gatherv\n");
   int rc;
   int contig, rsize, ssize;
   int pamidt = 1;
   ssize = 0;
   MPID_Datatype *dt_ptr = NULL;
   MPI_Aint send_true_lb, recv_true_lb;
   char *sbuf, *rbuf;
   pami_type_t stype, rtype;
   int tmp;
   volatile unsigned gatherv_active = 1;

   /* Check for native PAMI types and MPI_IN_PLACE on sendbuf */
   if(sendbuf == MPI_IN_PLACE || MPIDI_Datatype_to_pami(sendtype, &stype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;
   if(MPIDI_Datatype_to_pami(recvtype, &rtype, -1, NULL, &tmp) != MPI_SUCCESS)
      pamidt = 0;

   if(pamidt == 0 || comm_ptr->mpid.user_selectedvar[PAMI_XFER_GATHERV_INT] == MPID_COLL_USE_MPICH)
   {
      TRACE_ERR("GATHERV using MPICH\n");
      MPIDI_Update_last_algorithm(comm_ptr, "GATHERV_MPICH");
      return MPIR_Gatherv(sendbuf, sendcount, sendtype,
               recvbuf, recvcounts, displs, recvtype,
               root, comm_ptr, mpierrno);
   }

   MPIDI_Datatype_get_info(1, recvtype, contig, rsize, dt_ptr, recv_true_lb);
   rbuf = recvbuf + recv_true_lb;
   sbuf = sendbuf;

   if(comm_ptr->rank == root)
   {
      MPIDI_Datatype_get_info(1, sendtype, contig, ssize, dt_ptr, send_true_lb);
      sbuf = sendbuf + send_true_lb;
   }

   pami_xfer_t gatherv;
   gatherv.cb_done = cb_gatherv;
   gatherv.cookie = (void *)&gatherv_active;
   gatherv.cmd.xfer_gatherv_int.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
   gatherv.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_GATHERV];
   gatherv.cmd.xfer_gatherv_int.sndbuf = sbuf;
   gatherv.cmd.xfer_gatherv_int.rcvbuf = rbuf;
   #ifdef PAMI_DISPS_ARE_BYTES 
   TRACE_ERR("Malloc()ing array for MPIDO_Gatherv displacements\n");
   int *rdispls;
   rdispls = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   assert(rdispls != NULL);
   #endif
   #ifdef PAMI_BYTES_REQUIRED
   TRACE_ERR("Malloc()ing array for MPIDO_Gatherv counts\n");
   int *rcounts;
   rcounts = MPIU_Malloc(sizeof(int) * comm_ptr->local_size);
   assert(rcounts != NULL);
   gatherv.cmd.xfer_gatherv_int.rtype = PAMI_TYPE_BYTE;
   gatherv.cmd.xfer_gatherv_int.stype = PAMI_TYPE_BYTE;
   #else
   gatherv.cmd.xfer_gatherv_int.rtype = stype;
   gatherv.cmd.xfer_gatherv_int.stype = rtype;
   #endif

   /* Deal with the new arrays */
   #if defined(PAMI_DISPS_ARE_BYTES) || defined(PAMI_BYTES_REQUIRED)
   int i = 0;
   for(i = 0; i < comm_ptr->local_size; i++)
   {
      #ifdef PAMI_DISPS_ARE_BYTES
      rdispls[i] = displs[i] * rsize;
      #endif
      #ifdef PAMI_BYTES_REQUIRED
      rcounts[i] = recvcounts[i] * rsize;
      #endif
   }
   #endif

   /* Deal with datatypes */
   #ifdef PAMI_BYTES_REQUIRED
   gatherv.cmd.xfer_gatherv_int.stypecount = sendcount * ssize;
   gatherv.cmd.xfer_gatherv_int.rtypecounts = rcounts;
   #else
   gatherv.cmd.xfer_gatherv_int.stypecount = sendcount;
   gatherv.cmd.xfer_gatherv_int.rtypecounts = recvcounts;
   #endif

   #ifdef PAMI_DISPS_ARE_BYTES
   gatherv.cmd.xfer_gatherv_int.rdispls = rdispls;
   #else
   gatherv.cmd.xfer_gatherv_int.rdispls = displs;
   #endif

   
   MPIDI_Update_last_algorithm(comm_ptr,
      comm_ptr->mpid.user_metadata[PAMI_XFER_GATHERV_INT].name);
   if(MPIDI_Process.context_post)
   {
      MPIDI_Post_coll_t gatherv_post;
      TRACE_ERR("Posting gatherv\n");
      gatherv_post.coll_struct = &gatherv;
      rc = PAMI_Context_post(MPIDI_Context[0], &gatherv_post.state, 
                                 MPIDI_Pami_post_wrapper, (void *)&gatherv_post);
      TRACE_ERR("Gatherv posted; rc: %d\n", rc);
   }
   else
   {
      TRACE_ERR("Calling PAMI_Collective for gatherv\n");
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&gatherv);
   }
   
   TRACE_ERR("Waiting on active %d\n", gatherv_active);
   MPID_PROGRESS_WAIT_WHILE(gatherv_active);

#ifdef PAMI_BYTES_REQUIRED
   TRACE_ERR("Freeing memory for rcounts\n");
   MPIU_Free(rcounts);
#endif
#ifdef PAMI_DISPS_ARE_BYTES
   TRACE_ERR("Freeing memory for rdispls\n");
   MPIU_Free(rdispls);
#endif

   TRACE_ERR("Leaving MPIDO_Gatherv\n");
   return rc;
}
