/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpix.c
 * \brief This file is for MPI extensions that MPI apps are likely to use.
 */

#include <pami.h>

#include <mpid_config.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <mpix.h>

#if ASSERT_LEVEL==0
#define MPIX_assert(x)
#elif ASSERT_LEVEL>=1
#define MPIX_assert(x)  assert(x)
#endif

extern pami_client_t MPIDI_Client;

MPIX_Hardware_t mpid_hw;

/* Determine the number of torus dimensions. Implemented to keep this code
 * architecture-neutral. do we already have this cached somewhere?
 * this call is non-destructive (no asserts), however future calls are
 * destructive so the user needs to verify numdimensions != -1
 */

int MPIX_Get_torus_dims(int *numdimensions)
{
   typedef struct pami_extension_torus_information
   { 
      size_t dims;
      size_t *coord;
      size_t *size;
      size_t *torus;
   } pami_extension_torus_information_t;

   typedef const pami_extension_torus_information_t * (*pami_extension_torus_information_fn) ();

   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   if(rc != PAMI_SUCCESS)
   {
      fprintf(stderr,"No PAMI torus network extension available.\n");
      *numdimensions = -1;
      return rc;
   }
   
   pami_extension_torus_information_fn pamix_torus_info =
      (pami_extension_torus_information_fn) PAMI_Extension_function(extension, "information");
   MPIX_assert(pamix_torus_info != NULL);

   const pami_extension_torus_information_t *info = pamix_torus_info();
   *numdimensions = (int)info->dims;
   rc = PAMI_Extension_close(extension);
   return rc;
}
/* We might need to cache the extension. I suppose it depends on how
 * slow this is, and/or if the rank2torus/torus2rank functions become
 * performance-critical. it could probably be "opened" as part of 
 * MPI_Init() for example.
 */
int MPIX_Rank2torus(int rank, int *coords)
{
   typedef pami_result_t (*pami_extension_torus_task2torus_fn) (pami_task_t, size_t[]);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   size_t *coord_array;
   int i;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   MPIX_assert(rc == PAMI_SUCCESS);

   pami_extension_torus_task2torus_fn pamix_torus_task2torus =
      (pami_extension_torus_task2torus_fn) PAMI_Extension_function(extension,
                                                               "task2torus");
   MPIX_assert(pamix_torus_task2torus != NULL);

   coord_array = malloc(sizeof(size_t) * (mpid_hw.torus_dimension+1));
   MPIX_assert(coord_array != NULL);

   rc = pamix_torus_task2torus((pami_task_t)rank, coord_array);
   MPIX_assert(rc == PAMI_SUCCESS);

   for(i=0;i<mpid_hw.torus_dimension+1;i++)
      coords[i] = (int)coord_array[i];

   rc = PAMI_Extension_close(extension);
   return rc;
}

int MPIX_Torus2rank(int *coords, int *rank)
{
   typedef pami_result_t (*pami_extension_torus_torus2task_fn) (size_t[], pami_task_t *);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   size_t *coord_array;
   int i;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   MPIX_assert(rc == PAMI_SUCCESS);
   coord_array = malloc(sizeof(size_t) * (mpid_hw.torus_dimension+1));
   MPIX_assert(coord_array != NULL);

   pami_extension_torus_torus2task_fn pamix_torus_torus2task =
      (pami_extension_torus_torus2task_fn) PAMI_Extension_function(extension,
                                                               "torus2task");
   MPIX_assert(pamix_torus_torus2task != NULL);
   
   for(i=0;i<mpid_hw.torus_dimension+1;i++)
      coord_array[i] = (size_t)coords[i];

   rc = pamix_torus_torus2task(coord_array, (pami_task_t *)rank);
   MPIX_assert(rc == PAMI_SUCCESS);


   rc = PAMI_Extension_close(extension);
   return rc;
}

int MPIX_Get_hardware(MPIX_Hardware_t *hw)
{
   assert(hw != NULL);
   /* 
    * We've already initialized the hw structure in MPID_Init,
    * so just copy it to the users buffer
    */
   memcpy(hw, &mpid_hw, sizeof(MPIX_Hardware_t));
   return 0;
}


