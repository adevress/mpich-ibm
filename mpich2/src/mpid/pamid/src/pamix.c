/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pamix.c
 * \brief ???
 */

#include <pami.h>

#include <mpid_config.h>
#include <assert.h>
#include <limits.h>

#include <stdio.h>
#if ASSERT_LEVEL==0
#define PAMIX_assert(x)
#elif ASSERT_LEVEL>=1
#define PAMIX_assert(x)  assert(x)
#endif

#define MIN(a,b) ((a<b)?a:b)
extern pami_client_t MPIDI_Client;

pami_configuration_t
PAMIX_Client_query(pami_client_t         client,
                   pami_attribute_name_t name)
{
  pami_result_t rc;
  pami_configuration_t query;
  query.name = name;
  rc = PAMI_Client_query(client, &query, 1);
  PAMIX_assert(rc == PAMI_SUCCESS);
  return query;
}


static inline pami_configuration_t
PAMIX_Dispatch_query(pami_context_t        context,
                     size_t                dispatch,
                     pami_attribute_name_t name)
{
  pami_result_t rc;
  pami_configuration_t query;
  query.name = name;
  rc = PAMI_Dispatch_query(context, dispatch, &query, 1);
  PAMIX_assert(rc == PAMI_SUCCESS);
  return query;
}


void
PAMIX_Dispatch_set(pami_context_t              context[],
                   size_t                      num_contexts,
                   size_t                      dispatch,
                   pami_dispatch_callback_fn   fn,
                   pami_send_hint_t            options,
                   size_t                    * immediate_max)
{
  pami_result_t rc;
  size_t i;
  size_t last_immediate_max = (size_t)-1;
  for (i=0; i<num_contexts; ++i) {
    rc = PAMI_Dispatch_set(context[i],
                           dispatch,
                           fn,
                           (void*)i,
                           options);
    PAMIX_assert(rc == PAMI_SUCCESS);

    size_t size;
    size = PAMIX_Dispatch_query(context[i], dispatch, PAMI_DISPATCH_SEND_IMMEDIATE_MAX).value.intval;
    last_immediate_max = MIN(size, last_immediate_max);
    size = PAMIX_Dispatch_query(context[i], dispatch, PAMI_DISPATCH_RECV_IMMEDIATE_MAX).value.intval;
    last_immediate_max = MIN(size, last_immediate_max);
  }

  if (immediate_max != NULL)
    *immediate_max = last_immediate_max;
}


/* Determine the number of torus dimensions. Implemented to keep this code
 * architecture-neutral. do we already have this cached somewhere?
 * this call is non-destructive (no asserts), however future calls are
 * destructive so the user needs to verify numdimensions != -1
 */

int PAMIX_Get_torus_dims(size_t *numdimensions)
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
   PAMIX_assert(pamix_torus_info != NULL);

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
int PAMIX_Rank2torus(int rank, size_t *coords)
{
   typedef pami_result_t (*pami_extension_torus_task2torus_fn) (pami_task_t, size_t[]);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   PAMIX_assert(rc == PAMI_SUCCESS);

   pami_extension_torus_task2torus_fn pamix_torus_task2torus =
      (pami_extension_torus_task2torus_fn) PAMI_Extension_function(extension,
                                                               "task2torus");
   PAMIX_assert(pamix_torus_task2torus != NULL);

   rc = pamix_torus_task2torus((pami_task_t)rank, coords);
   PAMIX_assert(rc == PAMI_SUCCESS);

   rc = PAMI_Extension_close(extension);
   return rc;
}

int PAMIX_Torus2rank(size_t *coords, int *rank)
{
   typedef pami_result_t (*pami_extension_torus_torus2task_fn) (size_t[], pami_task_t *);
   pami_result_t rc = PAMI_SUCCESS;
   pami_extension_t extension;
   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);
   PAMIX_assert(rc == PAMI_SUCCESS);

   pami_extension_torus_torus2task_fn pamix_torus_torus2task =
      (pami_extension_torus_torus2task_fn) PAMI_Extension_function(extension,
                                                               "torus2task");
   PAMIX_assert(pamix_torus_torus2task != NULL);
   
   rc = pamix_torus_torus2task(coords, (pami_task_t *)rank);
   PAMIX_assert(rc == PAMI_SUCCESS);

   rc = PAMI_Extension_close(extension);
   return rc;
}



