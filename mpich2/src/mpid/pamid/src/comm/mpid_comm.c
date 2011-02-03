/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/comm/mpid_comm.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void MPIDI_Update_coll(pami_algorithm_t coll, 
                              int type,
                              int index,
                              MPID_Comm *comm);

void geom_cb_done(void *ctxt, void *data, pami_result_t err)
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

typedef struct MPIDI_Post_geom
{
   pami_work_t state;
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
} MPIDI_Post_geom_t;

static pami_result_t geom_rangelist_create_wrapper(pami_context_t context, void *cookie)
{
   /* I'll need one of these per geometry creation function..... */
   MPIDI_Post_geom_t *geom_struct = (MPIDI_Post_geom_t *)cookie;
   TRACE_ERR("In geom create wrapper\n");
   return PAMI_Geometry_create_taskrange(
               geom_struct->client,
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

   
void MPIDI_Coll_comm_create(MPID_Comm *comm)
{
   int rc;
   int geom_init = 1;
   int i;
   pami_geometry_range_t *slices;
   MPIDI_Post_geom_t geom_post;

  TRACE_ERR("MPIDI_Coll_comm_create enter\n");
  if (!MPIDI_Process.optimized.collectives)
    return;

  comm->coll_fns = MPIU_Calloc0(1, MPID_Collops);
  MPID_assert(comm->coll_fns != NULL);

  if(comm->comm_kind != MPID_INTRACOMM) return;
  /* Create a geometry */
   
   if(comm->mpid.geometry != MPIDI_Process.world_geometry)
   {
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
                                          geom_cb_done,
                                          &geom_init);
      #endif

      slices = malloc(sizeof(pami_geometry_range_t) * comm->local_size);
      for(i=0;i<comm->local_size;i++)
      {
         slices[i].lo = MPID_VCR_GET_LPID(comm->vcr, i);
         slices[i].hi = MPID_VCR_GET_LPID(comm->vcr, i);
      }
      pami_configuration_t config;
      size_t numconfigs = 0;

      if(MPIDI_Process.optimized_subcomms)
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
         geom_post.num_configs = numconfigs;
         geom_post.newgeom = &comm->mpid.geometry,
         geom_post.parent = NULL;
         geom_post.slices = slices;
         geom_post.slice_count = (size_t)comm->local_size,
         geom_post.fn = geom_cb_done;
         geom_post.cookie = &geom_init;

         TRACE_ERR("Posting geom_create\n");
         rc = PAMI_Context_post(MPIDI_Context[0], &geom_post.state, 
                  geom_rangelist_create_wrapper, (void *)&geom_post);
      }
      else
      {
         rc = PAMI_Geometry_create_taskrange(MPIDI_Client,
                                         &config,
                                         numconfigs,
                                         &comm->mpid.geometry,
                                         NULL, /*MPIDI_Process.world_geometry,*/
                                         comm->context_id,
                                         slices,
                                         (size_t)comm->local_size,
                                         MPIDI_Context[0],
                                         geom_cb_done,
                                         &geom_init);

      }

      if(rc != PAMI_SUCCESS)
      {
         fprintf(stderr,"Error creating subcomm geometry. %d\n", rc);
         exit(1);
      }

      TRACE_ERR("Waiting for geom create to finish\n");
      while(geom_init)
         PAMI_Context_advance(MPIDI_Context[0], 1);
   }

   TRACE_ERR("Querying protocols\n");
   /* Determine what protocols are available for this comm/geom */
  MPIDI_Comm_coll_query(comm);
  MPIDI_Comm_coll_envvars(comm);
   TRACE_ERR("mpir barrier\n");
   MPIR_Barrier(comm);


  TRACE_ERR("MPIDI_Coll_comm_create exit\n");
}

void MPIDI_Coll_comm_destroy(MPID_Comm *comm)
{
  TRACE_ERR("MPIDI_Coll_comm_destroy enter\n");
  if (!MPIDI_Process.optimized.collectives)
    return;

  MPIU_TestFree(&comm->coll_fns);

  TRACE_ERR("MPIDI_Coll_comm_destroy exit\n");
}


static void MPIDI_Update_coll(pami_algorithm_t coll, 
                       int type, 
                       int index,
                       MPID_Comm *comm_ptr)
{
   comm_ptr->mpid.user_selectedvar[coll] = 1;
   comm_ptr->mpid.user_selected[coll] = 
      comm_ptr->mpid.coll_algorithm[coll][type][index];
   memcpy(&comm_ptr->mpid.user_metadata[coll],
          &comm_ptr->mpid.coll_metadata[coll][type][index],
          sizeof(pami_metadata_t));
}
   
static void MPIDI_Check_preallreduce(char *env, MPID_Comm *comm, char *name, int constant)
{
   char *envopts = getenv(env);
   if(envopts != NULL)
   {
      if(strncasecmp(env, "N", 1) == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Bypassing breallreduce for %s\n", name);
         comm->mpid.preallreduces[constant] = 0;
      }
   }
}
static int MPIDI_Check_protocols(char *env, MPID_Comm *comm, char *name, int constant)
{
   int i;
   char *envopts = getenv(env);
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known %s protocols\n", envopts, name);
      /* We could maybe turn off the MPIDO_{} fn ptr instead? */
      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for %s\n", name);
         return 0;
      }
      if(strncasecmp(envopts, "GLUE_", 5) == 0)
         return -1;

      for(i=0; i < comm->mpid.coll_count[constant][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[constant][0][i].name) == 0)
         {
            MPIDI_Update_coll(constant, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default %s for comm %p\n", envopts, name, comm);
            return 0;
         }
      }
      for(i=0; i < comm->mpid.coll_count[constant][1];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[constant][1][i].name) == 0)
         {
            MPIDI_Update_coll(constant, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default %s for comm %p\n", envopts, name, comm);
            return 0;
         }
      }
   }
   return 0;
}

void MPIDI_Comm_coll_envvars(MPID_Comm *comm)
{
   char *envopts;
   int i;
   int rc;
   TRACE_ERR("MPIDI_Comm_coll_envvars enter\n");

   /* Set up always-works defaults */
   for(i = 0; i < PAMI_XFER_COUNT; i++)
   {
      if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE ||
         i == PAMI_XFER_FENCE)
         continue;

      comm->mpid.user_selectedvar[i] = 0;
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Setting up collective %d on comm %p\n", i, comm);
      comm->mpid.user_selected[i] = comm->mpid.coll_algorithm[i][0][0];
      memcpy(&comm->mpid.user_metadata[i], &comm->mpid.coll_metadata[i][0][0],
            sizeof(pami_metadata_t));
   }


   TRACE_ERR("Checking env vars\n");

   MPIDI_Check_preallreduce("PAMI_ALLGATHER_PREALLREDUCE", comm, "allgather", 
         MPID_ALLGATHER_PREALLREDUCE);

   MPIDI_Check_preallreduce("PAMI_ALLGATHERV_PREALLREDUCE", comm, "allgatherv",
         MPID_ALLGATHERV_PREALLREDUCE);

   MPIDI_Check_preallreduce("PAMI_ALLREDUCE_PREALLREDUCE", comm, "allreduce",
         MPID_ALLREDUCE_PREALLREDUCE);

   MPIDI_Check_preallreduce("PAMI_BCAST_PREALLREDUCE", comm, "broadcast",
         MPID_BCAST_PREALLREDUCE);

   MPIDI_Check_preallreduce("PAMI_SCATTERV_PREALLREDUCE", comm, "scatterv",
         MPID_SCATTERV_PREALLREDUCE);

   MPIDI_Check_protocols("PAMI_BCAST", comm, "broadcast", PAMI_XFER_BROADCAST);

   MPIDI_Check_protocols("PAMI_ALLREDUCE", comm, "allreduce", PAMI_XFER_ALLREDUCE);

   MPIDI_Check_protocols("PAMI_BARRIER", comm, "barrier", PAMI_XFER_BARRIER);

   MPIDI_Check_protocols("PAMI_ALLTOALL", comm, "alltoall", PAMI_XFER_ALLTOALL);

   MPIDI_Check_protocols("PAMI_ALLTOALLV", comm, "alltoallv", PAMI_XFER_ALLTOALLV_INT);

   MPIDI_Check_protocols("PAMI_GATHERV", comm, "gatherv", PAMI_XFER_GATHERV_INT);

   MPIDI_Check_protocols("PAMI_REDUCE", comm, "reduce", PAMI_XFER_REDUCE);

   comm->mpid.scattervs[0] = comm->mpid.scattervs[1] = 0;
   rc = MPIDI_Check_protocols("PAMI_SCATTERV", comm, "scatterv", PAMI_XFER_SCATTERV_INT);
   if(rc != 0)
   {
      envopts = getenv("PAMI_SCATTERV");
      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue bcast for scatterv\n");
         comm->mpid.scattervs[0] = 1;
      }
      else if(strcasecmp(envopts, "GLUE_ALLTOALLV") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue alltoallv for scatterv\n");
         comm->mpid.scattervs[1] = 1;
      }
   }
      
   comm->mpid.optscatter = 0;
   rc = MPIDI_Check_protocols("PAMI_SCATTERV", comm, "scatter", PAMI_XFER_SCATTER);
   if(rc != 0)
   {
      envopts = getenv("PAMI_SCATTER");
      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for scatter\n");
         comm->mpid.optscatter = 1;
      }
   }

   comm->mpid.allgathers[0] = comm->mpid.allgathers[1] = comm->mpid.allgathers[2] = 0;
   rc = MPIDI_Check_protocols("PAMI_ALLGATHER", comm, "allgather", PAMI_XFER_ALLGATHER);
   if(rc != 0)
   {
      envopts = getenv("PAMI_ALLGATHER");
      if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgather\n");
         comm->mpid.allgathers[0] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgather\n");
         comm->mpid.allgathers[1] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_alltoall for allgather\n");
         comm->mpid.allgathers[2] = 1;
      }
   }

   comm->mpid.allgathervs[0] = comm->mpid.allgathervs[1] = comm->mpid.allgathervs[2] = 0;
   rc = MPIDI_Check_protocols("PAMI_ALLGATHERV", comm, "allgatherv", PAMI_XFER_ALLGATHERV_INT);
   if(rc != 0)
   {
      envopts = getenv("PAMI_ALLGATHERV");
      if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgatherv\n");
         comm->mpid.allgathervs[0] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgatherv\n");
         comm->mpid.allgathervs[1] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_alltoall for allgatherv\n");
         comm->mpid.allgathervs[2] = 1;
      }
   }

   comm->mpid.optgather = 0;
   rc = MPIDI_Check_protocols("PAMI_GATHER", comm, "gather", PAMI_XFER_GATHER);
   if(rc != 0)
   {
      envopts = getenv("PAMI_GATHER");
      if(strcasecmp(envopts, "GLUE_REDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"using glue_reduce for gather\n");
         comm->mpid.optgather = 1;
      }
   }

   TRACE_ERR("MPIDI_Comm_coll_envvars exit\n");
}


/* Determine how many of each collective type this communicator supports */
void MPIDI_Comm_coll_query(MPID_Comm *comm)
{
   TRACE_ERR("MPIDI_Comm_coll_query enter\n");
   int rc = 0, i, j;
   size_t num_algorithms[2];
   TRACE_ERR("Getting geometry from comm %p\n", comm);
   pami_geometry_t *geom = comm->mpid.geometry;;
   for(i = 0; i < PAMI_XFER_COUNT; i++)
   {
      if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE ||
         i == PAMI_XFER_FENCE)
         continue;
      rc = PAMI_Geometry_algorithms_num(MPIDI_Context[0],
                                        geom,
                                        i,
                                        num_algorithms);
      TRACE_ERR("Num algorithms of type %d: %zd %zd\n", i, num_algorithms[0], num_algorithms[1]);
      if(rc != PAMI_SUCCESS)
      {
         fprintf(stderr,"PAMI_Geometry_algorithms_num returned %d for type %d\n", rc, i);
         continue;
      }

      comm->mpid.coll_count[i][0] = 0;
      comm->mpid.coll_count[i][1] = 0;
      if(num_algorithms[0])
      {
         comm->mpid.coll_algorithm[i][0] = (pami_algorithm_t *)
               MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
         comm->mpid.coll_metadata[i][0] = (pami_metadata_t *)
               MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
         comm->mpid.coll_algorithm[i][1] = (pami_algorithm_t *)
               MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[1]);
         comm->mpid.coll_metadata[i][1] = (pami_metadata_t *)
               MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[1]);
         comm->mpid.coll_count[i][0] = num_algorithms[0];
         comm->mpid.coll_count[i][1] = num_algorithms[1];

         /* Despite the bad name, this looks at algorithms associated with
          * the geometry, NOT inherent physical properties of the geometry*/
         rc = PAMI_Geometry_algorithms_query(MPIDI_Context[0],
                                             geom,
                                             i,
                                             comm->mpid.coll_algorithm[i][0],
                                             comm->mpid.coll_metadata[i][0],
                                             num_algorithms[0],
                                             comm->mpid.coll_algorithm[i][1],
                                             comm->mpid.coll_metadata[i][1],
                                             num_algorithms[1]);
         if(rc != PAMI_SUCCESS)
         {
            fprintf(stderr,"PAMI_Geometry_algorithms_query returned %d for type %d\n", rc, i);
            continue;
         }

         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         {
            for(j = 0; j < num_algorithms[0]; j++)
               fprintf(stderr,"comm[%p] coll type %d, algorithm %d[0]: %s\n", comm, i, j, comm->mpid.coll_metadata[i][0][j].name);
            for(j = 0; j < num_algorithms[1]; j++)
               fprintf(stderr,"comm[%p] coll type %d, algorithm %d[1]: %s\n", comm, i, j, comm->mpid.coll_metadata[i][1][j].name);
         }
      }
   }
   /* Determine if we have protocols for these maybe, rather than just setting them? */
   comm->coll_fns->Barrier      = MPIDO_Barrier;
   comm->coll_fns->Bcast        = MPIDO_Bcast;
   comm->coll_fns->Allreduce    = MPIDO_Allreduce;
   comm->coll_fns->Allgather    = MPIDO_Allgather;
   comm->coll_fns->Allgatherv   = MPIDO_Allgatherv;
   comm->coll_fns->Scatterv     = MPIDO_Scatterv;
   comm->coll_fns->Scatter      = MPIDO_Scatter;
   comm->coll_fns->Gather       = MPIDO_Gather;
//   comm->coll_fns->Gatherv      = MPIDO_Gatherv;
//   comm->coll_fns->Reduce       = MPIDO_Reduce;

   TRACE_ERR("MPIDI_Comm_coll_query exit\n");
}


void MPIDI_Comm_world_setup()
{
  TRACE_ERR("MPIDI_Comm_world_setup enter\n");

  /* Anything special required for COMM_WORLD goes here */
   MPID_Comm *comm;
   comm = MPIR_Process.comm_world;

  TRACE_ERR("MPIDI_Comm_world_setup exit\n");
}
