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
 * \file src/comm/mpid_comm.c
 * \brief ???
 */

/*#define TRACE_ON */

#include <mpidimpl.h>
extern void MPIDI_Comm_coll_query(MPID_Comm *);
extern void MPIDI_Comm_coll_envvars(MPID_Comm *);

void geom_create_cb_done(void *ctxt, void *data, pami_result_t err)
{
   int *active = (int *)data;
   (*active)--;
}

void geom_destroy_cb_done(void *ctxt, void *data, pami_result_t err)
{
   int *active = (int *)data;
   (*active)--;
}

int MPIDI_Comm_create (MPID_Comm *comm)
{
  MPIDI_Coll_comm_create(comm);
  return MPI_SUCCESS;
}

int MPIDI_Comm_destroy (MPID_Comm *comm)
{
  MPIDI_Coll_comm_destroy(comm);
  return MPI_SUCCESS;
}

typedef struct MPIDI_Post_geom_create
{
   pami_work_t state;
   size_t context_offset;
   pami_client_t client;
   pami_configuration_t *configs;
   size_t num_configs;
   pami_geometry_t newgeom;
   pami_geometry_t parent;
   unsigned id;
   pami_geometry_range_t *ranges;
   pami_task_t *tasks;
   size_t count; /* count of ranges or tasks */
   pami_event_function fn;
   void* cookie;
} MPIDI_Post_geom_create_t;

typedef struct MPIDI_Post_geom_destroy
{
   pami_work_t state;
   pami_client_t client;
   pami_geometry_t *geom;
   pami_event_function fn;
   void *cookie;
} MPIDI_Post_geom_destroy_t;

static pami_result_t geom_rangelist_create_wrapper(pami_context_t context, void *cookie)
{
   /* I'll need one of these per geometry creation function..... */
   MPIDI_Post_geom_create_t *geom_struct = (MPIDI_Post_geom_create_t *)cookie;
   TRACE_ERR("In geom create wrapper\n");
   return PAMI_Geometry_create_taskrange(
               geom_struct->client,
               geom_struct->context_offset,
               geom_struct->configs,
               geom_struct->num_configs,
               geom_struct->newgeom,
               geom_struct->parent,
               geom_struct->id,
               geom_struct->ranges,
               geom_struct->count,
               context,
               geom_struct->fn,
               geom_struct->cookie);
   TRACE_ERR("Done in geom create wrapper\n");
}
static pami_result_t geom_tasklist_create_wrapper(pami_context_t context, void *cookie)
{
   /* I'll need one of these per geometry creation function..... */
   MPIDI_Post_geom_create_t *geom_struct = (MPIDI_Post_geom_create_t *)cookie;
   TRACE_ERR("In geom create wrapper\n");
   return PAMI_Geometry_create_tasklist(
               geom_struct->client,
               geom_struct->context_offset,
               geom_struct->configs,
               geom_struct->num_configs,
               geom_struct->newgeom,
               geom_struct->parent,
               geom_struct->id,
               geom_struct->tasks,
               geom_struct->count,
               context,
               geom_struct->fn,
               geom_struct->cookie);
   TRACE_ERR("Done in geom create wrapper\n");
}

static pami_result_t geom_destroy_wrapper(pami_context_t context, void *cookie)
{
   MPIDI_Post_geom_destroy_t *geom_destroy_struct = (MPIDI_Post_geom_destroy_t *)cookie;
   TRACE_ERR("In geom destroy wrapper\n");
   return PAMI_Geometry_destroy(
               geom_destroy_struct->client,
               geom_destroy_struct->geom,
               context,
               geom_destroy_struct->fn,
               geom_destroy_struct->cookie);
   TRACE_ERR("Done in geom destroy wrapper\n");
}



void MPIDI_Coll_comm_create(MPID_Comm *comm)
{
   volatile int geom_init = 1;
   int i;
   MPIDI_Post_geom_create_t geom_post;

  TRACE_ERR("MPIDI_Coll_comm_create enter\n");
  if (!MPIDI_Process.optimized.collectives)
    return;

  comm->coll_fns = MPIU_Calloc0(1, MPID_Collops);
  MPID_assert(comm->coll_fns != NULL);

  if(comm->comm_kind != MPID_INTRACOMM) return;
  /* Create a geometry */
   
   if(comm->mpid.geometry != MPIDI_Process.world_geometry)
   {
      if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0))
         fprintf(stderr,"world geom: %p parent geom: %p\n", MPIDI_Process.world_geometry, comm->mpid.parent);
      TRACE_ERR("Creating subgeom\n");
      /* Change to this at some point */

      comm->mpid.tasks = NULL; 
      for(i=1;i<comm->local_size;i++)
      {
         /* only if sequential tasks should we use a (single) range.
            Multi or reordered ranges are inefficient */
         if(MPID_VCR_GET_LPID(comm->vcr, i) != (MPID_VCR_GET_LPID(comm->vcr, i-1) + 1)) {
            /* not sequential, use tasklist */
            comm->mpid.tasks = comm->vcr; 
            break;
         }
      }
      /* Should we use a range? (no task list set) */
      if(comm->mpid.tasks == NULL)
      {
         /* one range, {first rank ... last rank} */
         comm->mpid.range.lo = MPID_VCR_GET_LPID(comm->vcr, 0);
         comm->mpid.range.hi = MPID_VCR_GET_LPID(comm->vcr, comm->local_size-1);
      }

      if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0))
         fprintf(stderr,"create geometry tasks %p {%u..%u}\n", comm->mpid.tasks, MPID_VCR_GET_LPID(comm->vcr, 0),MPID_VCR_GET_LPID(comm->vcr, comm->local_size-1));

      pami_configuration_t config[3];
      config[0].name = PAMI_GEOMETRY_NONCONTIG;
      if(MPIDI_Process.optimized.memory & MPID_OPT_LVL_NONCONTIG) 
         config[0].value.intval = 0; // Disable non-contig, pamid doesn't use pami for non-contig data collectives
      else
         config[0].value.intval = 1; // Enable non-contig even though pamid doesn't use pami for non-contig data collectives, 
                                     // we still possibly want those collectives for other reasons.
      size_t numconfigs = 1;
      if(MPIDI_Process.optimized.subcomms)
      {
         config[numconfigs].name = PAMI_GEOMETRY_OPTIMIZE;
         config[numconfigs].value.intval = 1; 
         ++numconfigs;
      }
      if(MPIDI_Process.optimized.memory) 
      {
         config[numconfigs].name = PAMI_GEOMETRY_MEMORY_OPTIMIZE;
         config[numconfigs].value.intval = MPIDI_Process.optimized.memory; /* level of optimization */
         ++numconfigs;
      }

      if((MPIDI_Process.optimized.memory  & MPID_OPT_LVL_IRREG) && (comm->local_size & (comm->local_size-1)))
      {
         /* Don't create irregular geometries.  Fallback to MPICH only collectives */
         geom_init = 0;
         comm->mpid.geometry = PAMI_GEOMETRY_NULL;
      }
      else if(comm->mpid.tasks == NULL)
      {   
         geom_post.client = MPIDI_Client;
         geom_post.configs = config;
         geom_post.context_offset = 0; /* TODO BES investigate */
         geom_post.num_configs = numconfigs;
         geom_post.newgeom = &comm->mpid.geometry,
         geom_post.parent = PAMI_GEOMETRY_NULL;
         geom_post.id     = comm->context_id;
         geom_post.ranges = &comm->mpid.range;
         geom_post.tasks = NULL;;
         geom_post.count = (size_t)1;
         geom_post.fn = geom_create_cb_done;
         geom_post.cookie = (void*)&geom_init;

         TRACE_ERR("%s geom_rangelist_create\n", MPIDI_Process.context_post>0?"Posting":"Invoking");
         MPIDI_Context_post(MPIDI_Context[0], &geom_post.state,
                            geom_rangelist_create_wrapper, (void *)&geom_post);
      }
      else
      {
         geom_post.client = MPIDI_Client;
         geom_post.configs = config;
         geom_post.context_offset = 0; /* TODO BES investigate */
         geom_post.num_configs = numconfigs;
         geom_post.newgeom = &comm->mpid.geometry,
         geom_post.parent = PAMI_GEOMETRY_NULL;
         geom_post.id     = comm->context_id;
         geom_post.ranges = NULL;
         geom_post.tasks = comm->mpid.tasks;
         geom_post.count = (size_t)comm->local_size;
         geom_post.fn = geom_create_cb_done;
         geom_post.cookie = (void*)&geom_init;

         TRACE_ERR("%s geom_tasklist_create\n", MPIDI_Process.context_post>0?"Posting":"Invoking");
         MPIDI_Context_post(MPIDI_Context[0], &geom_post.state,
                            geom_tasklist_create_wrapper, (void *)&geom_post);
      }

      TRACE_ERR("Waiting for geom create to finish\n");
      MPID_PROGRESS_WAIT_WHILE(geom_init);

      if(comm->mpid.geometry == PAMI_GEOMETRY_NULL)
      {
         if(unlikely(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0))
            fprintf(stderr,"Created unoptimized communicator id=%u, size=%u\n", (unsigned) comm->context_id,comm->local_size);
         MPIU_TestFree(&comm->coll_fns);
         return;
      }
   }
   /* Initialize the async flow control in case it will be used. */
   comm->mpid.num_requests = MPIDI_Process.optimized.num_requests;

   TRACE_ERR("Querying protocols\n");
   /* Determine what protocols are available for this comm/geom */
   /* These two functions moved to mpid_collselect.c */
   MPIDI_Comm_coll_query(comm); /* allocates algorithm lists */
   MPIDI_Comm_coll_envvars(comm);
   if(MPIDI_Process.optimized.select_colls)
      MPIDI_Comm_coll_select(comm);

#ifdef __BGQ__
   /* BGQ is done with algorithm lists */
   for(i=0;i<PAMI_XFER_COUNT;i++)
   {
     TRACE_ERR("Freeing algo/meta %d\n", i);
     /* When allocating comm->mpid.algorithm_list, we skip allocations for
       AM collectives. Also there is no explicit initialization of 
       comm->mpid.algorithm_list to NULLs. This may cause MPIU_TestFree to
       cause problems when freeing. We skip AM collectives here as we skip
       allocating them in MPIDI_Comm_coll_query */
     if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
        i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE)
         continue;

     MPIU_TestFree(&comm->mpid.algorithm_list[i][0]);
     MPIU_TestFree(&comm->mpid.algorithm_list[i][1]);
     MPIU_TestFree(&comm->mpid.algorithm_metadata_list[i][0]);
     MPIU_TestFree(&comm->mpid.algorithm_metadata_list[i][1]);
   }
#endif
   TRACE_ERR("mpir barrier\n");
   int mpierrno;
   /* Switch to comm->coll_fns->fn() */
   MPIDO_Barrier(comm, &mpierrno);


  TRACE_ERR("MPIDI_Coll_comm_create exit\n");
}

void MPIDI_Coll_comm_destroy(MPID_Comm *comm)
{
  TRACE_ERR("MPIDI_Coll_comm_destroy enter\n");
  int i;
  volatile int geom_destroy = 1;
  MPIDI_Post_geom_destroy_t geom_destroy_post;
  if (!MPIDI_Process.optimized.collectives)
    return;

  if(comm->comm_kind != MPID_INTRACOMM) 
    return;

  /* It's possible (MPIR_Setup_intercomm_localcomm) to have an intracomm
     without a geometry even when using optimized collectives */
  if(comm->mpid.geometry == PAMI_GEOMETRY_NULL)
    return;

   MPIU_TestFree(&comm->coll_fns);

#ifndef __BGQ__
   /* non-BGQ (collective selection) is done with algorithm lists */
   for(i=0;i<PAMI_XFER_COUNT;i++)
   {
     TRACE_ERR("Freeing algo/meta %d\n", i);
     /* When allocating comm->mpid.algorithm_list, we skip allocations for
       AM collectives. Also there is no explicit initialization of 
       comm->mpid.algorithm_list to NULLs. This may cause MPIU_TestFree to
       cause problems when freeing. We skip AM collectives here as we skip
       allocating them in MPIDI_Comm_coll_query */
     if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE)
         continue;

     MPIU_TestFree(&comm->mpid.algorithm_list[i][0]);
     MPIU_TestFree(&comm->mpid.algorithm_list[i][1]);
     MPIU_TestFree(&comm->mpid.algorithm_metadata_list[i][0]);
     MPIU_TestFree(&comm->mpid.algorithm_metadata_list[i][1]);
   }
#endif

   TRACE_ERR("Destroying geometry\n");

   geom_destroy_post.client = MPIDI_Client;
   geom_destroy_post.geom = &comm->mpid.geometry;
   geom_destroy_post.fn = geom_destroy_cb_done;
   geom_destroy_post.cookie = (void *)&geom_destroy;
   TRACE_ERR("%s geom_destroy\n", MPIDI_Process.context_post>0?"Posting":"Invoking");
   MPIDI_Context_post(MPIDI_Context[0], &geom_destroy_post.state,
                      geom_destroy_wrapper, (void *)&geom_destroy_post);

   TRACE_ERR("Waiting for geom destroy to finish\n");
   MPID_PROGRESS_WAIT_WHILE(geom_destroy);
/*   TRACE_ERR("Freeing geometry ranges\n");
   MPIU_TestFree(&comm->mpid.tasks_descriptor.ranges);
*/ 
   TRACE_ERR("MPIDI_Coll_comm_destroy exit\n");
}
