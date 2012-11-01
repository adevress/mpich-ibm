/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
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
   const size_t BCAST_LIMIT = 0x40000000;
   int data_size, data_contig, rc;
   void *data_buffer    = NULL,
        *noncontig_buff = NULL;
   volatile unsigned active = 1;
   MPI_Aint data_true_lb = 0;
   MPID_Datatype *data_ptr;
   MPID_Segment segment;
   MPIDI_Post_coll_t bcast_post;
   struct MPIDI_Comm *mpid = &(comm_ptr->mpid);
   int rank = comm_ptr->rank;


   MPIDI_Datatype_get_info(count, datatype,
               data_contig, data_size, data_ptr, data_true_lb);

   if(count == 0)
   {
      MPIDI_Update_last_algorithm(comm_ptr,"BCAST_NONE");
      return MPI_SUCCESS;
   }
   if(mpid->user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_USE_MPICH || data_size == 0)
   {
     if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && rank == 0))
       fprintf(stderr,"Using MPICH bcast algorithm\n");
      return MPIR_Bcast_intra(buffer, count, datatype, root, comm_ptr, mpierrno);
   }

   if(unlikely( (size_t)data_size > BCAST_LIMIT) )
   {
      int data_size_one = data_size/count;
      void *new_buffer=buffer;
      int new_count, c;
      new_count = (int)BCAST_LIMIT/data_size_one;
      MPID_assert(new_count > 0);

      for(c=1; ((size_t)c*(size_t)new_count) <= (size_t)count; ++c)
      {
        if ((rc = MPIDO_Bcast(new_buffer,
                        new_count,
                        datatype,
                        root,
                        comm_ptr,
                              mpierrno)) != MPI_SUCCESS)
         return rc;
	 new_buffer = (char*)new_buffer + (size_t)data_size_one*(size_t)new_count;
      }
      new_count = count % new_count; /* 0 is ok, just returns no-op */
      return MPIDO_Bcast(new_buffer,
                         new_count,
                         datatype,
                         root,
                         comm_ptr,
                         mpierrno);
   }

   //   if(mpid->user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_USE_MPICH || data_size == 0)
   //{
     //if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && rank == 0))
     //  fprintf(stderr,"Using MPICH bcast algorithm\n");
     // return MPIR_Bcast_intra(buffer, count, datatype, root, comm_ptr, mpierrno);
   //}

   /* If the user has constructed some weird 0-length datatype but 
    * count is not 0, we'll let mpich handle it */
   //      if(unlikely( data_size == 0) )
   //{
     // if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && rank == 0))
   //    fprintf(stderr,"Using MPICH bcast algorithm for data_size 0\n");
   //return MPIR_Bcast_intra(buffer, count, datatype, root, comm_ptr, mpierrno);
   //}
   data_buffer = buffer + data_true_lb;

   if(!data_contig)
   {
     if(rank == root)
       TRACE_ERR("noncontig data\n");
      noncontig_buff = MPIU_Malloc(data_size);
      data_buffer = noncontig_buff;
      if(noncontig_buff == NULL)
      {
        //    fprintf(stderr,
        //  "Pack: Tree Bcast cannot allocate local non-contig pack buffer\n");
//         MPIX_Dump_stacks();
         MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
            "Fatal:  Cannot allocate pack buffer");
      }
      if(rank == root)
      {
         DLOOP_Offset last = data_size;
         MPID_Segment_init(buffer, count, datatype, &segment, 0);
         MPID_Segment_pack(&segment, 0, &last, noncontig_buff);
      }
   }

   pami_xfer_t bcast;
   pami_algorithm_t my_bcast;
   pami_metadata_t *my_bcast_md;
   int queryreq = 0;

   bcast.cb_done = cb_bcast;
   bcast.cookie = (void *)&active;
   bcast.cmd.xfer_broadcast.root = MPID_VCR_GET_LPID(comm_ptr->vcr, root);
   bcast.algorithm = mpid->user_selected[PAMI_XFER_BROADCAST];
   bcast.cmd.xfer_broadcast.buf = data_buffer;
   bcast.cmd.xfer_broadcast.type = PAMI_TYPE_BYTE;
   /* Needs to be sizeof(type)*count since we are using bytes as * the generic type */
   bcast.cmd.xfer_broadcast.typecount = data_size;

#ifdef MPIDI_BASIC_COLLECTIVE_SELECTION
   if(mpid->user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_SELECTED)
   {
      TRACE_ERR("Optimized bcast (%s) and (%s) were pre-selected\n",
         mpid->opt_protocol_md[PAMI_XFER_BROADCAST][0].name,
         mpid->opt_protocol_md[PAMI_XFER_BROADCAST][1].name);

      if(data_size > mpid->cutoff_size[PAMI_XFER_BROADCAST][0])
      {
         my_bcast = mpid->opt_protocol[PAMI_XFER_BROADCAST][1];
         my_bcast_md = &mpid->opt_protocol_md[PAMI_XFER_BROADCAST][1];
         queryreq = mpid->must_query[PAMI_XFER_BROADCAST][1];
      }
      else
      {
         my_bcast = mpid->opt_protocol[PAMI_XFER_BROADCAST][0];
         my_bcast_md = &mpid->opt_protocol_md[PAMI_XFER_BROADCAST][0];
         queryreq = mpid->must_query[PAMI_XFER_BROADCAST][0];
      }
   }
   else
#endif
   {
      TRACE_ERR("Optimized bcast (%s) was specified by user\n",
         mpid->user_metadata[PAMI_XFER_BROADCAST].name);
      my_bcast =  mpid->user_selected[PAMI_XFER_BROADCAST];
      my_bcast_md = &mpid->user_metadata[PAMI_XFER_BROADCAST];
      queryreq = mpid->user_selectedvar[PAMI_XFER_BROADCAST];
   }

   bcast.algorithm = my_bcast;

   if(queryreq == MPID_COLL_ALWAYS_QUERY || queryreq == MPID_COLL_CHECK_FN_REQUIRED)
   {
      metadata_result_t result = {0};
      TRACE_ERR("querying bcast protocol %s, type was: %d\n",
         my_bcast_md->name, queryreq);
      result = my_bcast_md->check_fn(&bcast);
      TRACE_ERR("bitmask: %#X\n", result.bitmask);
      if(!result.bitmask)
      {
         fprintf(stderr,"query failed for %s.\n", my_bcast_md->name);
      }
   }


   TRACE_ERR("posting bcast, context: %d, algoname: %s\n",0, 
      my_bcast_md->name);
   MPIDI_Update_last_algorithm(comm_ptr, my_bcast_md->name);

   if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL && rank == 0))
   {
      unsigned long long int threadID;
      MPIU_Thread_id_t tid;
      MPIU_Thread_self(&tid);
      threadID = (unsigned long long int)tid;
      fprintf(stderr,"<%llx> Using protocol %s for bcast on %u\n", 
              threadID,
              my_bcast_md->name,
              (unsigned) comm_ptr->context_id);
   }
   if(MPIDI_Process.context_post)
   {
      bcast_post.coll_struct = &bcast;
      rc = PAMI_Context_post(MPIDI_Context[0], &bcast_post.state, MPIDI_Pami_post_wrapper, (void *)&bcast_post);

      TRACE_ERR("bcast posted, rc: %d\n", rc);
   }
   else
   {
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&bcast);
   }

   MPID_PROGRESS_WAIT_WHILE(active);
   TRACE_ERR("bcast done\n");

   if(!data_contig)
   {
      TRACE_ERR("cleaning up noncontig\n");
      if(rank != root)
         MPIR_Localcopy(noncontig_buff, data_size, MPI_CHAR,
                        buffer,         count,     datatype);
      MPIU_Free(noncontig_buff);
   }

   TRACE_ERR("leaving bcast\n");
   return rc;
}
