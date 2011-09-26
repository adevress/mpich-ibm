/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/comm/mpid_collselect.c
 * \brief Code for setting up collectives per geometry/communicator
 */

//#define TRACE_ON

#include <mpidimpl.h>

static void MPIDI_Update_coll(pami_algorithm_t coll, 
                              int type,
                              int index,
                              MPID_Comm *comm);

static void MPIDI_Update_coll(pami_algorithm_t coll, 
                       int type,  /* must query vs always works */
                       int index,
                       MPID_Comm *comm_ptr)
{

   comm_ptr->mpid.user_selectedvar[coll] = type;

   /* Are we in the 'must query' list? If so determine how "bad" it is */
   if(type == MPID_COLL_QUERY)
   {
      /* First, is a check always required? */
      if(comm_ptr->mpid.coll_metadata[coll][type][index].check_correct.values.checkrequired)
      {
         /* We must have a check_fn */
         MPID_assert_always(comm_ptr->mpid.coll_metadata[coll][type][index].check_fn !=NULL);
         comm_ptr->mpid.user_selectedvar[coll] = MPID_COLL_CHECK_FN_REQUIRED;
      }
      else if(comm_ptr->mpid.coll_metadata[coll][type][index].check_fn != NULL)
      {
         /* For now, if there's a check_fn we will always call it and not cache.
            We *could* be smarter about this eventually.                        */
         comm_ptr->mpid.user_selectedvar[coll] = MPID_COLL_ALWAYS_QUERY;
      }

   }

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
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
         fprintf(stderr,"Checking %s against known %s protocols\n", envopts, name);
      /* We could maybe turn off the MPIDO_{} fn ptr instead? */
      if(strcasecmp(envopts, "MPICH") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using MPICH for %s\n", name);
         comm->mpid.user_selectedvar[constant] = MPID_COLL_USE_MPICH;
         return 0;
      }
      if(strncasecmp(envopts, "GLUE_", 5) == 0)
         return -1;

      /* Default to MPICH if we don't find a match for the specific protocol specified */
      comm->mpid.user_selectedvar[constant] = MPID_COLL_USE_MPICH;

      for(i=0; i < comm->mpid.coll_count[constant][0];i++)
      {
         if(strncasecmp(envopts, comm->mpid.coll_metadata[constant][0][i].name,strlen(envopts)) == 0)
         {
            MPIDI_Update_coll(constant, MPID_COLL_NOQUERY, i, comm);
            if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
               fprintf(stderr,"setting %s as default %s for comm %p\n", comm->mpid.coll_metadata[constant][0][i].name, name, comm);
            return 0;
         }
      }
      for(i=0; i < comm->mpid.coll_count[constant][1];i++)
      {
         if(strncasecmp(envopts, comm->mpid.coll_metadata[constant][1][i].name,strlen(envopts)) == 0)
         {
            MPIDI_Update_coll(constant, MPID_COLL_QUERY, i, comm);
            if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
               fprintf(stderr,"setting (query required protocol) %s as default %s for comm %p\n", comm->mpid.coll_metadata[constant][1][i].name, name, comm);
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
   MPID_assert_always(comm!=NULL);
   TRACE_ERR("MPIDI_Comm_coll_envvars enter\n");

   /* Set up always-works defaults */
   for(i = 0; i < PAMI_XFER_COUNT; i++)
   {
      if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE)
         continue;

      comm->mpid.user_selectedvar[i] = MPID_COLL_NOQUERY;
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Setting up collective %d on comm %p\n", i, comm);
      if(comm->mpid.coll_count[i][0] == 0 && comm->mpid.coll_count[i][1] == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"There are no 'always works' protocols of type %d. This could be a problem later in your app\n", i);
         comm->mpid.user_selectedvar[i] = MPID_COLL_USE_MPICH;
      }
      else
      {
         comm->mpid.user_selected[i] = comm->mpid.coll_algorithm[i][0][0];
         memcpy(&comm->mpid.user_metadata[i], &comm->mpid.coll_metadata[i][0][0],
               sizeof(pami_metadata_t));
      }
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

   MPIDI_Check_protocols("PAMI_REDUCE", comm, "reduce", PAMI_XFER_REDUCE);

   /* Assume MPI will use _INT protocols but no need to make user know that */
   MPIDI_Check_protocols("PAMI_ALLTOALLV", comm, "alltoallv", PAMI_XFER_ALLTOALLV_INT);
   MPIDI_Check_protocols("PAMI_ALLTOALLV_INT", comm, "alltoallv", PAMI_XFER_ALLTOALLV_INT);

   MPIDI_Check_protocols("PAMI_GATHERV", comm, "gatherv", PAMI_XFER_GATHERV_INT);

   comm->mpid.scattervs[0] = comm->mpid.scattervs[1] = 0;
   rc = MPIDI_Check_protocols("PAMI_SCATTERV", comm, "scatterv", PAMI_XFER_SCATTERV_INT);
   if(rc != 0)
   {
      envopts = getenv("PAMI_SCATTERV");
      if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using glue bcast for scatterv\n");
         comm->mpid.scattervs[0] = 1;
      }
      else if(strcasecmp(envopts, "GLUE_ALLTOALLV") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgather\n");
         comm->mpid.allgathers[0] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgather\n");
         comm->mpid.allgathers[1] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using glue_allreduce for allgatherv\n");
         comm->mpid.allgathervs[0] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
            fprintf(stderr,"Using glue_bcast for allgatherv\n");
         comm->mpid.allgathervs[1] = 1;
      }

      else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
      {
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
         i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE)
         continue;
      rc = PAMI_Geometry_algorithms_num(geom,
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
      if(num_algorithms[0] || num_algorithms[1])
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

         /* BES TODO I am assuming all contexts have the same algorithms. Probably
          * need to investigate that assumption
          */
         rc = PAMI_Geometry_algorithms_query(geom,
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

         if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
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
   comm->coll_fns->Alltoallv    = MPIDO_Alltoallv;
   comm->coll_fns->Alltoall     = MPIDO_Alltoall;
//   comm->coll_fns->Gatherv      = MPIDO_Gatherv;
//   comm->coll_fns->Reduce       = MPIDO_Reduce;

   TRACE_ERR("MPIDI_Comm_coll_query exit\n");
}


