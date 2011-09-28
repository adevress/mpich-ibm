/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pamix/pamix.c
 * \brief This file contains routines to make the PAMI API usable for MPI internals. It is less likely that
 *        MPI apps will use these routines.
 */

#include <assert.h>
#include <limits.h>
#include <pamix.h>

#include <mpid_config.h>
#define PAMIX_assert_always(x) assert(x)
#if ASSERT_LEVEL==0
#define PAMIX_assert(x)
#elif ASSERT_LEVEL>=1
#define PAMIX_assert(x)        assert(x)
#endif

#define MIN(a,b) ((a<b)?a:b)

#if defined(__BGQ__) || defined(__BGP__)
#define __BG__
#endif

typedef pami_result_t (*pamix_progress_register_fn) (pami_context_t            context,
                                                     pamix_progress_function   progress_fn,
                                                     pamix_progress_function   suspend_fn,
                                                     pamix_progress_function   resume_fn,
                                                     void                     * cookie);
typedef pami_result_t (*pamix_progress_enable_fn) (pami_context_t   context,
                                                   pamix_progress_t event_type);
typedef pami_result_t (*pamix_progress_disable_fn) (pami_context_t   context,
                                                    pamix_progress_t event_type);

#if defined(__BG__)
typedef const pamix_torus_info_t* (*pamix_torus_info_fn) ();
typedef pami_result_t             (*pamix_task2torus_fn) (pami_task_t, size_t[]);
typedef pami_result_t             (*pamix_torus2task_fn) (size_t[], pami_task_t *);
#endif


static struct
{
  pamix_progress_register_fn progress_register;
  pamix_progress_enable_fn   progress_enable;
  pamix_progress_disable_fn  progress_disable;

#if defined(__BG__)
  pamix_torus_info_fn torus_info;
  pamix_task2torus_fn task2torus;
  pamix_torus2task_fn torus2task;
#endif
} PAMIX_Functions = {0};

#if defined(__BGQ__)
uint32_t * PAMIX_BGQ_mapcache = NULL;
#endif

static struct
{
  pami_extension_t progress;

#if defined(__BG__)
  pami_extension_t torus;
#endif
#if defined(__BGQ__)
  pami_extension_t mapping;
#endif
} PAMIX_Extensions;


#define PAMI_EXTENSION_OPEN(client, name, ext)  \
({                                              \
  pami_result_t rc;                             \
  rc = PAMI_Extension_open(client, name, ext);  \
  PAMIX_assert_always(rc == PAMI_SUCCESS);      \
})
#define PAMI_EXTENSION_FUNCTION(type, name, ext)        \
({                                                      \
  void* fn;                                             \
  fn = PAMI_Extension_symbol(ext, name);                \
  PAMIX_assert_always(fn != NULL);                      \
  (type)fn;                                             \
})
void
PAMIX_Initialize(pami_client_t client)
{
#ifndef MPIDI_SINGLE_CONTEXT_ASYNC_PROGRESS
  PAMI_EXTENSION_OPEN(client, "EXT_async_progress", &PAMIX_Extensions.progress);
  PAMIX_Functions.progress_register = PAMI_EXTENSION_FUNCTION(pamix_progress_register_fn, "register", PAMIX_Extensions.progress);
  PAMIX_Functions.progress_enable   = PAMI_EXTENSION_FUNCTION(pamix_progress_enable_fn,   "enable",   PAMIX_Extensions.progress);
  PAMIX_Functions.progress_disable  = PAMI_EXTENSION_FUNCTION(pamix_progress_disable_fn,  "disable",  PAMIX_Extensions.progress);
#endif

#if defined(__BG__)
  PAMI_EXTENSION_OPEN(client, "EXT_torus_network", &PAMIX_Extensions.torus);
  PAMIX_Functions.torus_info = PAMI_EXTENSION_FUNCTION(pamix_torus_info_fn, "information", PAMIX_Extensions.torus);
  PAMIX_Functions.task2torus = PAMI_EXTENSION_FUNCTION(pamix_task2torus_fn, "task2torus",  PAMIX_Extensions.torus);
  PAMIX_Functions.torus2task = PAMI_EXTENSION_FUNCTION(pamix_torus2task_fn, "torus2task",  PAMIX_Extensions.torus);
#endif

#if defined(__BGQ__)
  PAMI_EXTENSION_OPEN(client, "BGQ_bgq_mapping", &PAMIX_Extensions.mapping);
  PAMIX_BGQ_mapcache = PAMI_EXTENSION_FUNCTION(uint32_t *, "mapcache", PAMIX_Extensions.mapping);
#endif
}


void
PAMIX_Finalize(pami_client_t client)
{
  pami_result_t rc;

#ifndef __PE__
  rc = PAMI_Extension_close(PAMIX_Extensions.progress);
  PAMIX_assert_always(rc == PAMI_SUCCESS);
#endif

#if defined(__BG__)
  rc = PAMI_Extension_close(PAMIX_Extensions.torus);
  PAMIX_assert_always(rc == PAMI_SUCCESS);
#endif

#if defined(__BGQ__)
  rc = PAMI_Extension_close(PAMIX_Extensions.mapping);
  PAMIX_assert_always(rc == PAMI_SUCCESS);
  PAMIX_BGQ_mapcache = NULL;
#endif
}


pami_configuration_t
PAMIX_Client_query(pami_client_t         client,
                   pami_attribute_name_t name)
{
  pami_result_t rc;
  pami_configuration_t query;
  query.name = name;
  rc = PAMI_Client_query(client, &query, 1);
  PAMIX_assert_always(rc == PAMI_SUCCESS);
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
  PAMIX_assert_always(rc == PAMI_SUCCESS);
  return query;
}


void
PAMIX_Dispatch_set(pami_context_t                  context[],
                   size_t                          num_contexts,
                   size_t                          dispatch,
                   pami_dispatch_callback_function fn,
                   pami_dispatch_hint_t            options,
                   size_t                        * immediate_max)
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
    PAMIX_assert_always(rc == PAMI_SUCCESS);

    size_t size;
    size = PAMIX_Dispatch_query(context[i], dispatch, PAMI_DISPATCH_SEND_IMMEDIATE_MAX).value.intval;
    last_immediate_max = MIN(size, last_immediate_max);
    size = PAMIX_Dispatch_query(context[i], dispatch, PAMI_DISPATCH_RECV_IMMEDIATE_MAX).value.intval;
    last_immediate_max = MIN(size, last_immediate_max);
  }

  if (immediate_max != NULL)
    *immediate_max = last_immediate_max;
}


pami_task_t
PAMIX_Endpoint_query(pami_endpoint_t endpoint)
{
  pami_task_t rank;
  size_t      offset;

  pami_result_t rc;
  rc = PAMI_Endpoint_query(endpoint, &rank, &offset);
  PAMIX_assert(rc == PAMI_SUCCESS);

  return rank;
}


void
PAMIX_Progress_register(pami_context_t            context,
                        pamix_progress_function   progress_fn,
                        pamix_progress_function   suspend_fn,
                        pamix_progress_function   resume_fn,
                        void                    * cookie)
{
  PAMIX_assert_always(PAMIX_Functions.progress_register != NULL);
  pami_result_t rc;
  rc = PAMIX_Functions.progress_register(context, progress_fn,suspend_fn, resume_fn, cookie);
  PAMIX_assert_always(rc == PAMI_SUCCESS);
}


void
PAMIX_Progress_enable(pami_context_t   context,
                      pamix_progress_t event_type)
{
  PAMIX_assert(PAMIX_Functions.progress_enable != NULL);
  pami_result_t rc;
  rc = PAMIX_Functions.progress_enable(context, event_type);
  PAMIX_assert(rc == PAMI_SUCCESS);
}


void
PAMIX_Progress_disable(pami_context_t   context,
                       pamix_progress_t event_type)
{
  PAMIX_assert(PAMIX_Functions.progress_disable != NULL);
  pami_result_t rc;
  rc = PAMIX_Functions.progress_disable(context, event_type);
  PAMIX_assert(rc == PAMI_SUCCESS);
}


#if defined(__BG__)

const pamix_torus_info_t *
PAMIX_Torus_info()
{
  PAMIX_assert(PAMIX_Functions.torus_info != NULL);
  return PAMIX_Functions.torus_info();
}


int
PAMIX_Task2torus(pami_task_t task_id,
                 size_t      coords[])
{
  PAMIX_assert(PAMIX_Functions.task2torus != NULL);
  pami_result_t rc;
  rc = PAMIX_Functions.task2torus(task_id, coords);
  return rc;
}


#include <stdio.h>
int
PAMIX_Torus2task(size_t        coords[],
                 pami_task_t * task_id)
{
  PAMIX_assert(PAMIX_Functions.torus2task != NULL);
  pami_result_t rc;
  rc = PAMIX_Functions.torus2task(coords, task_id);
  return rc;
#if 0
  if(rc != PAMI_SUCCESS)
  {
   fprintf(stderr,"coords in: %zu %zu %zu %zu %zu, rc: %d\n", coords[0], coords[1], coords[2],coords[3],coords[4], rc);
     PAMIX_assert(rc == PAMI_SUCCESS);
   }
   else return rc;
#endif
}

#endif
