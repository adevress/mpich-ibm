#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) fprintf x

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
   MPID_Comm *comm_world;
   if(comm->coll_fns) MPIU_Free(comm->coll_fns);
   comm->coll_fns = NULL;

   comm->coll_fns = (MPID_Collops *)MPIU_Malloc(sizeof(MPID_Collops));
   MPID_assert(comm->coll_fns != NULL);
   memset(comm->coll_fns, 0, sizeof(MPID_Collops));

   if(comm->comm_kind != MPID_INTRACOMM) return;

   MPID_Comm_get_ptr(MPI_COMM_WORLD, comm_world);
   TRACE_ERR((stderr,"MPIDI_Coll_comm_create exit\n"));


}

void MPIDI_Coll_comm_destroy(MPID_Comm *comm)
{
   TRACE_ERR((stderr,"MPIDI_Coll_comm_destroy enter\n"));
   TRACE_ERR((stderr,"MPIDI_Coll_comm_destroy exit\n"));

}

void MPIDI_Comm_world_setup()
{
   TRACE_ERR((stderr,"MPIDI_Comm_world_setup enter\n"));
   int rc = 0;
   char *envopts;
   int useshmembarrier = 1;
   int useshmembcast = 1;
   int useshmemallreduce = 1;
   MPID_Comm *world = MPIR_Process.comm_world;

   envopts = getenv("PAMI_BARRIER");
   if(envopts != NULL)
   {
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
                  malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
         world->mpid.barrier_metas = (pami_metadata_t *)
                  malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
         /* Despite the bad name, this looks at algorithms associated with
          * the geometry, NOT inherent physical properties of the geometry*/
         rc = PAMI_Geometry_query(MPIDI_Context[0],
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
                     malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
         world->mpid.allreduce_metas = (pami_metadata_t *)
                     malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
         rc = PAMI_Geometry_query(MPIDI_Context[0],
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

         world->mpid.bcasts = (pami_algorithm_t *)malloc(sizeof(pami_algorithm_t) *
                              num_algorithms[0]);
         world->mpid.bcast_metas = (pami_metadata_t *)malloc(sizeof(pami_metadata_t) *
                              num_algorithms[0]);

         rc = PAMI_Geometry_query(MPIDI_Context[0],
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
         
      
