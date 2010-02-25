#include <xmi.h>


#include <mpid_config.h>
#include <assert.h>
#if ASSERT_LEVEL==0
#define XMIX_abort()         assert(0)
#define XMIX_assert(x)
#define XMIX_assert_debug(x)
#elif ASSERT_LEVEL==1
#define XMIX_abort()         assert(0)
#define XMIX_assert(x)       assert(x)
#define XMIX_assert_debug(x)
#else /* ASSERT_LEVEL==2 */
/** \brief Always exit--usually implies missing functionality */
#define XMIX_abort()         assert(0)
/** \brief Tests for likely problems--may not be active in performance code  */
#define XMIX_assert(x)       assert(x)
/** \brief Tests for rare problems--may not be active in production code */
#define XMIX_assert_debug(x) assert(x)
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
