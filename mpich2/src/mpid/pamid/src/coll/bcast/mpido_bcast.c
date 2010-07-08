/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/coll/bcast/mpido_bcast.c
 * \brief ???
 */


#include "mpidimpl.h"

static void cb_bcast(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR("callback enter, active: %d\n", (*active));
   (*active)--;
}

int MPIDO_Bcast(void *buffer,
                int count,
                MPI_Datatype datatype,
                int root,
                MPID_Comm *comm_ptr)
{
   TRACE_ERR("in mpido_bcast\n");
   int data_size, data_contig, rc;
   void *data_buffer    = NULL,
        *noncontig_buff = NULL;
   volatile unsigned active = 1;
   MPI_Aint data_true_lb = 0;
   MPID_Datatype *data_ptr;
   MPID_Segment segment;
   if(count == 0)
   {
      MPIDI_Update_last_algorithm(comm_ptr,"BCAST_NONE");
      return MPI_SUCCESS;
   }

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
   bcast.algorithm = comm_ptr->mpid.bcasts[0];
   bcast.cmd.xfer_broadcast.root = comm_ptr->vcr[root];
   bcast.cmd.xfer_broadcast.buf = data_buffer;
   bcast.cmd.xfer_broadcast.type = PAMI_BYTE;
   /* Needs to be sizeof(type)*count since we are using bytes as * the generic type */
   bcast.cmd.xfer_broadcast.typecount = data_size;

   TRACE_ERR("posting bcast, context: %d, algoname: %s\n",0, comm_ptr->mpid.bcast_metas[0].name);
   MPIDI_Update_last_algorithm(comm_ptr, comm_ptr->mpid.bcast_metas[0].name);
   rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&bcast);
   TRACE_ERR("bcast posted, rc: %d\n", rc);

   assert(rc == PAMI_SUCCESS);

   while(active)
   {
      static int spin = 0;
      if(spin %1000 == 0)
         TRACE_ERR("spinning, %d\n", spin);
      rc = PAMI_Context_advance(MPIDI_Context[0], 1);
      spin++;
   }
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
