/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidpre.h
 * \brief Extensions to XMI
 */


#ifndef XMIX_H
#define XMIX_H

#include <xmi.h>

xmi_configuration_t
XMIX_Configuration_query(xmi_client_t client,
                         xmi_attribute_name_t name);

void
XMIX_Dispatch_set(xmi_context_t              context[],
                  size_t                     num_contexts,
                  size_t                     dispatch,
                  xmi_dispatch_callback_fn   fn,
                  xmi_send_hint_t            options);
#endif
