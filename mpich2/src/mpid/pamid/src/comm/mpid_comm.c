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

void MPIDI_Coll_comm_create(MPID_Comm *comm)
{
   int rc;
   int geom_init = 1;
   int i;
   pami_geometry_range_t *slices;

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
      slices = malloc(sizeof(pami_geometry_range_t) * comm->local_size);
      for(i=0;i<comm->local_size;i++)
      {
         slices[i].lo = MPID_VCR_GET_LPID(comm->vcr, i);
         slices[i].hi = MPID_VCR_GET_LPID(comm->vcr, i);
      }
      rc = PAMI_Geometry_create_taskrange(MPIDI_Client,
                                         NULL,
                                         0,
                                         &comm->mpid.geometry,
                                         NULL, /*MPIDI_Process.world_geometry,*/
                                         comm->context_id,
                                         slices,
                                         (size_t)comm->local_size,
                                         MPIDI_Context[0],
                                         geom_cb_done,
                                         &geom_init);

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


void MPIDI_Update_coll(pami_algorithm_t coll, 
                       int type, 
                       int index,
                       MPID_Comm *comm_ptr)
{
   comm_ptr->mpid.user_selected[coll] = 
      comm_ptr->mpid.coll_algorithm[coll][type][index];
   memcpy(&comm_ptr->mpid.user_metadata[coll],
          &comm_ptr->mpid.coll_metadata[coll][type][index],
          sizeof(pami_metadata_t));
}
   
void MPIDI_Comm_coll_envvars(MPID_Comm *comm)
{
   char *envopts;
   int i;
   TRACE_ERR("MPIDI_Comm_coll_envvars enter\n");

   /* Set up always-works defaults */
   for(i = 0; i < PAMI_XFER_COUNT; i++)
   {
      if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE ||
         i == PAMI_XFER_FENCE)
         continue;

      comm->mpid.user_selectedvar[i] = 1;
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Setting up collective %d on comm %p\n", i, comm);
      comm->mpid.user_selected[i] = comm->mpid.coll_algorithm[i][0][0];
      memcpy(&comm->mpid.user_metadata[i], &comm->mpid.coll_metadata[i][0][0],
            sizeof(pami_metadata_t));
   }
#if 0
   TRACE_ERR("Assigning always works protocols\n");
   /* eventually move this into the for() loop too? */
   comm->mpid.user_selected[PAMI_XFER_BROADCAST] =
      comm->mpid.coll_algorithm[PAMI_XFER_BROADCAST][0][0];
   comm->mpid.user_selected[PAMI_XFER_ALLREDUCE] =
      comm->mpid.coll_algorithm[PAMI_XFER_ALLREDUCE][0][0];
   comm->mpid.user_selected[PAMI_XFER_ALLGATHER] =
      comm->mpid.coll_algorithm[PAMI_XFER_ALLGATHER][0][0];
   comm->mpid.user_selected[PAMI_XFER_ALLGATHERV] =
      comm->mpid.coll_algorithm[PAMI_XFER_ALLGATHERV][0][0];
   comm->mpid.user_selected[PAMI_XFER_GATHER] =
      comm->mpid.coll_algorithm[PAMI_XFER_GATHER][0][0];
   comm->mpid.user_selected[PAMI_XFER_SCATTERV] =
      comm->mpid.coll_algorithm[PAMI_XFER_SCATTERV][0][0];
   comm->mpid.user_selected[PAMI_XFER_SCATTER] =
      comm->mpid.coll_algorithm[PAMI_XFER_SCATTER][0][0];
   comm->mpid.user_selected[PAMI_XFER_BARRIER] =
      comm->mpid.coll_algorithm[PAMI_XFER_BARRIER][0][0];

   TRACE_ERR("Memcpy user data\n");
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_BROADCAST],
      &comm->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_ALLREDUCE],
      &comm->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_ALLGATHER],
      &comm->mpid.coll_metadata[PAMI_XFER_ALLGATHER][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_ALLGATHERV],
      &comm->mpid.coll_metadata[PAMI_XFER_ALLGATHERV][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_GATHER],
      &comm->mpid.coll_metadata[PAMI_XFER_GATHER][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_SCATTERV],
      &comm->mpid.coll_metadata[PAMI_XFER_SCATTERV][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_SCATTER],
      &comm->mpid.coll_metadata[PAMI_XFER_SCATTER][0][0],
      sizeof(pami_metadata_t));
   memcpy(&comm->mpid.user_metadata[PAMI_XFER_BARRIER],
      &comm->mpid.coll_metadata[PAMI_XFER_BARRIER][0][0],
      sizeof(pami_metadata_t));

#endif
   TRACE_ERR("Checking env vars\n");

   envopts = getenv("PAMI_BCAST");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known bcast protocols\n", envopts);
      /* We could maybe turn off the MPIDO_{} fn ptr instead? */
      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for bcast\n");
         comm->mpid.user_selectedvar[PAMI_XFER_BROADCAST] = 0;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_BROADCAST][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_BROADCAST][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_BROADCAST, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default bcast for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_BROADCAST][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_BROADCAST, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default bcast for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   envopts = getenv("PAMI_ALLREDUCE");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known allreduce protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for allreduce\n");
         comm->mpid.user_selectedvar[PAMI_XFER_ALLREDUCE] = 0;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_ALLREDUCE][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLREDUCE, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default allreduce for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLREDUCE][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLREDUCE, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default allreduce for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   envopts = getenv("PAMI_BARRIER");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known barrier protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for barrier\n");
         comm->mpid.user_selectedvar[PAMI_XFER_BARRIER] = 0;
      }
      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_BARRIER][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_BARRIER][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_BARRIER, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default barrier for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_BARRIER][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_BARRIER, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default barrier for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   comm->mpid.scattervs[0] = comm->mpid.scattervs[1] = 0;
   envopts = getenv("PAMI_SCATTERV");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known scatterv protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for scatterv\n");
         comm->mpid.user_selectedvar[PAMI_XFER_SCATTERV] = 0;
      }

      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue bcast for scatterv\n");
         comm->mpid.scattervs[0] = 1;
      }

      if(strcasecmp(envopts, "GLUE_ALLTOALLV") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue alltoallv for scatterv\n");
         comm->mpid.scattervs[1] = 1;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_SCATTERV][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_SCATTERV][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_SCATTERV, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default scatterv for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_SCATTERV][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_SCATTERV, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default scatterv for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   comm->mpid.optscatter = 0;
   envopts = getenv("PAMI_SCATTER");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known scatter protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for scatter\n");
         comm->mpid.user_selectedvar[PAMI_XFER_SCATTER] = 0;
      }

      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for scatter\n");
         comm->mpid.optscatter = 1;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_SCATTER][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_SCATTER][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_SCATTER, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default scatter for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_SCATTER][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_SCATTER, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default scatter for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   comm->mpid.allgathers[0] = comm->mpid.allgathers[1] = comm->mpid.allgathers[2] = 0;
   envopts = getenv("PAMI_ALLGATHER");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known allgather protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for allgather\n");
         comm->mpid.user_selectedvar[PAMI_XFER_ALLGATHER] = 0;
      }

      if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgather\n");
         comm->mpid.allgathers[0] = 1;
      }

      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgather\n");
         comm->mpid.allgathers[1] = 1;
      }

      if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_alltoall for allgather\n");
         comm->mpid.allgathers[2] = 1;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_ALLGATHER][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLGATHER][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLGATHER, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default allgather for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLGATHER][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLGATHER, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default allgather for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   comm->mpid.allgathervs[0] = comm->mpid.allgathervs[1] = comm->mpid.allgathervs[2] = 0;
   envopts = getenv("PAMI_ALLGATHERV");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known allgatherv protocols\n", envopts);
         
      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for allgatherv\n");
         comm->mpid.user_selectedvar[PAMI_XFER_ALLGATHERV] = 0;
      }

      if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgatherv\n");
         comm->mpid.allgathervs[0] = 1;
      }

      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgatherv\n");
         comm->mpid.allgathervs[1] = 1;
      }

      if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using glue_alltoall for allgatherv\n");
         comm->mpid.allgathervs[2] = 1;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_ALLGATHERV][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLGATHERV][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLGATHERV, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default allgatherv for comm %p\n", envopts, comm);
            break;
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_ALLGATHERV][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_ALLGATHERV, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default allgatherv for comm %p\n", envopts, comm);
            break;
         }
      }
   }

   comm->mpid.optgather = 0;
   envopts = getenv("PAMI_GATHER");
   if(envopts != NULL)
   {
      if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known gather protocols\n", envopts);

      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for gather\n");
         comm->mpid.user_selectedvar[PAMI_XFER_GATHER] = 0;
      }

      if(strcasecmp(envopts, "GLUE_REDUCE") == 0)
      {
         if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
            fprintf(stderr,"using glue_reduce for gather\n");
         comm->mpid.optgather = 1;
      }

      for(i=0; i < comm->mpid.coll_count[PAMI_XFER_GATHER][0];i++)
      {
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_GATHER][0][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_GATHER, 0, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting %s as default gather for comm %p\n", envopts, comm);
         }
         if(strcasecmp(envopts, comm->mpid.coll_metadata[PAMI_XFER_GATHER][1][i].name) == 0)
         {
            MPIDI_Update_coll(PAMI_XFER_GATHER, 1, i, comm);
            if(MPIDI_Process.verbose >= 1 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default gather for comm %p\n", envopts, comm);
         }
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
