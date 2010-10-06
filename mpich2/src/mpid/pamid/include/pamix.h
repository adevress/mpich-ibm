/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/pamix.h
 * \brief Extensions to PAMI
 */


#ifndef __include_pamix_h__
#define __include_pamix_h__

#include <pami.h>

#if defined(__cplusplus)
extern "C" {
#endif


void
PAMIX_Init(pami_client_t client);

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

#if defined(__BGQ__) || defined(__BGP__)

typedef struct
{
  size_t  dims;
  size_t *coord;
  size_t *size;
  size_t *torus;
} pamix_torus_info_t;

const pamix_torus_info_t * PAMIX_Torus_info();
void PAMIX_Task2torus(pami_task_t task_id, size_t coords[]);
void PAMIX_Torus2task(size_t coords[], pami_task_t* task_id);

#endif


#if defined(__cplusplus)
}
#endif
#endif
