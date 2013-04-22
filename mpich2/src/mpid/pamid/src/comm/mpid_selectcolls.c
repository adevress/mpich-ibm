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
 * \file src/comm/mpid_collselect.c
 * \brief Code for setting up collectives per geometry/communicator
 */

/*#define TRACE_ON */

#include <mpidimpl.h>

static char* MPIDI_Coll_type_name(int i)
{
  switch(i)
  {
  case PAMI_XFER_BROADCAST      : return("Broadcast");
  case PAMI_XFER_ALLREDUCE      : return("Allreduce");
  case PAMI_XFER_REDUCE         : return("Reduce");
  case PAMI_XFER_ALLGATHER      : return("Allgather");
  case PAMI_XFER_ALLGATHERV     : return("Allgatherv_size_t");
  case PAMI_XFER_ALLGATHERV_INT : return("Allgatherv");
  case PAMI_XFER_SCATTER        : return("Scatter");
  case PAMI_XFER_SCATTERV       : return("Scatterv_size_t");
  case PAMI_XFER_SCATTERV_INT   : return("Scatterv");
  case PAMI_XFER_GATHER         : return("Gather");
  case PAMI_XFER_GATHERV        : return("Gatherv_size_t");
  case PAMI_XFER_GATHERV_INT    : return("Gatherv");
  case PAMI_XFER_BARRIER        : return("Barrier");
  case PAMI_XFER_ALLTOALL       : return("Alltoall");
  case PAMI_XFER_ALLTOALLV      : return("Alltoallv_size_t");
  case PAMI_XFER_ALLTOALLV_INT  : return("Alltoallv");
  case PAMI_XFER_SCAN           : return("Scan");
  case PAMI_XFER_REDUCE_SCATTER : return("Reduce_scatter");
  default: return("AM Collective");
  }
}

static void MPIDI_Update_optimized_algorithm(pami_algorithm_t coll, 
                                                 int type,  /* must query vs always works */
                                                 int index,
                                                 MPID_Comm *comm)
{

  comm->mpid.optimized_algorithm_type[coll][0] = type;
  TRACE_ERR("Update_coll for protocol %s, type: %d index: %d\n", 
            comm->mpid.algorithm_metadata_list[coll][type][index].name, type, index);

  comm->mpid.optimized_algorithm[coll][0] = 
  comm->mpid.algorithm_list[coll][type][index];

  memcpy(&comm->mpid.optimized_algorithm_metadata[coll][0],
         &comm->mpid.algorithm_metadata_list[coll][type][index],
         sizeof(pami_metadata_t));
}

static void MPIDI_Check_preallreduce(char *env, MPID_Comm *comm, char *name, int constant)
{
  /* If a given protocol only requires a check for nonlocal conditions and preallreduce
   * is turned off, we can consider it a always works protocol instead 
   */
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
static int MPIDI_Check_protocols(char *names[], MPID_Comm *comm, char *name, int constant)
{
  int i = 0;
  char *envopts;
  for(;; ++i)
  {
    if(names[i] == NULL)
      return 0;
    envopts = getenv(names[i]);
    if(envopts != NULL)
      break;
  }
  /* Now, change it if we have a match */
  if(envopts != NULL)
  {
    if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
      fprintf(stderr,"Checking %s against known %s protocols\n", envopts, name);
    /* Check if MPICH was specifically requested */
    if(strcasecmp(envopts, "MPICH") == 0)
    {
      TRACE_ERR("Selecting MPICH for %s\n", name);
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting MPICH as user selected protocol for %s on comm %p\n", name, comm);
      comm->mpid.optimized_algorithm_type[constant][0] = MPID_COLL_USE_MPICH;
      comm->mpid.optimized_algorithm[constant][0] = 0;
      return 0;
    }

    for(i=0; i < comm->mpid.num_algorithms[constant][0];i++)
    {
      if(strncasecmp(envopts, comm->mpid.algorithm_metadata_list[constant][0][i].name,strlen(envopts)) == 0)
      {
        MPIDI_Update_optimized_algorithm(constant, MPID_COLL_NOQUERY, i, comm);
        if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
          fprintf(stderr,"Setting %s as user selected protocol for %s on comm %p\n", comm->mpid.algorithm_metadata_list[constant][0][i].name, name, comm);
        return 0;
      }
    }
    for(i=0; i < comm->mpid.num_algorithms[constant][1];i++)
    {
      if(strncasecmp(envopts, comm->mpid.algorithm_metadata_list[constant][1][i].name,strlen(envopts)) == 0)
      {
        TRACE_ERR("Calling updatecoll...\n");
        MPIDI_Update_optimized_algorithm(constant, MPID_COLL_QUERY, i, comm);
        TRACE_ERR("Done calling update coll\n");
        if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
          fprintf(stderr,"Setting %s as user selected (query) protocol for %s on comm %p\n", comm->mpid.algorithm_metadata_list[constant][1][i].name, name, comm);
        return 0;
      }
    }
    /* An envvar was specified, so we should probably use MPICH of we can't find
     * the specified protocol */
    TRACE_ERR("Specified protocol %s was unavailable; using MPICH for %s\n", envopts, name);
    comm->mpid.optimized_algorithm_type[constant][0] = MPID_COLL_USE_MPICH;
    comm->mpid.optimized_algorithm[constant][0] = 0;
    return 0;
  }
  /* USE WHATEVER DEFAULTS ARE ALREADY SET.
     Looks like we didn't get anything, set NOSELECTION so automated selection can pick something
  comm->mpid.optimized_algorithm_type[constant][0] = MPID_COLL_DEFAULT; 
  comm->mpid.optimized_algorithm[constant][0] = 0; 
   */ 
  return 0;
}

void MPIDI_Comm_coll_envvars(MPID_Comm *comm)
{
  char *envopts;
  int i;
  MPID_assert_always(comm!=NULL);
  TRACE_ERR("MPIDI_Comm_coll_envvars enter\n");

  /* Set up always-works defaults */
  for(i = 0; i < PAMI_XFER_COUNT; i++)
  {
    if(i == PAMI_XFER_AMBROADCAST || i == PAMI_XFER_AMSCATTER ||
       i == PAMI_XFER_AMGATHER || i == PAMI_XFER_AMREDUCE)
      continue;

    /* Initialize to noselection instead of noquery for PE/FCA stuff. Is this the right answer? */
    comm->mpid.optimized_algorithm_type[i][1] = MPID_COLL_USE_MPICH;
    if(/* PAMID glue doesn't use these, so don't report them (it's misleading) */
       i == PAMI_XFER_ALLGATHERV ||
       i == PAMI_XFER_SCATTERV ||
       i == PAMI_XFER_GATHERV ||
       i == PAMI_XFER_ALLTOALLV ||
       i == PAMI_XFER_REDUCE_SCATTER)
        ;/* noop */
    else if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
      fprintf(stderr,"Setting up collective %d (%s) on comm %p\n", i,  MPIDI_Coll_type_name(i),comm);
    if((comm->mpid.num_algorithms[i][0] == 0) && (comm->mpid.num_algorithms[i][1] == 0))
    {
      comm->mpid.optimized_algorithm_type[i][0] = MPID_COLL_USE_MPICH;
      comm->mpid.optimized_algorithm[i][0] = 0;
    }
    else if(comm->mpid.num_algorithms[i][0] != 0)
    {
      comm->mpid.optimized_algorithm[i][0] = comm->mpid.algorithm_list[i][0][0];
      memcpy(&comm->mpid.optimized_algorithm_metadata[i][0], &comm->mpid.algorithm_metadata_list[i][0][0],
             sizeof(pami_metadata_t));
      comm->mpid.optimized_algorithm_type[i][0] = MPID_COLL_DEFAULT;
    }
    else
    {
      comm->mpid.optimized_algorithm[i][0] = comm->mpid.algorithm_list[i][1][0];
      memcpy(&comm->mpid.optimized_algorithm_metadata[i][0], &comm->mpid.algorithm_metadata_list[i][1][0],
             sizeof(pami_metadata_t));
      comm->mpid.optimized_algorithm_type[i][0] = MPID_COLL_DEFAULT_QUERY;
    }
    /* No cutoff, by default, for whatever we selected above */
    comm->mpid.optimized_algorithm_cutoff_size[i][0] = 0; 

    /* Init 'large message' protocol fields to MPICH even though they shouldn't be used (no cutoff set) */
    comm->mpid.optimized_algorithm_type[i][1] = MPID_COLL_USE_MPICH;
    comm->mpid.optimized_algorithm[i][1] = 0;
    comm->mpid.optimized_algorithm_cutoff_size[i][1] = 0; 
  }


  TRACE_ERR("Checking env vars\n");

  MPIDI_Check_preallreduce("PAMID_COLLECTIVE_ALLGATHER_PREALLREDUCE", comm, "allgather",
                           MPID_ALLGATHER_PREALLREDUCE);

  MPIDI_Check_preallreduce("PAMID_COLLECTIVE_ALLGATHERV_PREALLREDUCE", comm, "allgatherv",
                           MPID_ALLGATHERV_PREALLREDUCE);

  MPIDI_Check_preallreduce("PAMID_COLLECTIVE_ALLREDUCE_PREALLREDUCE", comm, "allreduce",
                           MPID_ALLREDUCE_PREALLREDUCE);

  MPIDI_Check_preallreduce("PAMID_COLLECTIVE_BCAST_PREALLREDUCE", comm, "broadcast",
                           MPID_BCAST_PREALLREDUCE);

  MPIDI_Check_preallreduce("PAMID_COLLECTIVE_SCATTERV_PREALLREDUCE", comm, "scatterv",
                           MPID_SCATTERV_PREALLREDUCE);

  {
    TRACE_ERR("Checking bcast\n");
    char* names[] = {"PAMID_COLLECTIVE_BCAST", "MP_S_MPI_BCAST", NULL};
    MPIDI_Check_protocols(names, comm, "broadcast", PAMI_XFER_BROADCAST);
  }
  {
    TRACE_ERR("Checking allreduce\n");
    char* names[] = {"PAMID_COLLECTIVE_ALLREDUCE", "MP_S_MPI_ALLREDUCE", NULL};
    MPIDI_Check_protocols(names, comm, "allreduce", PAMI_XFER_ALLREDUCE);
  }
  {
    TRACE_ERR("Checking barrier\n");
    char* names[] = {"PAMID_COLLECTIVE_BARRIER", "MP_S_MPI_BARRIER", NULL};
    MPIDI_Check_protocols(names, comm, "barrier", PAMI_XFER_BARRIER);
  }
  {
    TRACE_ERR("Checking alltaoll\n");
    char* names[] = {"PAMID_COLLECTIVE_ALLTOALL", NULL};
    MPIDI_Check_protocols(names, comm, "alltoall", PAMI_XFER_ALLTOALL);
  }
  comm->mpid.optreduce = 0;
  envopts = getenv("PAMID_COLLECTIVE_REDUCE");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue allreduce for reduce\n");
      comm->mpid.optreduce = 1;
    }
  }
  /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
  {
    TRACE_ERR("Checking reduce\n");
    char* names[] = {"PAMID_COLLECTIVE_REDUCE", "MP_S_MPI_REDUCE", NULL};
    MPIDI_Check_protocols(names, comm, "reduce", PAMI_XFER_REDUCE);
  }
  {
    TRACE_ERR("Checking alltoallv\n");
    char* names[] = {"PAMID_COLLECTIVE_ALLTOALLV", NULL};
    MPIDI_Check_protocols(names, comm, "alltoallv", PAMI_XFER_ALLTOALLV_INT);
  }
  {
    TRACE_ERR("Checking gatherv\n");
    char* names[] = {"PAMID_COLLECTIVE_GATHERV",  NULL};
    MPIDI_Check_protocols(names, comm, "gatherv", PAMI_XFER_GATHERV_INT);
  }
  {
    TRACE_ERR("Checking scan\n");
    char* names[] = {"PAMID_COLLECTIVE_SCAN", NULL};
    MPIDI_Check_protocols(names, comm, "scan", PAMI_XFER_SCAN);
  }

  comm->mpid.scattervs[0] = comm->mpid.scattervs[1] = 0;

  TRACE_ERR("Checking scatterv\n");
  envopts = getenv("PAMID_COLLECTIVE_SCATTERV");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_BCAST") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue bcast for scatterv\n");
      comm->mpid.scattervs[0] = 1;
    }
    else if(strcasecmp(envopts, "GLUE_ALLTOALLV") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue alltoallv for scatterv\n");
      comm->mpid.scattervs[1] = 1;
    }
  }
  { /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
    char* names[] = {"PAMID_COLLECTIVE_SCATTERV", NULL};
    MPIDI_Check_protocols(names, comm, "scatterv", PAMI_XFER_SCATTERV_INT);
  }

  TRACE_ERR("Checking scatter\n");
  comm->mpid.optscatter = 0;
  envopts = getenv("PAMID_COLLECTIVE_SCATTER");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_BCAST") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_bcast for scatter\n");
      comm->mpid.optscatter = 1;
    }
  }
  { /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
    char* names[] = {"PAMID_COLLECTIVE_SCATTER", NULL};
    MPIDI_Check_protocols(names, comm, "scatter", PAMI_XFER_SCATTER);
  }

  TRACE_ERR("Checking allgather\n");
  comm->mpid.allgathers[0] = comm->mpid.allgathers[1] = comm->mpid.allgathers[2] = 0;
  envopts = getenv("PAMID_COLLECTIVE_ALLGATHER");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_allreduce for allgather\n");
      comm->mpid.allgathers[0] = 1;
    }

    else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_bcast for allgather\n");
      comm->mpid.allgathers[1] = 1;
    }

    else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_alltoall for allgather\n");
      comm->mpid.allgathers[2] = 1;
    }
  }
  { /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
    char* names[] = {"PAMID_COLLECTIVE_ALLGATHER", "MP_S_MPI_ALLGATHER", NULL};
    MPIDI_Check_protocols(names, comm, "allgather", PAMI_XFER_ALLGATHER);
  }

  TRACE_ERR("Checking allgatherv\n");
  comm->mpid.allgathervs[0] = comm->mpid.allgathervs[1] = comm->mpid.allgathervs[2] = 0;
  envopts = getenv("PAMID_COLLECTIVE_ALLGATHERV");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_allreduce for allgatherv\n");
      comm->mpid.allgathervs[0] = 1;
    }

    else if(strcasecmp(envopts, "GLUE_BCAST") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_bcast for allgatherv\n");
      comm->mpid.allgathervs[1] = 1;
    }

    else if(strcasecmp(envopts, "GLUE_ALLTOALL") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue_alltoall for allgatherv\n");
      comm->mpid.allgathervs[2] = 1;
    }
  }
  { /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
    char* names[] = {"PAMID_COLLECTIVE_ALLGATHERV", "MP_S_MPI_ALLGATHERV", NULL};
    MPIDI_Check_protocols(names, comm, "allgatherv", PAMI_XFER_ALLGATHERV_INT);
  }

  TRACE_ERR("Checking gather\n");
  comm->mpid.optgather[0] = comm->mpid.optgather[1] = 0;
  envopts = getenv("PAMID_COLLECTIVE_GATHER");
  if(envopts != NULL)
  {
    if(strcasecmp(envopts, "GLUE_ALLGATHER") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue allgather for gather\n");
      comm->mpid.optgather[1] = 1;
    }
    else if(strcasecmp(envopts, "GLUE_REDUCE") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue reduce for gather\n");
      comm->mpid.optgather[0] = 1;
    }
    else if(strcasecmp(envopts, "GLUE_ALLREDUCE") == 0)
    {
      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
        fprintf(stderr,"Selecting glue allreduce for gather\n");
      comm->mpid.optgather[0] = 2;
    }
  }
  { /* In addition to glue protocols, check for other PAMI protocols and check for PE now */
    char* names[] = {"PAMID_COLLECTIVE_GATHER", NULL};
    MPIDI_Check_protocols(names, comm, "gather", PAMI_XFER_GATHER);
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

    comm->mpid.num_algorithms[i][0] = 0;
    comm->mpid.num_algorithms[i][1] = 0;
    if(num_algorithms[0] || num_algorithms[1])
    {
      comm->mpid.algorithm_list[i][0] = (pami_algorithm_t *)
                                        MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[0]);
      comm->mpid.algorithm_metadata_list[i][0] = (pami_metadata_t *)
                                                 MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[0]);
      comm->mpid.algorithm_list[i][1] = (pami_algorithm_t *)
                                        MPIU_Malloc(sizeof(pami_algorithm_t) * num_algorithms[1]);
      comm->mpid.algorithm_metadata_list[i][1] = (pami_metadata_t *)
                                                 MPIU_Malloc(sizeof(pami_metadata_t) * num_algorithms[1]);
      comm->mpid.num_algorithms[i][0] = num_algorithms[0];
      comm->mpid.num_algorithms[i][1] = num_algorithms[1];

      /* Despite the bad name, this looks at algorithms associated with
       * the geometry, NOT inherent physical properties of the geometry*/

      rc = PAMI_Geometry_algorithms_query(geom,
                                          i,
                                          comm->mpid.algorithm_list[i][0],
                                          comm->mpid.algorithm_metadata_list[i][0],
                                          num_algorithms[0],
                                          comm->mpid.algorithm_list[i][1],
                                          comm->mpid.algorithm_metadata_list[i][1],
                                          num_algorithms[1]);
      if(rc != PAMI_SUCCESS)
      {
        fprintf(stderr,"PAMI_Geometry_algorithms_query returned %d for type %d\n", rc, i);
        continue;
      }

      if(/* PAMID glue doesn't use these, so don't report them (it's misleading) */
         i == PAMI_XFER_ALLGATHERV ||
         i == PAMI_XFER_SCATTERV ||
         i == PAMI_XFER_GATHERV ||
         i == PAMI_XFER_ALLTOALLV ||
         i == PAMI_XFER_REDUCE_SCATTER)
        continue;

      if(MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_0 && comm->rank == 0)
      {
        for(j = 0; j < num_algorithms[0]; j++)
          fprintf(stderr,"comm[%p] coll type %d (%s), algorithm %d 0: %s\n", comm, i, MPIDI_Coll_type_name(i), j, comm->mpid.algorithm_metadata_list[i][0][j].name);
        for(j = 0; j < num_algorithms[1]; j++)
          fprintf(stderr,"comm[%p] coll type %d (%s), algorithm %d 1: %s\n", comm, i, MPIDI_Coll_type_name(i), j, comm->mpid.algorithm_metadata_list[i][1][j].name);
        if(i == PAMI_XFER_ALLGATHERV_INT || i == PAMI_XFER_ALLGATHER)
        {
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLREDUCE\n", comm, i, MPIDI_Coll_type_name(i));
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_BCAST\n", comm, i, MPIDI_Coll_type_name(i));
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLTOALL\n", comm, i, MPIDI_Coll_type_name(i));
        }
        if(i == PAMI_XFER_SCATTERV_INT)
        {
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_BCAST (unimplemented)\n", comm, i, MPIDI_Coll_type_name(i));
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLTOALLV (unimplemented)\n", comm, i, MPIDI_Coll_type_name(i));
        }
        if(i == PAMI_XFER_SCATTER)
        {
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_BCAST\n", comm, i, MPIDI_Coll_type_name(i));
        }
        if(i == PAMI_XFER_REDUCE)
        {
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLREDUCE\n", comm, i, MPIDI_Coll_type_name(i));
        }
        if(i == PAMI_XFER_GATHER)
        {
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLGATHER\n", comm, i, MPIDI_Coll_type_name(i));
/*        Not exposing GLUE_REDUCE but still selectable if you know it's there...
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_REDUCE\n", comm, i, MPIDI_Coll_type_name(i));
*/
          fprintf(stderr,"comm[%p] coll type %d (%s), \"glue\" algorithm: GLUE_ALLREDUCE\n", comm, i, MPIDI_Coll_type_name(i));
        }
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
  comm->coll_fns->Gatherv      = MPIDO_Gatherv;
  comm->coll_fns->Reduce       = MPIDO_Reduce;
  comm->coll_fns->Scan         = MPIDO_Scan;
  comm->coll_fns->Exscan       = MPIDO_Exscan;

  /* MPI-3 Support, no optimized collectives hooked in yet */
  comm->coll_fns->Ibarrier              = MPIR_Ibarrier_intra;
  comm->coll_fns->Ibcast                = MPIR_Ibcast_intra;
  comm->coll_fns->Igather               = MPIR_Igather_intra;
  comm->coll_fns->Igatherv              = MPIR_Igatherv;
  comm->coll_fns->Iscatter              = MPIR_Iscatter_intra;
  comm->coll_fns->Iscatterv             = MPIR_Iscatterv;
  comm->coll_fns->Iallgather            = MPIR_Iallgather_intra;
  comm->coll_fns->Iallgatherv           = MPIR_Iallgatherv_intra;
  comm->coll_fns->Ialltoall             = MPIR_Ialltoall_intra;
  comm->coll_fns->Ialltoallv            = MPIR_Ialltoallv_intra;
  comm->coll_fns->Ialltoallw            = MPIR_Ialltoallw_intra;
  comm->coll_fns->Iallreduce            = MPIR_Iallreduce_intra;
  comm->coll_fns->Ireduce               = MPIR_Ireduce_intra;
  comm->coll_fns->Ireduce_scatter       = MPIR_Ireduce_scatter_intra;
  comm->coll_fns->Ireduce_scatter_block = MPIR_Ireduce_scatter_block_intra;
  comm->coll_fns->Iscan                 = MPIR_Iscan_rec_dbl;
  comm->coll_fns->Iexscan               = MPIR_Iexscan;
  comm->coll_fns->Neighbor_allgather    = MPIR_Neighbor_allgather_default;
  comm->coll_fns->Neighbor_allgatherv   = MPIR_Neighbor_allgatherv_default;
  comm->coll_fns->Neighbor_alltoall     = MPIR_Neighbor_alltoall_default;
  comm->coll_fns->Neighbor_alltoallv    = MPIR_Neighbor_alltoallv_default;
  comm->coll_fns->Neighbor_alltoallw    = MPIR_Neighbor_alltoallw_default;

  /* MPI-3 Support, optimized collectives hooked in */
  comm->coll_fns->Ibarrier_optimized              = MPIDO_Ibarrier;
  comm->coll_fns->Ibcast_optimized                = MPIDO_Ibcast;
  comm->coll_fns->Iallgather_optimized            = MPIDO_Iallgather;
  comm->coll_fns->Iallgatherv_optimized           = MPIDO_Iallgatherv;
  comm->coll_fns->Iallreduce_optimized            = MPIDO_Iallreduce;
  comm->coll_fns->Ialltoall_optimized             = MPIDO_Ialltoall;
  comm->coll_fns->Ialltoallv_optimized            = MPIDO_Ialltoallv;
  comm->coll_fns->Ialltoallw_optimized            = MPIDO_Ialltoallw;
  comm->coll_fns->Iexscan_optimized               = MPIDO_Iexscan;
  comm->coll_fns->Igather_optimized               = MPIDO_Igather;
  comm->coll_fns->Igatherv_optimized              = MPIDO_Igatherv;
  comm->coll_fns->Ireduce_scatter_block_optimized = MPIDO_Ireduce_scatter_block;
  comm->coll_fns->Ireduce_scatter_optimized       = MPIDO_Ireduce_scatter;
  comm->coll_fns->Ireduce_optimized               = MPIDO_Ireduce;
  comm->coll_fns->Iscan_optimized                 = MPIDO_Iscan;
  comm->coll_fns->Iscatter_optimized              = MPIDO_Iscatter;
  comm->coll_fns->Iscatterv_optimized             = MPIDO_Iscatterv;

  TRACE_ERR("MPIDI_Comm_coll_query exit\n");
}

