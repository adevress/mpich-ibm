/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/xmix.c
 * \brief ???
 */

#include <xmi.h>

#include <mpid_config.h>
#include <assert.h>
#if ASSERT_LEVEL==0
#define XMIX_assert(x)
#elif ASSERT_LEVEL>=1
#define XMIX_assert(x)       assert(x)
#endif


xmi_configuration_t
XMIX_Configuration_query(xmi_client_t client,
                         xmi_attribute_name_t name)
{
  xmi_result_t rc;
  xmi_configuration_t query;
  query.name = name;
  rc = XMI_Configuration_query(client, &query);
  XMIX_assert(rc == XMI_SUCCESS);
  return query;
}


void
XMIX_Dispatch_set(xmi_context_t              context[],
                  size_t                     num_contexts,
                  size_t                     dispatch,
                  xmi_dispatch_callback_fn   fn,
                  xmi_send_hint_t            options)
{
  xmi_result_t rc;
  size_t i;
  for (i=0; i<num_contexts; ++i) {
    rc = XMI_Dispatch_set(context[i],
                          dispatch,
                          fn,
                          (void*)i,
                          options);
    XMIX_assert(rc == XMI_SUCCESS);
  }
}


void
XMIX_Context_destroy(xmi_context_t* contexts,
                     size_t num_contexts)
{
  xmi_result_t rc;
  size_t i;
  for (i=0; i<num_contexts; ++i) {
    rc = XMI_Context_destroy(contexts[i]);
    XMIX_assert(rc == XMI_SUCCESS);
  }
}
