/* ---------------------------------------------------------------- */
/* (C)Copyright IBM Corp.  2007, 2008                               */
/* ---------------------------------------------------------------- */
/**
 * \file ad_bg_pset.c
 * \brief Definition of functions associated to structs ADIOI_BG_ProcInfo_t and ADIOI_BG_ConfInfo_t 
 */

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

//#define TRACE_ON 
#include <stdlib.h>
#include "ad_bg.h"
#include "ad_bg_pset.h"
#include "mpidimpl.h"
#include <firmware/include/personality.h>


ADIOI_BG_ProcInfo_t *
ADIOI_BG_ProcInfo_new()
{
    ADIOI_BG_ProcInfo_t *p = (ADIOI_BG_ProcInfo_t *) ADIOI_Malloc (sizeof(ADIOI_BG_ProcInfo_t));
    ADIOI_BG_assert ((p != NULL));
    return p;
}

ADIOI_BG_ProcInfo_t *
ADIOI_BG_ProcInfo_new_n( int n )
{
    ADIOI_BG_ProcInfo_t *p = (ADIOI_BG_ProcInfo_t *) ADIOI_Malloc (n * sizeof(ADIOI_BG_ProcInfo_t));
    ADIOI_BG_assert ((p != NULL));
    return p;
}

void
ADIOI_BG_ProcInfo_free( ADIOI_BG_ProcInfo_t *info )
{
    if (info != NULL) ADIOI_Free (info);
}

static void ADIOI_BG_ProcInfo_set(ADIOI_BG_ProcInfo_t *info, Personality_t pers, MPIX_Hardware_t hw,   int r)
{
//   #warning need to get ioNodeIndex;

   return;
   

   #if 0
    info->psetNum    = hw->idOfPset;
    info->xInPset    = hw->xCoord;
    info->yInPset    = hw->yCoord;
    info->zInPset    = hw->zCoord;
    info->cpuid      = hw->tCoord;
    info->rank       = r;
    info->rankInPset = hw->rankInPset;
   #endif
}


ADIOI_BG_ConfInfo_t *
ADIOI_BG_ConfInfo_new ()
{
    ADIOI_BG_ConfInfo_t *p = (ADIOI_BG_ConfInfo_t *) ADIOI_Malloc (sizeof(ADIOI_BG_ConfInfo_t));
    ADIOI_BG_assert ((p != NULL));
    return p;
}

/* needs a procinfo passed in too */
static void ADIOI_BG_ConfInfo_set(ADIOI_BG_ConfInfo_t *info, int s, int n_aggrs)
{
   return;
   
   #if 0
    info->PsetSize        = hw->sizeOfPset;
    info->numPsets        = (hw->xSize * hw->ySize *
					hw->zSize) / hw->sizeOfPset;
    info->isVNM           = (hw->tSize != 1);
    info->cpuidSize       = hw->tSize;
    info->virtualPsetSize = hw->sizeOfPset * hw->tSize;
    info->nProcs          = s;

    /* More complicated logic maybe needed for nAggrs specification */
    info->nAggrs          = n_aggrs;
    if ( info->nAggrs <=0 || MIN(info->nProcs, info->virtualPsetSize) < info->nAggrs ) 
        info->nAggrs      = ADIOI_BGL_NAGG_PSET_DFLT;
    if ( info->nAggrs > info->virtualPsetSize ) info->nAggrs = info->virtualPsetSize;

    info->aggRatio        = 1. * info->nAggrs / info->virtualPsetSize;
    if (info->aggRatio > 1) info->aggRatio = 1.;
    #endif 
    ;
}

void
ADIOI_BG_ConfInfo_free( ADIOI_BG_ConfInfo_t *info )
{
    if (info != NULL) ADIOI_Free (info);
}


typedef struct
{
   int rank;
   int bridge;
} sortstruct;

static int intsort(const void *p1, const void *p2)
{
   sortstruct *i1, *i2;
   i1 = (sortstruct *)p1;
   i2 = (sortstruct *)p2;
   return(i1->bridge - i2->bridge);
}


void 
ADIOI_BG_persInfo_init(ADIOI_BG_ConfInfo_t *conf, 
			ADIOI_BG_ProcInfo_t *proc, 
			int size, int rank, int n_aggrs, MPI_Comm comm)
{
   int i, iambridge=0, bridgerank, numbridge;
   int rc;
   int count = 0;
   sortstruct *array;
   sortstruct *bridges;
   int commsize;

   TRACE_ERR("Entering BG_persInfo_init, size: %d, rank: %d, n_aggrs: %d, comm: %d\n", size, rank, n_aggrs, (int)comm);

   Personality_t pers;
   MPIX_Hardware_t hw;
   MPIX_Hardware(&hw);

   Kernel_GetPersonality(&pers, sizeof(pers));

   proc->rank = rank;
   proc->coreID = hw.coreID;
   MPI_Comm_size(comm, &commsize);

   /* Determine the rank of the nearest bridge node */
   int bridgeCoords[6];
   bridgeCoords[0] = pers.Network_Config.cnBridge_A;
   bridgeCoords[1] = pers.Network_Config.cnBridge_B;
   bridgeCoords[2] = pers.Network_Config.cnBridge_C;
   bridgeCoords[3] = pers.Network_Config.cnBridge_D;
   bridgeCoords[4] = pers.Network_Config.cnBridge_E;
   bridgeCoords[5] = 0;

   rc = MPIX_Torus2rank(bridgeCoords, &bridgerank);
   if(rc != MPI_SUCCESS)
   {
      fprintf(stderr,"Tours2Rank failed for finding bridgenode rank\n");
      ADIOI_BG_assert((rc == MPI_SUCCESS));
   }

   TRACE_ERR("Bridge coords: %d %d %d %d %d, %d. Rank: %d\n", bridgeCoords[0], bridgeCoords[1], bridgeCoords[2], bridgeCoords[3], bridgeCoords[4], bridgeCoords[5], bridgerank);
   /* Some special cases first */
   if(bridgerank == -1 || size == 1 || bridgerank >= commsize)
   {
      if(bridgerank == -1) /* Bridge node not part of job */
      {
         TRACE_ERR("Bridge node not part of job, setting 0 as the bridgenode\n");

         proc->bridgeRank = 0; /* Rank zero will be the bridge node */
         if(rank == 0)
         {
            iambridge = 1;
            proc->iamBridgenode = 1;
         }
         else
         {
            iambridge = 0;
            proc->iamBridgenode = 0;
         }
      }
      else if(bridgerank >= commsize)
      {
         bridgerank = 0;
         if(rank == 0)
         {
            iambridge = 1;
            proc->iamBridgenode = 1;
         }
         else
         {
            iambridge = 0;
            proc->iamBridgenode = 0;
         }
      }
      else if(size == 1)
      {
         iambridge = 1;
         proc->iamBridgenode = 1;
      }

      /* Set up the other parameters */
      proc->myIOSize = size;
      proc->ioNodeIndex = 0;
      conf->ioMinSize = size;
      conf->ioMaxSize = size;
      conf->numBridgeNodes = 1;
      conf->nProcs = size;
      conf->cpuIDsize = hw.ppn;
      conf->virtualPsetSize = conf->ioMaxSize * conf->cpuIDsize;
      conf->nAggrs = 1;
      conf->aggRatio = 1. * conf->nAggrs / conf->virtualPsetSize;
      if(conf->aggRatio > 1) conf->aggRatio = 1.;
   }
   else
   {
      /* Determine how many bridge nodes there are */
      iambridge = (rank == bridgerank)? 1 : 0;

      proc->bridgeRank = bridgerank;

      proc->iamBridgenode = iambridge;

      MPI_Allreduce(&iambridge, &numbridge, 1, MPI_INT, MPI_SUM, comm);

      /* Determine how many compute nodes belong to each bridge node */
      array = (sortstruct *) ADIOI_Malloc(sizeof(sortstruct) * size);
      for(i=0; i<size; i++)
      {
         array[i].bridge = bridgerank;
         array[i].rank = rank;
      }

      /* TODO: At some point, create a communicator of just core 0s to make this
       * faster? */
      MPI_Allgather(MPI_IN_PLACE, 2, MPI_INT, array, 2, MPI_INT, comm);

      proc->myIOSize = count;
            
      bridges = (sortstruct *) ADIOI_Malloc(sizeof(sortstruct) * numbridge);

      /* This should be rank 0 of the communicator */
      if(rank == 0) /* only rank 0 needs to do the sorting/unique counting */
      {
         qsort(array, size, sizeof(sortstruct), intsort);
         int temp, index = 0, mincompute, maxcompute;
         temp = array[0].bridge;
         /* Once the list is sorted walk through it to determine how many
          * compute nodes share a bridge node */
         for(i=0; i<size; i++)
         {
            if(array[i].bridge == temp)
               count++;
            else
            {
               /* number of nodes under this bridge node */
               bridges[index].bridge = count/hw.ppn; 
               /* this particular bridge node rank */
               bridges[index].rank = temp;
               index++;
               temp = array[i].bridge;
               count = 0;
            }
         }
         bridges[index].rank = temp;
         bridges[index].bridge = (count+1)/hw.ppn;
         mincompute = size+1;
         maxcompute = 1;

         for(i=0;i<numbridge;i++)
         {
            if(bridges[i].bridge > maxcompute)
               maxcompute = bridges[i].bridge;
            if(bridges[i].bridge < mincompute)
               mincompute = bridges[i].bridge;
         }
         for(i=0;i<numbridge;i++)
         {
            TRACE_ERR("Bridge node %d (%d) has %d compute nodes\n",
               i, bridges[i].rank, bridges[i].bridge);
         }

         /* Only rank 0 has a conf structure, fill in stuff as appropriate */
         conf->ioMinSize = mincompute;
         conf->ioMaxSize = maxcompute; /* equivalent to pset size */
         conf->numBridgeNodes = numbridge;
         conf->nProcs = size;
         conf->cpuIDsize = hw.ppn;
         conf->virtualPsetSize = maxcompute * conf->cpuIDsize;

         conf->nAggrs = n_aggrs;
         /* First pass gets nAggrs = -1 */
         if(conf->nAggrs <=0 || 
               MIN(conf->nProcs, conf->virtualPsetSize) < conf->nAggrs) 
            conf->nAggrs = ADIOI_BG_NAGG_PSET_DFLT;
         if(conf->nAggrs > conf->numBridgeNodes) // maybe? * conf->cpuIDsize)
            conf->nAggrs = conf->numBridgeNodes; // * conf->cpuIDsize;

         conf->aggRatio = 1. * conf->nAggrs / conf->virtualPsetSize;
         if(conf->aggRatio > 1) conf->aggRatio = 1.;
         TRACE_ERR("Maximum nodes under a bridgenode: %d, minimum: %d, nAggrs: %d, vps: %d, numbridge: %d pset dflt: %d naggrs: %d ratio: %f\n", maxcompute, mincompute, conf->nAggrs, conf->virtualPsetSize, conf->numBridgeNodes, ADIOI_BG_NAGG_PSET_DFLT, conf->nAggrs, conf->aggRatio);

      }

      /* Broadcast the bridges array to everyone else */
      MPI_Bcast(bridges, 2 * numbridge, MPI_INT, 0, comm);
      for(i=0;i<numbridge;i++)
      {
         if(bridges[i].rank == bridgerank)
         {
            proc->myIOSize = bridges[i].bridge;
            proc->ioNodeIndex = i;
         }
      }
      TRACE_ERR("%d has bridge node %d (rank: %d) with %d other nodes, ioNodeIndex: %d\n", rank,  proc->ioNodeIndex, bridgerank, proc->myIOSize, proc->ioNodeIndex);

      ADIOI_Free(array);
      ADIOI_Free(bridges);
   }

}

void 
ADIOI_BG_persInfo_free( ADIOI_BG_ConfInfo_t *conf, ADIOI_BG_ProcInfo_t *proc )
{
    ADIOI_BG_ConfInfo_free( conf );
    ADIOI_BG_ProcInfo_free( proc );
}
