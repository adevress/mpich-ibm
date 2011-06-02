/*  (C)Copyright IBM Corp.  2007, 2011  */
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
PAMIX_Initialize(pami_client_t client);

void
PAMIX_Finalize(pami_client_t client);

pami_configuration_t
PAMIX_Client_query(pami_client_t         client,
                   pami_attribute_name_t name);

void
PAMIX_Dispatch_set(pami_context_t                  context[],
                   size_t                          num_contexts,
                   size_t                          dispatch,
                   pami_dispatch_callback_function fn,
                   pami_dispatch_hint_t            options,
                   size_t                        * immediate_max);

pami_task_t
PAMIX_Endpoint_query(pami_endpoint_t endpoint);


typedef void (*pamix_progress_function) (pami_context_t context, void *cookie);
#define PAMIX_CLIENT_ASYNC_GUARANTEE 1016
typedef enum
{
  PAMIX_PROGRESS_ALL =    0,
  PAMIX_PROGRESS_EXT = 1000
} pamix_progress_t;

void
PAMIX_progress_register(pami_context_t            context,
                        pamix_progress_function   progress_fn,
                        pamix_progress_function   suspend_fn,
                        pamix_progress_function   resume_fn,
                        void                    * cookie);
void
PAMIX_progress_enable(pami_context_t   context,
                      pamix_progress_t event_type);

void
PAMIX_progress_disable(pami_context_t   context,
                       pamix_progress_t event_type);


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
