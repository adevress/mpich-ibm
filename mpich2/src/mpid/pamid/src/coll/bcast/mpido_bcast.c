/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/bcast/mpido_bcast.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void cb_bcast(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR("callback enter, active: %d\n", (*active));
   MPIDI_Progress_signal();
   (*active)--;
}

int MPIDO_Bcast(void *buffer,
                int count,
                MPI_Datatype datatype,
                int root,
                MPID_Comm *comm_ptr,
                int *mpierrno)
{
   TRACE_ERR("in mpido_bcast\n");
   int data_size, data_contig, rc;
   void *data_buffer    = NULL,
        *noncontig_buff = NULL;
   volatile unsigned active = 1;
   MPI_Aint data_true_lb = 0;
   MPID_Datatype *data_ptr;
   MPID_Segment segment;
   MPIDI_Post_coll_t bcast_post;
//   MPIDI_Post_coll_t allred_post; // eventually for preallreduces
   if(count == 0)
   {
      MPIDI_Update_last_algorithm(comm_ptr,"BCAST_NONE");
      return MPI_SUCCESS;
   }
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_USE_MPICH)
     return MPIR_Bcast_intra(buffer, count, datatype, root, comm_ptr, mpierrno);

   MPIDI_Datatype_get_info(count, datatype,
               data_contig, data_size, data_ptr, data_true_lb);

   data_buffer = buffer + data_true_lb;

   if(!data_contig)
   {
      if(comm_ptr->rank == root)
         TRACE_ERR("noncontig data\n");
      noncontig_buff = MPIU_Malloc(data_size);
      data_buffer = noncontig_buff;
      if(noncontig_buff == NULL)
      {
         fprintf(stderr,
            "Pack: Tree Bcast cannot allocate local non-contig pack buffer\n");
//         MPIX_Dump_stacks();
         MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
            "Fatal:  Cannot allocate pack buffer");
      }
      if(comm_ptr->rank == root)
      {
         DLOOP_Offset last = data_size;
         MPID_Segment_init(buffer, count, datatype, &segment, 0);
         MPID_Segment_pack(&segment, 0, &last, noncontig_buff);
      }
   }

   pami_xfer_t bcast;
   bcast.cb_done = cb_bcast;
   bcast.cookie = (void *)&active;
   bcast.cmd.xfer_broadcast.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
   bcast.algorithm = comm_ptr->mpid.user_selected[PAMI_XFER_BROADCAST];
   bcast.cmd.xfer_broadcast.buf = data_buffer;
   bcast.cmd.xfer_broadcast.type = PAMI_TYPE_BYTE;
   /* Needs to be sizeof(type)*count since we are using bytes as * the generic type */
   bcast.cmd.xfer_broadcast.typecount = data_size;
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] >= MPID_COLL_QUERY)
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying bcast protocol %s, type was: %d\n",
         comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].name, 
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST]);
      result = comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].check_fn(&bcast);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"query failed for %s.\n",
         comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].name);
      }
   }


   TRACE_ERR("posting bcast, context: %d, algoname: %s\n",0, 
         comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].name);
   MPIDI_Update_last_algorithm(comm_ptr, 
         comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].name);

   if(MPIDI_Process.context_post)
   {
      bcast_post.coll_struct = &bcast;
      rc = PAMI_Context_post(MPIDI_Context[0], &bcast_post.state, MPIDI_Pami_post_wrapper, (void *)&bcast_post);

      TRACE_ERR("bcast posted, rc: %d\n", rc);
   }
   else
   {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL)
         fprintf(stderr,"Using protocol %s.\n",comm_ptr->mpid.user_metadata[PAMI_XFER_BROADCAST].name);
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&bcast);
   }

   MPID_PROGRESS_WAIT_WHILE(active);
   TRACE_ERR("bcast done\n");

   if(!data_contig)
   {
      TRACE_ERR("cleaning up noncontig\n");
      if(comm_ptr->rank != root)
         MPIR_Localcopy(noncontig_buff, data_size, MPI_CHAR,
                        buffer,         count,     datatype);
      MPIU_Free(noncontig_buff);
   }

   TRACE_ERR("leaving bcast\n");
   return rc;
}
