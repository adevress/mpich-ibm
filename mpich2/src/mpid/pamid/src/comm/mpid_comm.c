/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/comm/mpid_comm.c
 * \brief ???
 */

//#define TRACE_ON

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

void MPIDI_Comm_create (MPID_Comm *comm)
{
  MPIDI_Coll_comm_create(comm);
}

void MPIDI_Comm_destroy (MPID_Comm *comm)
{
  MPIDI_Coll_comm_destroy(comm);
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
   pami_geometry_range_t *slices;
   size_t slice_count;
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
               geom_struct->slices,
               geom_struct->slice_count,
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
   int rc;
   volatile int geom_init = 1;
   int i;
   pami_geometry_range_t *slices = NULL;
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
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL)
         fprintf(stderr,"world geom: %p parent geom: %p\n", MPIDI_Process.world_geometry, comm->mpid.parent);
      TRACE_ERR("Creating subgeom\n");
      /* Change to this at some point */
      #if 0
      rc = PAMI_Geometry_create_tasklist( MPIDI_Client,
                                          NULL,
                                          0,
                                          &comm->mpid.geometry,
                                          NULL, /* Parent */
                                          comm->context_id,
                                          /* task list, where/how do I get that? */
                                          comm->local_size,
                                          MPIDI_Context[0],
                                          geom_create_cb_done,
                                          &geom_init);
      #endif

      slices = MPIU_Malloc(sizeof(pami_geometry_range_t) * comm->local_size);
      for(i=0;i<comm->local_size;i++)
      {
         slices[i].lo = MPID_VCR_GET_LPID(comm->vcr, i);
         slices[i].hi = MPID_VCR_GET_LPID(comm->vcr, i);
      }
      pami_configuration_t config;
      size_t numconfigs = 0;

      if(MPIDI_Process.optimized.subcomms)
      {
         config.name = PAMI_GEOMETRY_OPTIMIZE;
         numconfigs = 1;
      }
      else
      {
         numconfigs = 0;
      }

      if(MPIDI_Process.context_post)
      {
         geom_post.client = MPIDI_Client;
         geom_post.configs = &config;
         geom_post.context_offset = 0; /* TODO BES investigate */
         geom_post.num_configs = numconfigs;
         geom_post.newgeom = &comm->mpid.geometry,
         geom_post.parent = NULL;
         geom_post.id     = comm->context_id;
         geom_post.slices = slices;
         geom_post.slice_count = (size_t)comm->local_size,
         geom_post.fn = geom_create_cb_done;
         geom_post.cookie = (void*)&geom_init;

         TRACE_ERR("Posting geom_create\n");
         rc = PAMI_Context_post(MPIDI_Context[0], &geom_post.state, 
                  geom_rangelist_create_wrapper, (void *)&geom_post);
      }
      else
      {
         rc = PAMI_Geometry_create_taskrange(MPIDI_Client,
                                          0, /* TODO BES need to investigate */
                                         &config,
                                         numconfigs,
                                         &comm->mpid.geometry,
                                         NULL, /*MPIDI_Process.world_geometry,*/
                                         comm->context_id,
                                         slices,
                                         (size_t)comm->local_size,
                                         MPIDI_Context[0],
                                         geom_create_cb_done,
                                         (void*)&geom_init);

      }

      if(rc != PAMI_SUCCESS)
      {
         fprintf(stderr,"Error creating subcomm geometry. %d\n", rc);
         exit(1);
      }

      TRACE_ERR("Waiting for geom create to finish\n");
      MPID_PROGRESS_WAIT_WHILE(geom_init);
   }

   TRACE_ERR("Querying protocols\n");
   /* Determine what protocols are available for this comm/geom */
   /* These two functions moved to mpid_collselect.c */
   MPIDI_Comm_coll_query(comm);
   MPIDI_Comm_coll_envvars(comm);
   TRACE_ERR("mpir barrier\n");
   int mpierrno;
   MPIR_Barrier(comm, &mpierrno);
   MPIU_Free(slices);


  TRACE_ERR("MPIDI_Coll_comm_create exit\n");
}

void MPIDI_Coll_comm_destroy(MPID_Comm *comm)
{
  TRACE_ERR("MPIDI_Coll_comm_destroy enter\n");
  int i, rc;
  volatile int geom_destroy = 1;
  MPIDI_Post_geom_destroy_t geom_destroy_post;
  if (!MPIDI_Process.optimized.collectives)
    return;

  if(comm->comm_kind != MPID_INTRACOMM) 
    return;

  /* It's possible (MPIR_Setup_intercomm_localcomm) to have an intracomm
     without a geometry even when using optimized collectives */
  if(comm->mpid.geometry == NULL)
    return; 

   MPIU_TestFree(&comm->coll_fns);
   for(i=0;i<PAMI_XFER_COUNT;i++)
   {
     TRACE_ERR("Freeing algo/meta %d\n", i);
     MPIU_TestFree(&comm->mpid.coll_algorithm[i][0]);
     MPIU_TestFree(&comm->mpid.coll_algorithm[i][1]);
     MPIU_TestFree(&comm->mpid.coll_metadata[i][0]);
     MPIU_TestFree(&comm->mpid.coll_metadata[i][1]);
   }

   TRACE_ERR("Destroying geometry\n");

   if(MPIDI_Process.context_post)
   {
      geom_destroy_post.client = MPIDI_Client;
      geom_destroy_post.geom = &comm->mpid.geometry;
      geom_destroy_post.fn = geom_destroy_cb_done;
      geom_destroy_post.cookie = (void *)&geom_destroy;
      TRACE_ERR("Posting geom_destroy\n");
      rc = PAMI_Context_post(MPIDI_Context[0], &geom_destroy_post.state, 
                  geom_destroy_wrapper, (void *)&geom_destroy_post);
   }
   else
   {
      rc = PAMI_Geometry_destroy(MPIDI_Client, 
                        &comm->mpid.geometry, 
                        MPIDI_Context[0],
                        geom_destroy_cb_done,
                        (void *)&geom_destroy);
   }

   if(rc != PAMI_SUCCESS)
   {
      fprintf(stderr,"Error destroying geometry %d\n", rc);
      exit(1);
   }
   TRACE_ERR("Waiting for geom destroy to finish\n");
   MPID_PROGRESS_WAIT_WHILE(geom_destroy);
   TRACE_ERR("MPIDI_Coll_comm_destroy exit\n");
}



void MPIDI_Comm_world_setup()
{
  TRACE_ERR("MPIDI_Comm_world_setup enter\n");

  /* Anything special required for COMM_WORLD goes here */
   MPID_Comm *comm;
   comm = MPIR_Process.comm_world;

  TRACE_ERR("MPIDI_Comm_world_setup exit\n");
}
