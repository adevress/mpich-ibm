/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pamix.c
 * \brief This file contains routines to make the PAMI API usable for MPI internals. It is less likely that
 *        MPI apps will use these routines.
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



