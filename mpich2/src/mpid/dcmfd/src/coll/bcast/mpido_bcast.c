/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid/dcmfd/src/coll/bcast/mpido_bcast.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"
#include "mpix.h"


#ifdef USE_CCMI_COLL

#pragma weak PMPIDO_Bcast = MPIDO_Bcast

static void allred_cb_done(void *clientdata, DCMF_Error_t *err)
{
   volatile unsigned *work_left = (unsigned *)clientdata;
   *work_left=0;
   MPID_Progress_signal();
}


int
MPIDO_Bcast(void *buffer,
            int count, MPI_Datatype datatype, int root, MPID_Comm * comm)
{
  bcast_fptr func = NULL;
  MPIDO_Embedded_Info_Set *properties = &(comm->dcmf.properties);

  int data_size, data_contig, rc = MPI_ERR_INTERN;
  char *data_buffer = NULL, *noncontig_buff = NULL;
  volatile unsigned allred_active = 1;
  DCMF_CollectiveRequest_t request;
  DCMF_Callback_t allred_cb = {allred_cb_done, (void*) &allred_active};
  int buffer_aligned = 0, skip_buff_check = 0;
  int dputok[2] = {0, 0};

  unsigned char userenvset = MPIDO_INFO_ISSET(properties, MPIDO_BCAST_ENVVAR);
  
  MPI_Aint data_true_lb = 0;
  MPID_Datatype *data_ptr;
  MPID_Segment segment;

  if (count==0)
    return MPI_SUCCESS;

  if (MPIDO_INFO_ISSET(properties, MPIDO_USE_MPICH_BCAST))
    return MPIR_Bcast(buffer, count, datatype, root, comm);

  MPIDI_Datatype_get_info(count,
                          datatype,
                          data_contig, data_size, data_ptr, data_true_lb);


  MPIDI_VerifyBuffer(buffer, data_buffer, data_true_lb);

  if (!data_contig)
  {
    noncontig_buff = MPIU_Malloc(data_size);
    data_buffer = noncontig_buff;
    if (noncontig_buff == NULL)
    {
      fprintf(stderr,
              "Pack: Tree Bcast cannot allocate local non-contig pack"
              " buffer\n");
      MPIX_Dump_stacks();
      MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                 "Fatal:  Cannot allocate pack buffer");
    }

    if (comm->rank == root)
    {
      DLOOP_Offset last = data_size;
      MPID_Segment_init(buffer, count, datatype, &segment, 0);
      MPID_Segment_pack(&segment, 0, &last, noncontig_buff);
    }
  }

  /*
    this conditions checks for one special case where R protocol is better
    than Dput protocol at large configurations in VN mode and at 65KB messages
    on rectangle subcomms.  If bcast is safe, this condition will fail and
    we would want to use Dput since it is faster. Skipping the buffer check
    will make the control go thru the normal code path and will eventually
    skip dput and go for Rectangle protocol. Note that we want this overall
    behavior to occur in the case of Rect subcomms. Otherwise, tree protocols
    should be used.
   */
  if (comm->local_size >= 32768 &&
      data_size == 65536 &&
      mpid_hw.tSize == 4 &&
      !userenvset &&
      MPIDO_INFO_ISSET(properties, MPIDO_USE_PREALLREDUCE_BCAST) &&
      MPIDO_INFO_ISSET(properties, MPIDO_RECT_COMM))
    skip_buff_check = 1;

  if (!skip_buff_check)
  {
    buffer_aligned = !((unsigned) data_buffer & 0x0F);
    /* Are we likely to use DPUT? If so, we must pre-allreduce to see if 
     * everyone has an aligned buffer. This sucks, but dput is a bandwidth
     * optimized protocol so it shouldn't be a huge deal 
     * These properties are the same on all nodes, so if they are valid
     * and we haven't turned off the pre-allreduce check, then we will need
     * to do the allreduce. Note: We add buffer alignment inside the
     * preallreduce step because everyone must agree to *do* the allreduce.
     *
     * CCMI Tree Dput is suggested as a protocol for 1 byte messages. Is
     * this really true? It appears that a "normal" Tree protocol has
     * priority at least. I'd rather not add *more* complexity to this
     * mess, so I don't see a need to check for small messages && tree
     * dput || medium to large messages && rect dput. I'm going
     * to leave the allreduce cutoff where the rect dput cutoff is, namely
     * 8192. If CCMI Tree Dput is viable at 1 byte and beats nondput tree,
     * then we'll have to reevaluate this decision and reorder the protocols
     * inside the if() checks..
     */
    dputok[0] = MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_DPUT_BCAST) &&
                (userenvset || data_size>8192);
    /* We never do CCMI_TREE_DPUT_BCAST unless a lot of other stuff is turned
     * off. That stuff can only be turned off via env vars, so it doesn't make
     * sense to do this preallreduce UNLESS env vars are set */
    dputok[1] = MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_DPUT_BCAST) &&
                userenvset;
  }
  
  if(MPIDO_INFO_ISSET(properties, MPIDO_USE_PREALLREDUCE_BCAST) && 
     (dputok[0] || dputok[1]))
  {
    dputok[0] = (dputok[0] && buffer_aligned);
    dputok[1] = (dputok[1] && buffer_aligned);
    if(comm->dcmf.short_allred == NULL)
    {
      STAR_info.internal_control_flow = 1;
      MPIDO_Allreduce(MPI_IN_PLACE, &dputok, 2, MPI_INT, MPI_BAND, comm);
      STAR_info.internal_control_flow = 0;
    }
    else
    {
      DCMF_Allreduce(comm->dcmf.short_allred,
                     &request,
                     allred_cb,
                     DCMF_MATCH_CONSISTENCY,
                     &(comm->dcmf.geometry),
                     (void *)dputok,
                     (void *)dputok,
                     2,
                     DCMF_SIGNED_INT,
                     DCMF_BAND);
      MPID_PROGRESS_WAIT_WHILE(allred_active);
    }
  }
  
  
  if (!STAR_info.enabled || STAR_info.internal_control_flow ||
      data_size < STAR_info.bcast_threshold)
  {
    if (userenvset)
    {
      if (mpid_hw.tSize > 1 && // data_size <= 8192 &&
          MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_SHMEM_BCAST))
      {
        func = MPIDO_Bcast_tree_shmem;
        comm->dcmf.last_algorithm = MPIDO_USE_TREE_SHMEM_BCAST;
      }
      
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
      {
        func = MPIDO_Bcast_tree;
        comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
      }
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
      {
        func = MPIDO_Bcast_CCMI_tree;
        comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;        
      }
      if (!func && dputok[1] && buffer_aligned) 
      {
        func = MPIDO_Bcast_CCMI_tree_dput;
        comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_DPUT_BCAST;
      }
      
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_SINGLETH_BCAST))
      {
        func = MPIDO_Bcast_rect_singleth;
        comm->dcmf.last_algorithm = MPIDO_USE_RECT_SINGLETH_BCAST;        
      }
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ARECT_BCAST))
      {
        func = MPIDO_Bcast_rect_async;
        comm->dcmf.last_algorithm = MPIDO_USE_ARECT_BCAST;        
      }
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ABINOM_BCAST))
      {
        func = MPIDO_Bcast_binom_async;
        comm->dcmf.last_algorithm = MPIDO_USE_ABINOM_BCAST;        
      }
      if (!func &&
          MPIDO_INFO_ISSET(properties, MPIDO_USE_SCATTER_GATHER_BCAST))
      {
        func = MPIDO_Bcast_scatter_gather;
        comm->dcmf.last_algorithm = MPIDO_USE_SCATTER_GATHER_BCAST;        
      }
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_BCAST))
      {
        func = MPIDO_Bcast_rect_sync;
        comm->dcmf.last_algorithm = MPIDO_USE_RECT_BCAST;        
      }
      if(!func && dputok[0] && buffer_aligned)
      {
        func = MPIDO_Bcast_rect_dput;
        comm->dcmf.last_algorithm = MPIDO_USE_RECT_DPUT_BCAST;
      }          
      if (!func &&
          MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_SINGLETH_BCAST))
      {
        func = MPIDO_Bcast_binom_singleth;
        comm->dcmf.last_algorithm = MPIDO_USE_BINOM_SINGLETH_BCAST;
      }
      if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_BCAST))
      {
        func = MPIDO_Bcast_binom_sync;      
        comm->dcmf.last_algorithm = MPIDO_USE_BINOM_BCAST;
      }
    }
    else
    {
      if (data_size <= 1024)
      {
        if (mpid_hw.tSize > 1 &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_SHMEM_BCAST))
        {
          func = MPIDO_Bcast_tree_shmem;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_SHMEM_BCAST;
        }
        
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
        }

         /* We don't actually use this protocol yet, but the performance
          * isn't horrible so we will use it some day when we get rid of
          * global tree. so leaving this one alone for now. */
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }      

        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_SINGLETH_BCAST) &&
            data_size > 128)
        {
          func = MPIDO_Bcast_rect_singleth;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_SINGLETH_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ARECT_BCAST))
        {
          func = MPIDO_Bcast_rect_async;
          comm->dcmf.last_algorithm = MPIDO_USE_ARECT_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ABINOM_BCAST))
        {
          func = MPIDO_Bcast_binom_async;
          comm->dcmf.last_algorithm = MPIDO_USE_ABINOM_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_SCATTER_GATHER_BCAST))
        {
          func = MPIDO_Bcast_scatter_gather;
          comm->dcmf.last_algorithm = MPIDO_USE_SCATTER_GATHER_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {         
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }
        /* This will never ever be used unless done via env vars. There is
         * no reason to ruin other protocols performance for that. */
        /** \todo Decide if there is ever a case to actually use this 

        if (!func && buffer_aligned && dputok[1])
        {
          func = MPIDO_Bcast_CCMI_tree_dput;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_DPUT_BCAST;
        }
        */
        
      }
      else if (data_size <= 8192)
      {
        if (mpid_hw.tSize > 1 &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_SHMEM_BCAST))
        {
          func = MPIDO_Bcast_tree_shmem;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_SHMEM_BCAST;
        }

        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {          
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_SINGLETH_BCAST))
        {
          func = MPIDO_Bcast_rect_singleth;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_SINGLETH_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_BCAST))
        {
          func = MPIDO_Bcast_rect_sync;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ABINOM_BCAST))
        {
          func = MPIDO_Bcast_binom_async;
          comm->dcmf.last_algorithm = MPIDO_USE_ABINOM_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_SCATTER_GATHER_BCAST))
        {
          func = MPIDO_Bcast_scatter_gather;
          comm->dcmf.last_algorithm = MPIDO_USE_SCATTER_GATHER_BCAST;
        }
        
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }
        
        /* This will never ever be used unless done via env vars. There is
         * no reason to ruin other protocols performance for that. */
        /** \todo Decide if there is ever a case to actually use this 
        if (!func && buffer_aligned && dputok[1])
        {
          func = MPIDO_Bcast_CCMI_tree_dput;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_DPUT_BCAST;
        }
        */
      }
      else if (data_size <= 65536)
      {
        if (mpid_hw.tSize > 2 && data_size <= 8192 &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_SHMEM_BCAST))
        {
          func = MPIDO_Bcast_tree_shmem;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_SHMEM_BCAST;
        }
        
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;        
        }

        /* This will never ever be used unless done via env vars. There is
         * no reason to ruin other protocols performance for that. */
        /** \todo Decide if there is ever a case to actually use this 
        if (!func && buffer_aligned && dputok[1])
        {
          func = MPIDO_Bcast_CCMI_tree_dput;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_DPUT_BCAST;
        }      
        */

        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {          
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }
         /* We now have global knowledge for dput. However, we don't globally 
          * have buffer alignement as part of the dputok variable because we 
          * might skip over the allreduce step which adds buffer_alignment 
          * checks to dputok. Therefore, dput checks need (dputok && 
          * buffer_alignment) in the general case
          */
        if(!func && dputok[0] && buffer_aligned)
        {
          func = MPIDO_Bcast_rect_dput;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_DPUT_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_BCAST))
        {
          func = MPIDO_Bcast_rect_sync;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_SINGLETH_BCAST))
        {
          func = MPIDO_Bcast_binom_singleth;
          comm->dcmf.last_algorithm = MPIDO_USE_BINOM_SINGLETH_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_ABINOM_BCAST) &&
            data_size < 16384)
        {
          func = MPIDO_Bcast_binom_async;
          comm->dcmf.last_algorithm = MPIDO_USE_ABINOM_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_BCAST))
        {
          func = MPIDO_Bcast_binom_sync;      
          comm->dcmf.last_algorithm = MPIDO_USE_BINOM_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_SCATTER_GATHER_BCAST))
        {
          func = MPIDO_Bcast_scatter_gather;
          comm->dcmf.last_algorithm = MPIDO_USE_SCATTER_GATHER_BCAST;          
        }
      }
      else
      {
         if(dputok[0] && buffer_aligned)
         {
            func = MPIDO_Bcast_rect_dput;
            comm->dcmf.last_algorithm = MPIDO_USE_RECT_DPUT_BCAST;
         }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_RECT_BCAST))
        {
          func = MPIDO_Bcast_rect_sync;
          comm->dcmf.last_algorithm = MPIDO_USE_RECT_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_SINGLETH_BCAST))
        {
          func = MPIDO_Bcast_binom_singleth;
          comm->dcmf.last_algorithm = MPIDO_USE_BINOM_SINGLETH_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_BINOM_BCAST))
        {
          func = MPIDO_Bcast_binom_sync;
          comm->dcmf.last_algorithm = MPIDO_USE_BINOM_BCAST;
        }
        if (!func &&
            MPIDO_INFO_ISSET(properties, MPIDO_USE_SCATTER_GATHER_BCAST))
        {
          func = MPIDO_Bcast_scatter_gather;
          comm->dcmf.last_algorithm = MPIDO_USE_SCATTER_GATHER_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_TREE_BCAST))
        {
          func = MPIDO_Bcast_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_TREE_BCAST;
        }
        if (!func && MPIDO_INFO_ISSET(properties, MPIDO_USE_CCMI_TREE_BCAST))
        {
          func = MPIDO_Bcast_CCMI_tree;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_BCAST;
        }
        /* This will never ever be used unless done via env vars. There is
         * no reason to ruin other protocols performance for that. */
        /** \todo Decide if there is ever a case to actually use this 
        if (!func && buffer_aligned && dputok[1])
        {
          func = MPIDO_Bcast_CCMI_tree_dput;
          comm->dcmf.last_algorithm = MPIDO_USE_CCMI_TREE_DPUT_BCAST;
        }        
        */
      }
    }
    
    if (!func)
    {
      comm->dcmf.last_algorithm = MPIDO_USE_MPICH_BCAST;
      return MPIR_Bcast(buffer, count, datatype, root, comm);
    }

    rc = (func) (data_buffer, data_size, root, comm);
  }
      
  else
  {
    int id;
    unsigned char same_callsite = 1;

    STAR_Callsite collective_site;

    void ** tb_ptr = (void **) MPIU_Malloc(sizeof(void *) *
                                           STAR_info.traceback_levels);


    /* set the internal control flow to disable internal star tuning */
    STAR_info.internal_control_flow = 1;

    /* get backtrace info for caller to this func, use that as callsite_id */
    backtrace(tb_ptr, STAR_info.traceback_levels);
    id = (int) tb_ptr[STAR_info.traceback_levels - 1];


    /* find out if all participants agree on the callsite id */
    if (STAR_info.agree_on_callsite)
    {
      int tmp[2], result[2];
      tmp[0] = id;
      tmp[1] = ~id;
      MPIDO_Allreduce(tmp, result, 2, MPI_UNSIGNED_LONG, MPI_MAX, comm);
      if (result[0] != (~result[1]))
        same_callsite = 0;
    }

    if (same_callsite)
    {
      /* create a signature callsite info for this particular call site */
      collective_site.call_type = BCAST_CALL;
      collective_site.comm = comm;
      collective_site.bytes = data_size;
      collective_site.op_type_support = MPIDO_SUPPORT_NOT_NEEDED;
      collective_site.id = id;

      /* decide buffer alignment */
      collective_site.buff_attributes[3] = 1; /* assume aligned */
      if (!buffer_aligned)
        collective_site.buff_attributes[3] = 0; /* set to not aligned */

      rc = STAR_Bcast(data_buffer, root, &collective_site,
                      STAR_bcast_repository,
                      STAR_info.bcast_algorithms);
    }

    if (rc == STAR_FAILURE || !same_callsite)
    {
      rc = MPIR_Bcast(buffer, count, datatype, root, comm);
    }
    
    /* unset the internal control flow */
    STAR_info.internal_control_flow = 0;

    MPIU_Free(tb_ptr);    
  }
  if (!data_contig)
  {
    if (comm->rank != root)
    {
      int smpi_errno, rmpi_errno;
      MPIDI_msg_sz_t rcount;
      MPIDI_DCMF_Buffer_copy(noncontig_buff, data_size, MPI_CHAR,
                             &smpi_errno, buffer, count, datatype,
                             &rcount, &rmpi_errno);
    }
    MPIU_Free(noncontig_buff);
  }

  return rc;
}

#else /* !USE_CCMI_COLL */

int
MPIDO_Bcast(void *buffer,
            int count, MPI_Datatype datatype, int root, MPID_Comm * comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
