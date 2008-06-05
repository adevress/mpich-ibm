/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/comm/collselect/mpid_coll.c
 * \brief Collective setup
 */
#include "mpido_coll.h"

#warning reasonable hack for now
#define MAXGEOMETRIES 65536


static DCMF_Geometry_t *mpid_geometrytable[MAXGEOMETRIES];
MPIDI_CollectiveProtocol_t MPIDI_CollectiveProtocols;

/*
 * geometries have a 'comm' ID which needs to be equivalently unique as
 * MPIs context_ids. So, we set geometry comm to context_id. Unfortunately
 * there is no trivial way to convert a context_id back to which MPI comm
 * it belongs to so this gross table is here for now. It will be replaced
 * probably with a lazy allocated list. Whatever goes here will have to be
 * cleaned up in comm_destroy as well
 */
static DCMF_Geometry_t *
getGeometryRequest(int comm)
{
   assert(mpid_geometrytable[comm%MAXGEOMETRIES] != NULL);
   return mpid_geometrytable[comm%MAXGEOMETRIES];
}



static int barriers_num=0;
static DCMF_CollectiveProtocol_t *barriers[DCMF_NUM_BARRIER_PROTOCOLS];

static inline int BARRIER_REGISTER(DCMF_Barrier_Protocol proto,
				   DCMF_CollectiveProtocol_t *proto_ptr,
				   DCMF_Barrier_Configuration_t *config)
{
  int rc;
  config->protocol = proto;
  rc = DCMF_Barrier_register(proto_ptr, config);
  if (rc == DCMF_SUCCESS)
    barriers[barriers_num++] = proto_ptr;
  MPID_assert_debug(barriers_num <= DCMF_NUM_BARRIER_PROTOCOLS);
  return rc;
}

static int local_barriers_num=0;
/* Local barriers PLUS room for one standard/global barrier 
   (DCMF_TORUS_BINOMIAL_BARRIER_PROTOCOL)*/
static DCMF_CollectiveProtocol_t *local_barriers[DCMF_NUM_LOCAL_BARRIER_PROTOCOLS+1];

static inline int LOCAL_BARRIER_REGISTER(DCMF_Barrier_Protocol proto,
                                         DCMF_CollectiveProtocol_t *proto_ptr,
                                         DCMF_Barrier_Configuration_t *config)
{
  int rc;
  config->protocol = proto;
  rc = DCMF_Barrier_register(proto_ptr, config);
  if (rc == DCMF_SUCCESS)
    local_barriers[local_barriers_num++] = proto_ptr;
  MPID_assert_debug(local_barriers_num <= DCMF_NUM_LOCAL_BARRIER_PROTOCOLS+1);
  return rc;
}

static inline int BROADCAST_REGISTER(DCMF_Broadcast_Protocol proto,
				     DCMF_CollectiveProtocol_t *proto_ptr,
				     DCMF_Broadcast_Configuration_t *config)
{
   config->protocol = proto;
   return DCMF_Broadcast_register(proto_ptr, config);
}


static inline int ASYNC_BROADCAST_REGISTER(DCMF_AsyncBroadcast_Protocol proto,
					   DCMF_CollectiveProtocol_t *proto_ptr,
					   DCMF_AsyncBroadcast_Configuration_t *config)
{
   config->protocol = proto;
   config->isBuffered = 1;
   config->cb_geometry = getGeometryRequest;
   return DCMF_AsyncBroadcast_register(proto_ptr, config);
}


static inline int ALLREDUCE_REGISTER(DCMF_Allreduce_Protocol proto,
                              DCMF_CollectiveProtocol_t *proto_ptr,
                              DCMF_Allreduce_Configuration_t *config)
{
   config->protocol = proto;
   return DCMF_Allreduce_register(proto_ptr, config);
}

static inline int ALLTOALLV_REGISTER(DCMF_Alltoallv_Protocol proto,
				     DCMF_CollectiveProtocol_t *proto_ptr,
				     DCMF_Alltoallv_Configuration_t *config)
{
   config->protocol = proto;
   return DCMF_Alltoallv_register(proto_ptr, config);
}

static inline int REDUCE_REGISTER(DCMF_Reduce_Protocol proto,
				  DCMF_CollectiveProtocol_t *proto_ptr,
				  DCMF_Reduce_Configuration_t *config)
{
   config->protocol = proto;
   return DCMF_Reduce_register(proto_ptr, config);
}



/** \brief Helper used to register all the collective protocols at initialization */
void MPIDI_Coll_register(void)
{
  DCMF_Embedded_Info_Set * properties = &MPIDI_CollectiveProtocols.properties;
   DCMF_Barrier_Configuration_t   barrier_config;
   DCMF_Broadcast_Configuration_t broadcast_config;
   DCMF_AsyncBroadcast_Configuration_t a_broadcast_config;
   DCMF_Allreduce_Configuration_t allreduce_config;
   DCMF_Alltoallv_Configuration_t alltoallv_config;
   DCMF_Reduce_Configuration_t    reduce_config;
   DCMF_GlobalBarrier_Configuration_t gbarrier_config;
   DCMF_GlobalBcast_Configuration_t gbcast_config;
   DCMF_GlobalAllreduce_Configuration_t gallreduce_config;

   DCMF_Result rc;

   /* Register the global functions first */
   
   /* ---------------------------------- */
   /* Register global barrier          */
   /* ---------------------------------- */
   gbarrier_config.protocol = DCMF_GI_GLOBALBARRIER_PROTOCOL;
   rc = DCMF_GlobalBarrier_register(&MPIDI_Protocols.globalbarrier,
                                    &gbarrier_config);
   /* registering the global barrier failed, so don't use it */
   if(rc != DCMF_SUCCESS)
   {
     DCMF_INFO_UNSET(properties, DCMF_USE_GI_BARRIER);
   }


   /* ---------------------------------- */
   /* Register global broadcast          */
   /* ---------------------------------- */
   gbcast_config.protocol = DCMF_TREE_GLOBALBCAST_PROTOCOL;

   /* Check if we are in dual or vn mode and that all ranks in the system */
   /* are physically located on one node. */
   unsigned size = DCMF_Messager_size ();
   if (size <= mpid_hw.tSize)
   {
     unsigned rank, x[4], y[4], z[4], t[4];
     DCMF_Messager_rank2torus (0, &x[0], &y[0], &z[0], &t[0]);

     gbcast_config.protocol = DCMF_INTRANODE_GLOBALBCAST_PROTOCOL;
     for (rank = 1; rank < size; rank++)
     {
       DCMF_Messager_rank2torus (rank, &x[rank], &y[rank], &z[rank], &t[rank]);
       if ((x[rank-1] != x[rank]) ||
           (y[rank-1] != y[rank]) ||
           (z[rank-1] != z[rank]))
       {
         gbcast_config.protocol = DCMF_TREE_GLOBALBCAST_PROTOCOL;
         break;
       }
     }
   }
   rc = DCMF_GlobalBcast_register(&MPIDI_Protocols.globalbcast, &gbcast_config);

   /* most likely, we lack shared memory and therefore can't use this */
   if(rc != DCMF_SUCCESS)
   {
     DCMF_INFO_UNSET(properties, DCMF_USE_TREE_BCAST);
   }

   /* ---------------------------------- */
   /* Register global allreduce          */
   /* ---------------------------------- */
   gallreduce_config.protocol = DCMF_TREE_GLOBALALLREDUCE_PROTOCOL;
   rc = DCMF_GlobalAllreduce_register(&MPIDI_Protocols.globalallreduce,
                                      &gallreduce_config);

   /* most likely, we lack shared memory and therefore can't use this */
   /* reduce uses the allreduce protocol */
   if(rc != DCMF_SUCCESS)
     {
       /* Try the ccmi tree if we were trying global tree */
       if (DCMF_INFO_ISSET(properties, DCMF_USE_TREE_ALLREDUCE))
	 DCMF_INFO_SET(properties, DCMF_USE_CCMI_TREE_ALLREDUCE);
       if (DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE))
	 DCMF_INFO_SET(properties, DCMF_USE_CCMI_TREE_REDUCE);
       
       DCMF_INFO_UNSET(properties, DCMF_USE_TREE_ALLREDUCE);
       DCMF_INFO_UNSET(properties, DCMF_USE_TREE_REDUCE);
     }


   /* register first barrier protocols now */
   barrier_config.cb_geometry = getGeometryRequest;

   /* set the function that will find the [all]reduce geometry on unexpected callbacks*/
   allreduce_config.cb_geometry = getGeometryRequest;
   reduce_config.cb_geometry = getGeometryRequest;

   /* set configuration flags in the config*/
   allreduce_config.reuse_storage = 
     DCMF_INFO_ISSET(properties, DCMF_USE_STORAGE_ALLREDUCE);

   reduce_config.reuse_storage = 
     DCMF_INFO_ISSET(properties, DCMF_USE_STORAGE_REDUCE);


   /* Other env vars can be checked at communicator creation time
    * but barriers are associated with a geometry and this knowledge
    * isn't available to mpido_barrier
    */
   if(DCMF_INFO_ISSET(properties, DCMF_USE_GI_BARRIER))
     {
       if(BARRIER_REGISTER(DCMF_GI_BARRIER_PROTOCOL,
			   &MPIDI_CollectiveProtocols.gi_barrier,
			   &barrier_config) != DCMF_SUCCESS)
	 DCMF_INFO_UNSET(properties, DCMF_USE_GI_BARRIER);
     }

   /*
    * Always register a binomial barrier for collectives in subcomms, just
    * choose not to use it at mpido_barrier
    */
   if(BARRIER_REGISTER(DCMF_TORUS_BINOMIAL_BARRIER_PROTOCOL,
		       &MPIDI_CollectiveProtocols.binomial_barrier,
		       &barrier_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_BINOM_BARRIER);
   
   /* if we don't even get a binomial barrier, we are in trouble */
   MPID_assert_debug(barriers_num >  0);
   
   /*
    * Register local barriers for the geometry.
    * Both a true local lockbox barrier and a global binomial
    * barrier (which can be used non-optimally).  The geometry
    * will decide internally if/which to use.
    * They are not used directly by MPICH but must be initialized.
    */
   if(DCMF_INFO_ISSET(properties, DCMF_USE_LOCKBOX_LBARRIER))
     {
       if(LOCAL_BARRIER_REGISTER(DCMF_LOCKBOX_BARRIER_PROTOCOL,
				 &MPIDI_CollectiveProtocols.lockbox_localbarrier,
				 &barrier_config) != DCMF_SUCCESS)
	 DCMF_INFO_UNSET(properties, DCMF_USE_LOCKBOX_LBARRIER);
     }

   /*
    * Always register a binomial barrier for collectives in subcomms
    */
  if(LOCAL_BARRIER_REGISTER(DCMF_TORUS_BINOMIAL_BARRIER_PROTOCOL,
			    &MPIDI_CollectiveProtocols.binomial_localbarrier,
			    &barrier_config) != DCMF_SUCCESS)
    DCMF_INFO_UNSET(properties, DCMF_USE_BINOM_LBARRIER);


   /* MPID doesn't care if this actually works.  Let someone else
    * handle problems as needed.
    * MPID_assert_debug(local_barriers_num >  0);
    */


   /* Register broadcast protocols */
   if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_BCAST))
     {
       if(BROADCAST_REGISTER(DCMF_TREE_BROADCAST_PROTOCOL,
			     &MPIDI_CollectiveProtocols.tree_bcast,
			     &broadcast_config) != DCMF_SUCCESS)
	 DCMF_INFO_UNSET(properties, DCMF_USE_TREE_BCAST);
     }

   if(BROADCAST_REGISTER(DCMF_TORUS_RECTANGLE_BROADCAST_PROTOCOL,
			 &MPIDI_CollectiveProtocols.rectangle_bcast,
			 &broadcast_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_RECT_BCAST);

   
   ASYNC_BROADCAST_REGISTER(DCMF_TORUS_ASYNCBROADCAST_RECTANGLE_PROTOCOL,
			    &MPIDI_CollectiveProtocols.async_rectangle_bcast,
			    &a_broadcast_config);
   
   //   BROADCAST_REGISTER(DCMF_TORUS_RECT_BCAST_3COLOR_PROTOCOL,
   //                     &MPIDI_CollectiveProtocols.broadcast.rectangle.threecolor,
   //                     &broadcast_config);

   if(BROADCAST_REGISTER(DCMF_TORUS_BINOMIAL_BROADCAST_PROTOCOL,
			 &MPIDI_CollectiveProtocols.binomial_bcast,
			 &broadcast_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_BINOM_BCAST);

   ASYNC_BROADCAST_REGISTER(DCMF_TORUS_ASYNCBROADCAST_BINOMIAL_PROTOCOL,
			    &MPIDI_CollectiveProtocols.async_binomial_bcast,
			    &a_broadcast_config);

   /* Register allreduce protocols */
   if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_ALLREDUCE) ||
      DCMF_INFO_ISSET(properties, DCMF_USE_CCMI_TREE_ALLREDUCE))
     {
       if(ALLREDUCE_REGISTER(DCMF_TREE_ALLREDUCE_PROTOCOL,
			     &MPIDI_CollectiveProtocols.tree_allreduce,
			     &allreduce_config) != DCMF_SUCCESS)
	 {
	   DCMF_INFO_UNSET(properties, DCMF_USE_TREE_ALLREDUCE);
	   DCMF_INFO_UNSET(properties, DCMF_USE_CCMI_TREE_ALLREDUCE);
	 }
     }
   
   if((ALLREDUCE_REGISTER(DCMF_TREE_PIPELINED_ALLREDUCE_PROTOCOL,
			  &MPIDI_CollectiveProtocols.pipelinedtree_allreduce,
			  &allreduce_config) != DCMF_SUCCESS) ||
      (ALLREDUCE_REGISTER(DCMF_TREE_DPUT_PIPELINED_ALLREDUCE_PROTOCOL,
			  &MPIDI_CollectiveProtocols.pipelinedtree_dput_allreduce,
			  &allreduce_config) != DCMF_SUCCESS) )
     DCMF_INFO_UNSET(properties, DCMF_USE_PIPELINED_TREE_ALLREDUCE);


   if(ALLREDUCE_REGISTER(DCMF_TORUS_RECTANGLE_ALLREDUCE_PROTOCOL,
			 &MPIDI_CollectiveProtocols.rectangle_allreduce,
			 &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_RECT_ALLREDUCE);

   if(ALLREDUCE_REGISTER(DCMF_TORUS_RECTANGLE_RING_ALLREDUCE_PROTOCOL,
			 &MPIDI_CollectiveProtocols.rectanglering_allreduce,
			 &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_RECTRING_ALLREDUCE);

   if(ALLREDUCE_REGISTER(DCMF_TORUS_BINOMIAL_ALLREDUCE_PROTOCOL,
			 &MPIDI_CollectiveProtocols.binomial_allreduce,
			 &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_BINOM_ALLREDUCE);

   if (ALLREDUCE_REGISTER(DCMF_TORUS_ASYNC_RECTANGLE_ALLREDUCE_PROTOCOL,
			  &MPIDI_CollectiveProtocols.async_rectangle_allreduce,
			  &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_ARECT_ALLREDUCE);

     if (ALLREDUCE_REGISTER(DCMF_TORUS_ASYNC_RECTANGLE_RING_ALLREDUCE_PROTOCOL,
			    &MPIDI_CollectiveProtocols.async_ringrectangle_allreduce,
			    &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_ARECTRING_ALLREDUCE);
     
     if (ALLREDUCE_REGISTER(DCMF_TORUS_ASYNC_BINOMIAL_ALLREDUCE_PROTOCOL,
			    &MPIDI_CollectiveProtocols.async_binomial_allreduce,
			    &allreduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_ABINOM_ALLREDUCE);
     
   /* Register alltoallv protocols */
   if(ALLTOALLV_REGISTER(DCMF_TORUS_ALLTOALLV_PROTOCOL,
			 &MPIDI_CollectiveProtocols.torus_alltoallv,
			 &alltoallv_config) != DCMF_SUCCESS)
     {
       DCMF_INFO_UNSET(properties, DCMF_USE_TORUS_ALLTOALL);
       DCMF_INFO_UNSET(properties, DCMF_USE_TORUS_ALLTOALLV);
       DCMF_INFO_UNSET(properties, DCMF_USE_TORUS_ALLTOALLW);
     }
   
   /* Register reduce protocols */
   if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE) ||
      DCMF_INFO_ISSET(properties, DCMF_USE_CCMI_TREE_REDUCE))
     {
       if(REDUCE_REGISTER(DCMF_TREE_REDUCE_PROTOCOL,
			  &MPIDI_CollectiveProtocols.tree_reduce,
			  &reduce_config) != DCMF_SUCCESS)
	 {
	   DCMF_INFO_UNSET(properties, DCMF_USE_TREE_REDUCE);
	   DCMF_INFO_UNSET(properties, DCMF_USE_CCMI_TREE_REDUCE);
	 }
     }
   
   if(REDUCE_REGISTER(DCMF_TORUS_BINOMIAL_REDUCE_PROTOCOL,
		      &MPIDI_CollectiveProtocols.binomial_reduce,
		      &reduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_BINOM_REDUCE);
   
   if(REDUCE_REGISTER(DCMF_TORUS_RECTANGLE_REDUCE_PROTOCOL,
		      &MPIDI_CollectiveProtocols.rectangle_reduce,
		      &reduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_RECT_REDUCE);
   
   if(REDUCE_REGISTER(DCMF_TORUS_RECTANGLE_RING_REDUCE_PROTOCOL,
		      &MPIDI_CollectiveProtocols.rectanglering_reduce,
		      &reduce_config) != DCMF_SUCCESS)
     DCMF_INFO_UNSET(properties, DCMF_USE_RECTRING_REDUCE);
}


/**
 * \brief Create collective communicators
 *
 * Hook function to handle collective-specific optimization during communicator creation
 */
void MPIDI_Coll_Comm_create (MPID_Comm *comm)
{
  int global, x_size, y_size, z_size, t_size;
  unsigned min_coords[4], max_coords[4];
  DCMF_Embedded_Info_Set * comm_prop, * coll_prop;
  MPID_Comm *comm_world;

  MPID_assert (comm!= NULL);



  if (comm->coll_fns) MPIU_Free(comm->coll_fns);
  comm->coll_fns=NULL;   /* !!! Intercomm_merge does not NULL the fcns,
                          * leading to stale functions for new comms.
                          * We'll null it here until argonne confirms
                          * this is the correct behavior of merge
                          */

  comm_prop = &(comm -> dcmf.properties);
  coll_prop = &MPIDI_CollectiveProtocols.properties;

  /* unset all properties of a comm by default */
  DCMF_INFO_ZERO(comm_prop);
  
  comm -> dcmf.bcast_iter = 0;

  /* ****************************************** */
  /* Allocate space for the collective pointers */
  /* ****************************************** */

   comm->coll_fns = (MPID_Collops *)MPIU_Malloc(sizeof(MPID_Collops));
   MPID_assert(comm->coll_fns != NULL);
   memset(comm->coll_fns, 0, sizeof(MPID_Collops));


  /* If we are an intracomm, MPICH should handle */
  if (comm->comm_kind != MPID_INTRACOMM) return;
  /* User may disable all collectives */
  if (!MPIDI_Process.optimized.collectives) return;

   MPID_Comm_get_ptr(MPI_COMM_WORLD, comm_world);
   MPID_assert_debug(comm_world != NULL);

  /* let us assume global context is comm_world */
  global = 1;
  
  if(MPIR_ThreadInfo.thread_provided == MPI_THREAD_MULTIPLE)
    {
      DCMF_INFO_SET(comm_prop, DCMF_THREADED_MODE);
      if(comm != comm_world)
	global = 0;
    }
  else /* single MPI thread. */
    {
      /* and if we are not dup of comm_world, global context is not safe */
      if(comm->local_size != comm_world->local_size)
	global = 0;
    }


  /* ******************************************************* */
  /* Setup Barriers and geometry for this communicator       */
  /* ******************************************************* */
  DCMF_Geometry_initialize(&comm->dcmf.geometry,
			   comm->context_id,
			   (unsigned*)comm->vcr,
			   comm->local_size,
			   barriers,
			   barriers_num,
			   local_barriers,
			   local_barriers_num,
			   &comm->dcmf.barrier,
			   MPIDI_CollectiveProtocols.numcolors,
			   global);
  
  mpid_geometrytable[(comm->context_id)%MAXGEOMETRIES] = &comm->dcmf.geometry;

  /* ****************************************** */
  /* These are ALL the pointers in the object   */
  /* ****************************************** */
  
  comm->coll_fns->Barrier        = MPIDO_Barrier;
  comm->coll_fns->Bcast          = MPIDO_Bcast;
  comm->coll_fns->Reduce         = MPIDO_Reduce;
  comm->coll_fns->Allreduce      = MPIDO_Allreduce;
  comm->coll_fns->Alltoall       = MPIDO_Alltoall;
  comm->coll_fns->Alltoallv      = MPIDO_Alltoallv;
  comm->coll_fns->Alltoallw      = MPIDO_Alltoallw;
  comm->coll_fns->Allgather      = MPIDO_Allgather;
  comm->coll_fns->Allgatherv     = MPIDO_Allgatherv;
  comm->coll_fns->Gather         = MPIDO_Gather;
  comm->coll_fns->Gatherv        = MPIDO_Gatherv;
  comm->coll_fns->Scatter        = MPIDO_Scatter;
  comm->coll_fns->Scatterv       = MPIDO_Scatterv;
  comm->coll_fns->Reduce_scatter = MPIDO_Reduce_scatter;
  comm->coll_fns->Scan           = MPIDO_Scan;
  comm->coll_fns->Exscan         = MPIDO_Exscan;

  /* set geometric properties of the communicator */
  if (DCMF_ISPOF2(comm->local_size))
    DCMF_INFO_SET(comm_prop, DCMF_POF2_COMM);

  if (DCMF_ISEVEN(comm->local_size))
    DCMF_INFO_SET(comm_prop, DCMF_EVEN_COMM);

  DCMF_INFO_SET(comm_prop, DCMF_TORUS_COMM);

  if (comm -> local_size == DCMF_Messager_size())
    DCMF_INFO_SET(comm_prop, DCMF_TREE_COMM);

  if (global)
    DCMF_INFO_SET(comm_prop, DCMF_GLOBAL_CONTEXT);
  else
    DCMF_INFO_SET(comm_prop, DCMF_SUB_COMM);

  MPIR_Barrier(comm);

  /* find if the communicator is a rectangle */
  MPIR_Allreduce(mpid_hw.Coord, min_coords,4, MPI_UNSIGNED, MPI_MIN, comm);
  MPIR_Allreduce(mpid_hw.Coord, max_coords,4, MPI_UNSIGNED, MPI_MAX, comm);
  
  t_size = (unsigned) (max_coords[0] - min_coords[0] + 1);
  z_size = (unsigned) (max_coords[1] - min_coords[1] + 1);
  y_size = (unsigned) (max_coords[2] - min_coords[2] + 1);
  x_size = (unsigned) (max_coords[3] - min_coords[3] + 1);

  if (x_size * y_size * z_size * t_size == comm -> local_size)
    {
      DCMF_INFO_SET(comm_prop, DCMF_RECT_COMM);

      if (x_size > 1 && y_size > 1 && z_size > 1)
	DCMF_INFO_SET(comm_prop, DCMF_3D_RECT_COMM);

      else if ((x_size > 1 && y_size > 1) ||
	       (x_size > 1 && z_size > 1) ||
	       (y_size > 1 && z_size > 1))
	DCMF_INFO_SET(comm_prop, DCMF_2D_RECT_COMM);

      else /* number of dims must be at least 1 if it is a rectangle */
	DCMF_INFO_SET(comm_prop, DCMF_1D_RECT_COMM);
    }

  /* end of setting geometric properties of the communicator */

  /* quick setting of conditions that results to defaulting to MPICH */
  if (comm -> comm_kind != MPID_INTRACOMM || comm->local_size < 3)
    DCMF_MSET_INFO(comm_prop,
		   DCMF_USE_MPICH_BARRIER,
		   DCMF_USE_MPICH_BCAST,
		   DCMF_USE_MPICH_ALLTOALL,
		   DCMF_USE_MPICH_ALLTOALLW,
		   DCMF_USE_MPICH_ALLTOALLV,
		   DCMF_USE_MPICH_ALLGATHER,
		   DCMF_USE_MPICH_ALLGATHERV,
		   DCMF_USE_MPICH_ALLREDUCE,
		   DCMF_USE_MPICH_REDUCE,
		   DCMF_USE_MPICH_GATHER,
		   DCMF_USE_MPICH_SCATTER,
		   DCMF_USE_MPICH_SCATTERV,
		   DCMF_USE_MPICH_REDUCESCATTER,
		   DCMF_END_ARGS);
	
  else
    {
      /* 
	 we basically be optimistic and assume all conditions are available 
	 for all protocols based on the mpidi_protocol properties. As such, we 
	 copy the informative bits from coll_prop to comm_prop. Then, we check 
	 the geometry bits of the communicator to uncheck any bit for a 
	 protocol 
      */
      DCMF_INFO_OR(coll_prop, comm_prop);

      if (!DCMF_INFO_ISSET(comm_prop, DCMF_RECT_COMM))
	{
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ARECT_BCAST);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECT_BCAST);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECT_BCAST_ALLGATHER);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ARECT_BCAST_ALLGATHER);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECT_BCAST_ALLGATHERV);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ARECT_BCAST_ALLGATHERV);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECT_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECTRING_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ARECT_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ARECTRING_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECT_REDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_RECTRING_REDUCE);
	}
      
      if (DCMF_INFO_ISSET(comm_prop, DCMF_THREADED_MODE) && !global)
	{
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TORUS_ALLTOALL);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TORUS_ALLTOALLW);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TORUS_ALLTOALLV);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_ALLTOALL_SCATTERV);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_REDUCESCATTER);
	}

      if (!DCMF_INFO_ISSET(comm_prop, DCMF_GLOBAL_CONTEXT) ||
	  !DCMF_INFO_ISSET(comm_prop, DCMF_TREE_COMM))
	{
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_GI_BARRIER);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TREE_BCAST);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TREE_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_CCMI_TREE_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_PIPELINED_TREE_ALLREDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_TREE_REDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_CCMI_TREE_REDUCE);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_REDUCE_GATHER);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_BCAST_SCATTER);
	  DCMF_INFO_UNSET(comm_prop, DCMF_USE_REDUCESCATTER);
	}
    }

  comm -> dcmf.sndlen = NULL;
  comm -> dcmf.rcvlen = NULL;
  comm -> dcmf.sdispls = NULL;
  comm -> dcmf.rdispls = NULL;
  comm -> dcmf.sndcounters = NULL;
  comm -> dcmf.rcvcounters = NULL;
  
  if (DCMF_INFO_ISSET(coll_prop, DCMF_USE_PREMALLOC_ALLTOALL))
    {
      int type_sz = sizeof(unsigned);
      comm -> dcmf.sndlen = MPIU_Malloc(type_sz * comm->local_size);
      comm -> dcmf.rcvlen = MPIU_Malloc(type_sz * comm->local_size);
      comm -> dcmf.sdispls = MPIU_Malloc(type_sz * comm->local_size);
      comm -> dcmf.rdispls = MPIU_Malloc(type_sz * comm->local_size);
      comm -> dcmf.sndcounters = MPIU_Malloc(type_sz * comm->local_size);
      comm -> dcmf.rcvcounters = MPIU_Malloc(type_sz * comm->local_size);
    }

  MPIR_Barrier(comm);
}


/**
 * \brief Destroy a communicator
 *
 * Hook function to handle collective-specific optimization during communicator destruction
 *
 * \note  We want to free the associated coll_fns buffer at this time.
 */
void MPIDI_Coll_Comm_destroy (MPID_Comm *comm)
{
  MPID_assert (comm != NULL);
  if (comm->coll_fns)
    MPIU_Free(comm->coll_fns);
  DCMF_Geometry_free(&comm->dcmf.geometry);
  if(comm->dcmf.sndlen)
    MPIU_Free(comm->dcmf.sndlen);
  if(comm->dcmf.rcvlen)
    MPIU_Free(comm->dcmf.rcvlen);
  if(comm->dcmf.sdispls)
    MPIU_Free(comm->dcmf.sdispls);
  if(comm->dcmf.rdispls)
    MPIU_Free(comm->dcmf.rdispls);
  if(comm->dcmf.sndcounters)
    MPIU_Free(comm->dcmf.sndcounters);
  if(comm->dcmf.rcvcounters)
    MPIU_Free(comm->dcmf.rcvcounters);

  comm->dcmf.sndlen  =
  comm->dcmf.rcvlen  =
  comm->dcmf.sdispls =
  comm->dcmf.rdispls =
  comm->dcmf.sndcounters =
  comm->dcmf.rcvcounters = NULL;
}
