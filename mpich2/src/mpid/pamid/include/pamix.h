/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/pamix.h
 * \brief Extensions to PAMI
 */


#ifndef __include_pamix_h__
#define __include_pamix_h__

#include <pami.h>

pami_configuration_t
PAMIX_Client_query(pami_client_t         client,
                   pami_attribute_name_t name);

void
PAMIX_Dispatch_set(pami_context_t              context[],
                   size_t                      num_contexts,
                   size_t                      dispatch,
                   pami_dispatch_callback_fn   fn,
                   pami_send_hint_t            options,
                   size_t                    * immediate_max);
#endif
