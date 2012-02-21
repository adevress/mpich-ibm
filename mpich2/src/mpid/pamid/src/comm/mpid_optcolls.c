/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/comm/mpid_optcolls.c
 * \brief Code for setting up optimized collectives per geometry/communicator
 * \brief currently uses strcmp and known protocol performance
 */



//#define TRACE_ON

#include <mpidimpl.h>

#ifdef MPIDI_BASIC_COLLECTIVE_SELECTION
void MPIDI_Comm_coll_select(MPID_Comm *comm_ptr)
{
   TRACE_ERR("Entering MPIDI_Comm_coll_select\n");
   int opt_proto = -1;
   int i;


   /* So, several protocols are really easy. Tackle them first. */
   /* Alltoallv */
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] == MPID_COLL_NOSELECTION)
   {
      TRACE_ERR("No alltoallv env var, so setting optimized alltoallv\n");
      /* The best alltoallv is always I0:M2MComposite:MU:MU */
      for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_ALLTOALLV_INT][0]; i++)
      {
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][i].name, "I0:M2MComposite:MU:MU") == 0)
         {
            opt_proto = i;
            break;
         }
      }
      if(opt_proto != -1)
      {
         TRACE_ERR("Memcpy protocol type %d number %d (%s) to optimized protocol\n",
            PAMI_XFER_ALLTOALLV_INT, opt_proto,
            comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][opt_proto].name);
         comm_ptr->mpid.opt_protocol[PAMI_XFER_ALLTOALLV_INT][0] =
                comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLTOALLV_INT][0][opt_proto];
         memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLTOALLV_INT][0], 
                &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][opt_proto], 
                sizeof(pami_metadata_t));
         comm_ptr->mpid.must_query[PAMI_XFER_ALLTOALLV_INT][0] = MPID_COLL_NOQUERY;
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] = MPID_COLL_SELECTED;
      }
      else
      {
         TRACE_ERR("Couldn't find optimial alltoallv protocol\n");
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] = MPID_COLL_USE_MPICH;
      }
      TRACE_ERR("Done setting optimized alltoallv\n");
   }

   /* Alltoall */
   /* If the user has forced a selection, don't bother setting it here */
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] == MPID_COLL_NOSELECTION)
   {
      TRACE_ERR("No alltoall env var, so setting optimized alltoall\n");
      /* the best alltoall is always I0:M2MComposite:MU:MU, though there are
       * displacement array memory issues today.... */
      /* Loop over the protocols until we find the one we want */
      for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_ALLTOALL][0]; i++)
      {
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALL][0][i].name, "I0:M2MComposite:MU:MU") == 0)
         {
            opt_proto = i;
            break;
         }
      }
      if(opt_proto != -1)
      {
         TRACE_ERR("Memcpy protocol type %d, number %d (%s) to optimized protocol\n",
            PAMI_XFER_ALLTOALL, opt_proto, 
            comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALL][0][opt_proto].name);
         comm_ptr->mpid.opt_protocol[PAMI_XFER_ALLTOALL][0] =
                comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLTOALL][0][opt_proto];
         memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLTOALL][0], 
                &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALL][0][opt_proto], 
                sizeof(pami_metadata_t));
         comm_ptr->mpid.must_query[PAMI_XFER_ALLTOALL][0] = MPID_COLL_NOQUERY;
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] = MPID_COLL_SELECTED;
      }
      else
      {
         TRACE_ERR("Couldn't find I0:M2MComposite:MU:MU in the list for alltoall, reverting to MPICH\n");
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] = MPID_COLL_USE_MPICH;
      }
      TRACE_ERR("Done setting optimized alltoall\n");
   }


   opt_proto = -1;
   /* Alltoallv */
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] == MPID_COLL_NOSELECTION)
   {
      TRACE_ERR("No alltoallv env var, so setting optimized alltoallv\n");
      /* the best alltoallv is always I0:M2MComposite:MU:MU, though there are
       * displacement array memory issues today.... */
      /* Loop over the protocols until we find the one we want */
      for(i = 0; i <comm_ptr->mpid.coll_count[PAMI_XFER_ALLTOALLV_INT][0]; i++)
      {
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][i].name, "I0:M2MComposite:MU:MU") == 0)
         {
            opt_proto = i;
            break;
         }
      }
      if(opt_proto != -1)
      {
         TRACE_ERR("Memcpy protocol type %d, number %d (%s) to optimized protocol\n",
            PAMI_XFER_ALLTOALLV_INT, opt_proto, 
            comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][opt_proto].name);
         comm_ptr->mpid.opt_protocol[PAMI_XFER_ALLTOALLV_INT][0] =
                comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLTOALLV_INT][0][opt_proto];
         memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLTOALLV_INT][0], 
                &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLTOALLV_INT][0][opt_proto], 
                sizeof(pami_metadata_t));
         comm_ptr->mpid.must_query[PAMI_XFER_ALLTOALLV_INT][0] = MPID_COLL_NOQUERY;
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] = MPID_COLL_SELECTED;
      }
      else
      {
         TRACE_ERR("Couldn't find I0:M2MComposite:MU:MU in the list for alltoallv, reverting to MPICH\n");
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALLV_INT] = MPID_COLL_USE_MPICH;
      }
      TRACE_ERR("Done setting optimized alltoallv\n");
   }
   
   opt_proto = -1;
   /* Barrier */
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] == MPID_COLL_NOSELECTION)
   {
      TRACE_ERR("No barrier env var, so setting optimized barrier\n");
      /* For 1ppn, I0:MultiSync:-:GI is best, followed by
       * I0:RectangleMultiSync:-:MU, followed by
       * I0:OptBinomial:P2P:P2P
       */
       /* For 16 and 64 ppn, I0:MultiSync2Device:SHMEM:GI (which doesn't exist at 1ppn)
       * is best, followed by
       * I0:RectangleMultiSync2Device:SHMEM:MU for rectangles, followed by
       * I0:OptBinomial:P2P:P2P */
      /* So we basically check for the protocols in reverse-optimal order */


         /* In order, >1ppn we use
          * I0:MultiSync2Device:SHMEM:GI
          * I0:RectangleMultiSync2Device:SHMEM:MU
          * I0:OptBinomial:P2P:P2P
          * MPICH
          *
          * In order 1ppn we use
          * I0:MultiSync:-:GI
          * I0:RectangleMultiSync:-:MU
          * I0:OptBinomial:P2P:P2P
          * MPICH
          */
      for(i = 0 ; i < comm_ptr->mpid.coll_count[PAMI_XFER_BARRIER][0]; i++)
      {
         /* These two are mutually exclusive */
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name, "I0:MultiSync2Device:SHMEM:GI") == 0)
         {
            opt_proto = i;
         }
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name, "I0:MultiSync:-:GI") == 0)
         {
            opt_proto = i;
         }
      }
      /* Next best rectangular to check */
      if(opt_proto == -1)
      {
         for(i = 0 ; i < comm_ptr->mpid.coll_count[PAMI_XFER_BARRIER][0]; i++)
         {
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name, "I0:RectangleMultiSync2Device:SHMEM:MU") == 0)
               opt_proto = i;
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name, "I0:RectangleMultiSync:-:MU") == 0)
               opt_proto = i;
         }
      }
      /* Finally, see if we have opt binomial */
      if(opt_proto == -1)
      {
         for(i = 0 ; i < comm_ptr->mpid.coll_count[PAMI_XFER_BARRIER][0]; i++)
         {
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name, "I0:OptBinomial:P2P:P2P") == 0)
               opt_proto = i;
         }
      }

      if(opt_proto != -1)
      {
         TRACE_ERR("Memcpy protocol type %d, number %d (%s) to optimize protocol\n",
            PAMI_XFER_BARRIER, opt_proto, 
            comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][opt_proto].name);

         comm_ptr->mpid.opt_protocol[PAMI_XFER_BARRIER][0] =
                comm_ptr->mpid.coll_algorithm[PAMI_XFER_BARRIER][0][opt_proto]; 
         memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BARRIER][0], 
                &comm_ptr->mpid.coll_metadata[PAMI_XFER_BARRIER][0][opt_proto], 
                sizeof(pami_metadata_t));
         comm_ptr->mpid.must_query[PAMI_XFER_BARRIER][0] = MPID_COLL_NOQUERY;
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] = MPID_COLL_SELECTED;
      }
      else
      {
         TRACE_ERR("Couldn't find any optimal barrier protocols. Using MPICH\n");
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] = MPID_COLL_USE_MPICH;
      }

      TRACE_ERR("Done setting optimized barrier\n");
   }

   opt_proto = -1;

   /* This becomes messy when we have to message sizes. If we were gutting the 
    * existing framework, it might be easier, but I think the existing framework
    * is useful for future collective selection libraries/mechanisms, so I'd
    * rather leave it in place and deal with it here instead.... */

   /* Broadcast */
   /* 1ppn */
   /* small messages: I0:MulticastDput:-:MU
    * if it exists, I0:RectangleDput:MU:MU for >64k */

   /* 16ppn */
   /* small messages: I0:MultiCastDput:SHMEM:MU
    * for 16ppn/1node: I0:MultiCastDput:SHMEM:- perhaps
    * I0:RectangleDput:SHMEM:MU for >128k */
   /* nonrect(?): I0:MultiCast2DeviceDput:SHMEM:MU 
    * no hw: I0:Sync2-nary:Shmem:MUDput */
   /* 64ppn */
   /* all sizes: I0:MultiCastDput:SHMEM:MU */
   /* otherwise, I0:2-nomial:SHMEM:MU */



   /* First, set up small message bcasts */
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_NOSELECTION)
   {
      /* Note: Neither of these protocols are in a 'must query' list so
       * the for() loop only needs to loop over [0] protocols */
      /* Complicated exceptions: */
      /* I0:RankBased_Binomial:-:ShortMU is good on irregular for <256 bytes */
      /* I0:MultiCast:SHMEM:- is good at 1 node/16ppn, which is a SOW point */
      TRACE_ERR("No bcast env var, so setting optimized bcast\n");
      for(i = 0 ; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
      {
         /* These two are mutually exclusive */
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:MultiCastDput:-:MU") == 0)
            opt_proto = i;
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:MultiCastDput:SHMEM:MU") == 0)
            opt_proto = i;
      }
      /* Next best rectangular to check */
      if(opt_proto == -1)
      {
         /* This is also NOT in the 'must query' list */
         for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
         {
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:MultiCast2DeviceDput:SHMEM:MU") == 0)
               opt_proto = i;
         }
      }
      if(opt_proto == -1)
      {
         for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][1]; i++)
         {
            /* This is a good choice for small messages only */
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][1][i].name, "I0:RankBased_Binomial:SHMEM:MU") == 0)
            {
               opt_proto = i;
               comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0] = 256;
            }
         }
      }
      /* Next best to check */
      if(opt_proto == -1)
      {
         for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
         {
            /* Also, NOT in the 'must query' list */
            if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:2-nomial:SHMEM:MU") == 0)
               opt_proto = i;
         }
      }
      
      /* These protocols are good for most message sizes, but there are some
       * better choices for larger messages */
      /* Set opt_proto for bcast[0] right now */
      if(opt_proto != -1)
      {
         TRACE_ERR("Memcpy protocol type %d, number %d (%s) to optimize protocol 0\n",
            PAMI_XFER_BROADCAST, opt_proto, 
            comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][opt_proto].name);

         comm_ptr->mpid.opt_protocol[PAMI_XFER_BROADCAST][0] = 
                comm_ptr->mpid.coll_algorithm[PAMI_XFER_BROADCAST][0][opt_proto];
         memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0], 
                &comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][opt_proto], 
                sizeof(pami_metadata_t));
         comm_ptr->mpid.must_query[PAMI_XFER_BROADCAST][0] = MPID_COLL_NOQUERY;
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] = MPID_COLL_SELECTED;
      }
      else
      {
         TRACE_ERR("Couldn't find any optimal barrier protocols. Using MPICH\n");
         comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] = MPID_COLL_USE_MPICH;
      }

      TRACE_ERR("Done setting optimized bcast 0\n");


      /* Now, look into large message bcasts */
      opt_proto = -1;
      /* If bcast0 is I0:MultiCastDput:-:MU, and I0:RectangleDput:MU:MU is available, use
       * it for 64k messages */
      /* Again, none of these protocols are in the 'must query' list */
      if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] != MPID_COLL_USE_MPICH)
      {
         if(strcasecmp(comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0].name, "I0:MultiCastDput:-:MU") == 0)
         {
            /* See if I0:RectangleDput:MU:MU is available */
            for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
            {
               if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:RectangleDput:MU:MU") == 0)
               {
                  opt_proto = i;
                  comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0] = 65536;
               }
            }
         }
          /* small messages: I0:MultiCastDput:SHMEM:MU*/
          /* I0:RectangleDput:SHMEM:MU for >128k */
         if(strcasecmp(comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0].name, "I0:MultiCastDput:SHMEM:MU") == 0)
         {
            /* See if I0:RectangleDput:SHMEM:MU is available */
            for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
            {
               if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:RectangleDput:SHMEM:MU") == 0)
               {
                  opt_proto = i;
                  comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0] = 131072;
               }
            }
         }
         if(strcasecmp(comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0].name, "I0:RankBased_Binomial:SHMEM:MU") == 0)
         {
            /* This protocol was only good for up to 256, and it was an irregular, so let's set
             * 2-nomial for larger message sizes. Cutoff should have already been set to 256 too */
            for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_BROADCAST][0]; i++)
            {
               if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name, "I0:2-nomial:SHMEM:MU") == 0)
                  opt_proto = i;
            }
         }

         if(opt_proto != -1)
         {
            if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
            {
               fprintf(stderr,"Using %s as optimal broadcast 1 (above %d)\n", 
                  comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][opt_proto].name, 
                  comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0]);
            }
            TRACE_ERR("Memcpy protocol type %d, number %d (%s) to optimize protocol 1 (above %d)\n",
               PAMI_XFER_BROADCAST, opt_proto, 
               comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][opt_proto].name,
               comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0]);

            comm_ptr->mpid.opt_protocol[PAMI_XFER_BROADCAST][1] =
                    comm_ptr->mpid.coll_algorithm[PAMI_XFER_BROADCAST][0][opt_proto];
            memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][1], 
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][opt_proto], 
                   sizeof(pami_metadata_t));
            comm_ptr->mpid.must_query[PAMI_XFER_BROADCAST][1] = MPID_COLL_NOQUERY;
            /* This should already be set... */
            /* comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] = MPID_COLL_SELECTED; */
         }
         else
         {
            TRACE_ERR("Secondary bcast protocols unavilable; using primary for all sizes\n");
            
            TRACE_ERR("Duplicating protocol type %d, number %d (%s) to optimize protocol 1 (above %d)\n",
               PAMI_XFER_BROADCAST, 0, 
               comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0].name,
               comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0]);

            comm_ptr->mpid.opt_protocol[PAMI_XFER_BROADCAST][1] = 
                    comm_ptr->mpid.opt_protocol[PAMI_XFER_BROADCAST][0];
            memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][1], 
                   &comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0],
                   sizeof(pami_metadata_t));
            comm_ptr->mpid.must_query[PAMI_XFER_BROADCAST][1] = MPID_COLL_NOQUERY;
         }
      }
      TRACE_ERR("Done with bcast protocol selection\n");

   }

   /* The most fun... allreduce */
   /* 512-way data: */
   /* For starters, Amith's protocol works on doubles on sum/min/max. Because
    * those are targetted types/ops, we will pre-cache it.
    * That protocol works on ints, up to 8k/ppn max message size. We'll precache 
    * it too
    */
   
   if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] == MPID_COLL_NOSELECTION)
   {
      /* the user hasn't selected a protocol, so we can NULL the protocol/metadatas */
      comm_ptr->mpid.query_allred_dsmm = MPID_COLL_USE_MPICH;
      comm_ptr->mpid.query_allred_ismm = MPID_COLL_USE_MPICH;

      comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0] = 128;

      /* 1ppn: I0:MultiCombineDput:-:MU if it is available, but it has a check_fn
       * since it is MU-based*/
      /* Next best is I1:ShortAllreduce:P2P:P2P for short messages, then MPICH is best*/
      /* I0:MultiCombineDput:-:MU could be used in the i/dsmm cached protocols, so we'll do that */
      /* First, look in the 'must query' list and see i I0:MultiCombine:Dput:-:MU is there */
      for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_ALLREDUCE][1]; i++)
      {
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i].name, "I0:MultiCombineDput:-:MU") == 0)
         {
            /* So, this should be fine for the i/dsmm protocols. everything else needs to call the check function */
            /* This also works for all message sizes, so no need to deal with it specially for query */
            comm_ptr->mpid.cached_allred_dsmm = 
                   comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][1][i];
            memcpy(&comm_ptr->mpid.cached_allred_dsmm_md,
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i],
                  sizeof(pami_metadata_t));
            comm_ptr->mpid.query_allred_dsmm = MPID_COLL_QUERY;

            comm_ptr->mpid.cached_allred_ismm =
                   comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][1][i];
            memcpy(&comm_ptr->mpid.cached_allred_ismm_md,
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i],
                  sizeof(pami_metadata_t));
            comm_ptr->mpid.query_allred_ismm = MPID_COLL_QUERY;
            comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] = MPID_COLL_SELECTED;
            opt_proto = i;

         }
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i].name, "I0:MultiCombineDput:SHMEM:MU") == 0)
         {
            /* This works well for doubles sum/min/max but has trouble with int > 8k/ppn */
            comm_ptr->mpid.cached_allred_dsmm =
                   comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][1][i];
            memcpy(&comm_ptr->mpid.cached_allred_dsmm_md,
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i],
                  sizeof(pami_metadata_t));
            comm_ptr->mpid.query_allred_dsmm = MPID_COLL_QUERY;

            comm_ptr->mpid.cached_allred_ismm =
                   comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][1][i];
            memcpy(&comm_ptr->mpid.cached_allred_ismm_md,
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i],
                  sizeof(pami_metadata_t));
            comm_ptr->mpid.query_allred_ismm = MPID_COLL_CHECK_FN_REQUIRED;
            comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] = MPID_COLL_SELECTED;
            /* I don't think MPIX_HW is initialized yet, so keep this at 128 for now, which 
             * is the upper lower limit anyway (8192 / 64) */
            /* comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0] = 8192 / ppn; */
            opt_proto = i;
         }
      }
      /* At this point, if opt_proto != -1, we have must-query protocols in the i/dsmm caches */
      /* We should pick a backup, non-must query */
      /* I0:ShortAllreduce:P2P:P2P < 128, then mpich*/
      for(i = 0; i < comm_ptr->mpid.coll_count[PAMI_XFER_ALLREDUCE][0]; i++)
      {
         if(strcasecmp(comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][0][i].name, "I0:ShortAllreduce:P2P:P2P") == 0)
         {
            comm_ptr->mpid.opt_protocol[PAMI_XFER_ALLREDUCE][0] =
                   comm_ptr->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][0][i];
            memcpy(&comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLREDUCE][0],
                   &comm_ptr->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][0][i],
                   sizeof(pami_metadata_t));
            comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][0] = MPID_COLL_NOQUERY;
            /* Short is good for up to 128 */
            comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0] = 128;
            comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] = MPID_COLL_SELECTED;
            /* MPICH is actually better at > 128 bytes for 1/16/64ppn at 512 nodes */
            comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][1] = MPID_COLL_USE_MPICH;
            opt_proto = -1;
         }
      }
      if(opt_proto == -1)
      {
         comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][0] = MPID_COLL_USE_MPICH;
         comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][1] = MPID_COLL_USE_MPICH;
      }
      TRACE_ERR("Done setting optimized allreduce protocols\n");
   }
      
            
   if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm_ptr->rank == 0)
   {
      if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BARRIER] == MPID_COLL_SELECTED)
         fprintf(stderr,"Using %s for opt barrier comm %p\n", comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BARRIER][0].name, comm_ptr);
      if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_BROADCAST] == MPID_COLL_SELECTED)
         fprintf(stderr,"Using %s for opt bcast up to size %d comm %p\n", comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][0].name,
            comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0], comm_ptr);
      if(comm_ptr->mpid.must_query[PAMI_XFER_BROADCAST][1] == MPID_COLL_NOQUERY)
         fprintf(stderr,"Using %s for opt bcast above size %d comm %p\n", comm_ptr->mpid.opt_protocol_md[PAMI_XFER_BROADCAST][1].name,
            comm_ptr->mpid.cutoff_size[PAMI_XFER_BROADCAST][0], comm_ptr);
      if(comm_ptr->mpid.user_selectedvar[PAMI_XFER_ALLTOALL] == MPID_COLL_SELECTED)
         fprintf(stderr,"Using %s for opt alltoall comm %p\n", comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLTOALL][0].name, comm_ptr);
      if(comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][0] == MPID_COLL_USE_MPICH)
         fprintf(stderr,"Using MPICH for allreduce below %d size comm %p\n", comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
      else
      {
         if(comm_ptr->mpid.query_allred_ismm != MPID_COLL_USE_MPICH)
         {
            fprintf(stderr,"Using %s for integer sum/min/max ops, query: %d comm %p\n",
               comm_ptr->mpid.cached_allred_ismm_md.name, comm_ptr->mpid.query_allred_ismm, comm_ptr);
         }
         if(comm_ptr->mpid.query_allred_dsmm != MPID_COLL_USE_MPICH)
         {
            fprintf(stderr,"Using %s for double sum/min/max ops, query: %d comm %p\n",
               comm_ptr->mpid.cached_allred_dsmm_md.name, comm_ptr->mpid.query_allred_dsmm, comm_ptr);
         }
         fprintf(stderr,"Using %s for other operations up to %d comm %p\n",
               comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLREDUCE][0].name, 
               comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
      }
      if(comm_ptr->mpid.must_query[PAMI_XFER_ALLREDUCE][1] == MPID_COLL_USE_MPICH)
         fprintf(stderr,"Using MPICH for allreduce above %d size comm %p\n", comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
      else
      {
         if(comm_ptr->mpid.query_allred_ismm != MPID_COLL_USE_MPICH)
         {
            fprintf(stderr,"Using %s for integer sum/min/max ops, above %d query: %d comm %p\n",
               comm_ptr->mpid.cached_allred_ismm_md.name, 
               comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0],
               comm_ptr->mpid.query_allred_ismm, comm_ptr);
         }
         else
         {
            fprintf(stderr,"Using MPICH for integer sum/min/max ops above %d size comm %p\n",
               comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
         }
         if(comm_ptr->mpid.query_allred_dsmm != MPID_COLL_USE_MPICH)
         {
            fprintf(stderr,"Using %s for double sum/min/max ops, above %d query: %d comm %p\n",
               comm_ptr->mpid.cached_allred_dsmm_md.name, 
               comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0],
               comm_ptr->mpid.query_allred_dsmm, comm_ptr);
         }
         else
         {
            fprintf(stderr,"Using MPICH for double sum/min/max ops above %d size comm %p\n",
               comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
         }
         fprintf(stderr,"Using %s for other operations over %d comm %p\n",
            comm_ptr->mpid.opt_protocol_md[PAMI_XFER_ALLREDUCE][1].name,
            comm_ptr->mpid.cutoff_size[PAMI_XFER_ALLREDUCE][0], comm_ptr);
      }
   }

   TRACE_ERR("Leaving MPIDI_Comm_coll_select\n");

}
#endif
