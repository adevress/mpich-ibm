/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pamix.c
 * \brief ???
 */

#include <pami.h>

#include <mpid_config.h>
#include <assert.h>
#if ASSERT_LEVEL==0
#define PAMIX_assert(x)
#elif ASSERT_LEVEL>=1
#define PAMIX_assert(x)  assert(x)
#endif


pami_configuration_t
PAMIX_Configuration_query(pami_client_t client,
                         pami_attribute_name_t name)
{
  pami_result_t rc;
  pami_configuration_t query;
  query.name = name;
  rc = PAMI_Configuration_query(client, &query);
  PAMIX_assert(rc == PAMI_SUCCESS);
  return query;
}


void
PAMIX_Dispatch_set(pami_context_t              context[],
                  size_t                     num_contexts,
                  size_t                     dispatch,
                  pami_dispatch_callback_fn   fn,
                  pami_send_hint_t            options)
{
  pami_result_t rc;
  size_t i;
  for (i=0; i<num_contexts; ++i) {
    rc = PAMI_Dispatch_set(context[i],
                          dispatch,
                          fn,
                          (void*)i,
                          options);
    PAMIX_assert(rc == PAMI_SUCCESS);
  }
}


void
PAMIX_Context_destroy(pami_context_t* contexts,
                     size_t num_contexts)
{
  pami_result_t rc;
  size_t i;
  for (i=0; i<num_contexts; ++i) {
    rc = PAMI_Context_destroy(contexts[i]);
    PAMIX_assert(rc == PAMI_SUCCESS);
  }
}
