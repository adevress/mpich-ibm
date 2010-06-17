/*  (C)Copyright IBM Corp.  2007, 2010  */
/**
 * \file src/comm/mpid_comm.c
 * \brief ???
 */


#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) //fprintf x

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
  TRACE_ERR((stderr,"MPIDI_Coll_comm_create enter\n"));
  if (!MPIDI_Process.optimized.collectives)
    return;

  comm->coll_fns = (MPID_Collops *)MPIU_Malloc(sizeof(MPID_Collops));
  MPID_assert(comm->coll_fns != NULL);
  memset(comm->coll_fns, 0, sizeof(MPID_Collops));

  if(comm->comm_kind != MPID_INTRACOMM) return;

  TRACE_ERR((stderr,"MPIDI_Coll_comm_create exit\n"));
}

void MPIDI_Coll_comm_destroy(MPID_Comm *comm)
{
  TRACE_ERR((stderr,"MPIDI_Coll_comm_destroy enter\n"));
  if (!MPIDI_Process.optimized.collectives)
    return;

  MPIU_TestFree(&comm->coll_fns);

  TRACE_ERR((stderr,"MPIDI_Coll_comm_destroy exit\n"));
}

void MPIDI_Comm_world_setup()
{
  TRACE_ERR((stderr,"MPIDI_Comm_world_setup enter\n"));
  if (!MPIDI_Process.optimized.collectives)
    return;

  int rc = 0;
  char *envopts;
  int useshmembarrier = 1;
  int useshmembcast = 1;
  int useshmemallreduce = 1;
  int useglueallgather = 1;
  int i;
  MPID_Comm *world = MPIR_Process.comm_world;

   for(i=0;i<4;i++) world->mpid.allgathervs[i] = 0; /* turn them all off for now */
   envopts = getenv("PAMI_ALLGATHERV");
   if(envopts != NULL)
   {
      TRACE_ERR((stderr,"allgatherv: %s\n", envopts));
      if(strncasecmp(envopts, "ALLT", 4) == 0) /*alltoall based */
      {
         world->mpid.allgathervs[0] = 1;
      }
      else if(strncasecmp(envopts, "ALLR", 4) == 0) /* allreduce */
      {
         world->mpid.allgathervs[1] = 1;
      }
      else if(strncasecmp(envopts, "B", 1) == 0) /* bcast */
      {
         world->mpid.allgathervs[2] = 1;
      }
      else if(strncasecmp(envopts, "M", 1) == 0) /* mpich */
      {
         for(i=0;i<4;i++) world->mpid.allgathervs[i] = 1;
      }
   }

   for(i=0;i<4;i++) world->mpid.allgathers[i] = 0; /* turn them all off for now */
   envopts = getenv("PAMI_ALLGATHER");
   if(envopts != NULL)
   {
      TRACE_ERR((stderr,"allgather: %s\n", envopts));
      if(strncasecmp(envopts, "ALLT", 4) == 0) /*alltoall based */
      {
         world->mpid.allgathers[0] = 1;
      }
      else if(strncasecmp(envopts, "ALLR", 4) == 0) /* allreduce */
      {
         world->mpid.allgathers[1] = 1;
      }
      else if(strncasecmp(envopts, "B", 1) == 0) /* bcast */
      {
         world->mpid.allgathers[2] = 1;
      }
      else if(strncasecmp(envopts, "M", 1) == 0) /* mpich */
      {
         for(i=0;i<4;i++) world->mpid.allgathers[i] = 1;
      }
   }

   envopts = getenv("PAMI_SCATTERV");
   if(envopts != NULL)
   {
      TRACE_ERR((stderr,"scatterv: %s\n", envopts));
      world->mpid.scattervs[0] = world->mpid.scattervs[1] = 0; /* turn them all off for now */
      if(strncasecmp(envopts, "B", 1) == 0)
         world->mpid.scattervs[0] = 1;
      else if(strncasecmp(envopts, "A", 1) == 0)
         world->mpid.scattervs[1] = 1;
      else
         world->mpid.scattervs[0] = world->mpid.scattervs[1] = 0;
   }
   envopts = getenv("PAMI_SCATTER");
   if(envopts != NULL)
   {
      TRACE_ERR((stderr,"scatter: %s\n", envopts));
      if(strncasecmp(envopts, "B", 1) == 0)
         world->mpid.optscatter = 1;
      else
         world->mpid.optscatter = 0;
   }

  envopts = getenv("PAMI_BARRIER");
  if(envopts != NULL)
    {
      TRACE_ERR((stderr,"barrier: %s\n", envopts));
      if(strncasecmp(envopts, "S", 1) == 0) /* shmem */
        {
          useshmembarrier = 1;
        }
      else if(strncasecmp(envopts, "M", 1) == 0) /* mpich */
        {
          useshmembarrier = 0;
        }
    }
  TRACE_ERR((stderr,"shmem barrier: %d\n", useshmembarrier));

  envopts = getenv("PAMI_BCAST");
  if(envopts != NULL)
    {
      TRACE_ERR((stderr,"bcast: %s\n", envopts));
      if(strncasecmp(envopts, "S", 1) == 0) /* shmem */
        {
          useshmembcast = 1;
        }
      else if(strncasecmp(envopts, "M", 1) == 0) /* mpich */
        {
          useshmembcast = 0;
        }
    }
  TRACE_ERR((stderr,"shmem bcast: %d\n", useshmembcast));

  envopts = getenv("PAMI_ALLREDUCE");
  if(envopts != NULL)
    {
      TRACE_ERR((stderr,"allreduce: %s\n", envopts));
      if(strncasecmp(envopts, "S", 1) == 0) /* shmem */
        {
          useshmemallreduce = 1;
        }
      else if(strncasecmp(envopts, "M", 1) == 0) /* mpich */
        {
          useshmemallreduce = 0;
        }
    }
  TRACE_ERR((stderr,"shmem allreduce: %d\n", useshmemallreduce));


  world->mpid.bcasts = NULL;
  world->mpid.barriers = NULL;
  world->mpid.allreduces = NULL;
  world->mpid.bcast_metas = NULL;
  world->mpid.barrier_metas = NULL;
  world->mpid.allreduce_metas = NULL;
  world->mpid.allgathers[0] = 1; /* guaranteed to work */
  int num_algorithms[2] = {0};

  /* Don't even bother registering if we are using mpich only */
  if(useshmembarrier)
    {
      rc = PAMI_Geometry_algorithms_num(MPIDI_Context[0],
                                        world->mpid.geometry,
                                        PAMI_XFER_BARRIER,
                                        num_algorithms);
      TRACE_ERR((stderr,"After barrier num\n"));
      assert(rc == PAMI_SUCCESS);
      if(num_algorithms[0])
        {
          TRACE_ERR((stderr,"world geometry has %d barriers[0] and %d barriers[1]\n",
                     num_algorithms[0], num_algorithms[1]));

          world->mpid.barriers = (pami_algorithm_t *)
            MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
          world->mpid.barrier_metas = (pami_metadata_t *)
            MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
          /* Despite the bad name, this looks at algorithms associated with
           * the geometry, NOT inherent physical properties of the geometry*/
          rc = PAMI_Geometry_algorithms_query(MPIDI_Context[0],
                                              world->mpid.geometry,
                                              PAMI_XFER_BARRIER,
                                              world->mpid.barriers,
                                              world->mpid.barrier_metas,
                                              num_algorithms[0],
                                              NULL,
                                              NULL,
                                              0);
          assert(rc == PAMI_SUCCESS);
        }
      TRACE_ERR((stderr,"barriers registered, assigning\n"));
      world->coll_fns->Barrier             = MPIDO_Barrier;
    }

  if(useshmemallreduce)
    {
      rc = PAMI_Geometry_algorithms_num(MPIDI_Context[0],
                                        world->mpid.geometry,
                                        PAMI_XFER_ALLREDUCE,
                                        num_algorithms);
      assert(rc == PAMI_SUCCESS);
      if(num_algorithms[0])
        {
          TRACE_ERR((stderr,"world geometry has %d allred[0] and %d allred[1]\n",
                     num_algorithms[0], num_algorithms[1]));

          world->mpid.allreduces = (pami_algorithm_t *)
            MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
          world->mpid.allreduce_metas = (pami_metadata_t *)
            MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
          rc = PAMI_Geometry_algorithms_query(MPIDI_Context[0],
                                              world->mpid.geometry,
                                              PAMI_XFER_ALLREDUCE,
                                              world->mpid.allreduces,
                                              world->mpid.allreduce_metas,
                                              num_algorithms[0],
                                              NULL,
                                              NULL,
                                              0);
          assert(rc == PAMI_SUCCESS);
        }
      TRACE_ERR((stderr,"allreduce registered, assigning\n"));
      world->coll_fns->Allreduce = MPIDO_Allreduce;
    }


  if(useshmembcast)
    {

      rc = PAMI_Geometry_algorithms_num(MPIDI_Context[0], // this needs figured out
                                        world->mpid.geometry,
                                        PAMI_XFER_BROADCAST,
                                        num_algorithms);

      assert(rc == PAMI_SUCCESS);

      if(num_algorithms[0])
        {
          TRACE_ERR((stderr,"world geometry has %d bcasts[0] and %d bcasts[1]\n",
                     num_algorithms[0], num_algorithms[1]));

          world->mpid.bcasts = (pami_algorithm_t *)MPIU_Malloc(sizeof(pami_algorithm_t) *
                                                               num_algorithms[0]);
          world->mpid.bcast_metas = (pami_metadata_t *)MPIU_Malloc(sizeof(pami_metadata_t) *
                                                                   num_algorithms[0]);

          rc = PAMI_Geometry_algorithms_query(MPIDI_Context[0],
                                              world->mpid.geometry,
                                              PAMI_XFER_BROADCAST,
                                              world->mpid.bcasts,
                                              world->mpid.bcast_metas,
                                              num_algorithms[0],
                                              NULL,
                                              NULL,
                                              0);
          assert(rc == PAMI_SUCCESS);
        }
      TRACE_ERR((stderr,"assigning bcast fns\n"));
      world->coll_fns->Bcast               = MPIDO_Bcast;
    }


  // at this point, i think we have a usable barrier/bcast

  TRACE_ERR((stderr,"MPIDI_Comm_world_setup exit\n"));
}
