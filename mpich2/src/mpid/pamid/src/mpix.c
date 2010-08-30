/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpix.c
 * \brief This file is for MPI extensions that MPI apps are likely to use.
 */

#include <pami.h>

#include <mpid_config.h>
#include <assert.h>
#include <stdio.h>

#if ASSERT_LEVEL==0
#define MPIX_assert(x)
#elif ASSERT_LEVEL>=1
#define MPIX_assert(x)  assert(x)
#endif

extern pami_client_t MPIDI_Client;


/* Determine the number of torus dimensions. Implemented to keep this code
 * architecture-neutral. do we already have this cached somewhere?
 * this call is non-destructive (no asserts), however future calls are
 * destructive so the user needs to verify numdimensions != -1
 */

int MPIX_Get_torus_dims(size_t *numdimensions)
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
   *numdimensions = info->dims;
   rc = PAMI_Extension_close(extension);
   return rc;
}
/* We might need to cache the extension. I suppose it depends on how
 * slow this is, and/or if the rank2torus/torus2rank functions become
 * performance-critical. it could probably be "opened" as part of 
 * MPI_Init() for example.
 */
int MPIX_Rank2torus(int rank, size_t *coords)
{
   typedef pami_result_t (*pami_extension_torus_task2torus_fn) (pami_task_t, size_t[]);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   MPIX_assert(rc == PAMI_SUCCESS);

   pami_extension_torus_task2torus_fn pamix_torus_task2torus =
      (pami_extension_torus_task2torus_fn) PAMI_Extension_function(extension,
                                                               "task2torus");
   MPIX_assert(pamix_torus_task2torus != NULL);

   rc = pamix_torus_task2torus((pami_task_t)rank, coords);
   MPIX_assert(rc == PAMI_SUCCESS);

   rc = PAMI_Extension_close(extension);
   return rc;
}

int MPIX_Torus2rank(size_t *coords, int *rank)
{
   typedef pami_result_t (*pami_extension_torus_torus2task_fn) (size_t[], pami_task_t *);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   MPIX_assert(rc == PAMI_SUCCESS);

   pami_extension_torus_torus2task_fn pamix_torus_torus2task =
      (pami_extension_torus_torus2task_fn) PAMI_Extension_function(extension,
                                                               "torus2task");
   MPIX_assert(pamix_torus_torus2task != NULL);
   
   rc = pamix_torus_torus2task(coords, (pami_task_t *)rank);
   MPIX_assert(rc == PAMI_SUCCESS);

   rc = PAMI_Extension_close(extension);
   return rc;
}


