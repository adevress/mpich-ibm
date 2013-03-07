/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_macros.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_macros_h__
#define __include_mpidi_macros_h__

#include "mpidi_datatypes.h"
#include "mpidi_externs.h"

#define TOKEN_FLOW_CONTROL_ON (TOKEN_FLOW_CONTROL && MPIU_Token_on())

#ifdef TRACE_ON
#ifdef __GNUC__
#define TRACE_ALL(fd, format, ...) fprintf(fd, "%s:%u (%d) " format, __FILE__, __LINE__, MPIR_Process.comm_world->rank, ##__VA_ARGS__)
#define TRACE_OUT(format, ...) TRACE_ALL(stdout, format, ##__VA_ARGS__)
#define TRACE_ERR(format, ...) TRACE_ALL(stderr, format, ##__VA_ARGS__)
#else
#define TRACE_OUT(format...) fprintf(stdout, format)
#define TRACE_ERR(format...) fprintf(stderr, format)
#endif
#else
#define TRACE_OUT(format...)
#define TRACE_ERR(format...)
#endif

#if TOKEN_FLOW_CONTROL
#define MPIU_Token_on() (MPIDI_Process.is_token_flow_control_on)
#else
#define MPIU_Token_on() (0)
#endif

/**
 * \brief Gets significant info regarding the datatype
 * Used in mpid_send, mpidi_send.
 */
#define MPIDI_Datatype_get_info(_count, _datatype,              \
_dt_contig_out, _data_sz_out, _dt_ptr, _dt_true_lb)             \
({                                                              \
  if (HANDLE_GET_KIND(_datatype) == HANDLE_KIND_BUILTIN)        \
    {                                                           \
        (_dt_ptr)        = NULL;                                \
        (_dt_contig_out) = TRUE;                                \
        (_dt_true_lb)    = 0;                                   \
        (_data_sz_out)   = (_count) *                           \
        MPID_Datatype_get_basic_size(_datatype);                \
    }                                                           \
  else                                                          \
    {                                                           \
        MPID_Datatype_get_ptr((_datatype), (_dt_ptr));          \
        (_dt_contig_out) = (_dt_ptr)->is_contig;                \
        (_dt_true_lb)    = (_dt_ptr)->true_lb;                  \
        (_data_sz_out)   = (_count) * (_dt_ptr)->size;          \
    }                                                           \
})

/* Add some error checking for size eventually */
#define MPIDI_Update_last_algorithm(_comm, _name) \
({ (_comm)->mpid.last_algorithm=_name; })


/**
 * \brief Macro for allocating memory
 *
 * \param[in] count Number of elements to allocate
 * \param[in] type  The type of the memory, excluding "*"
 * \return Address or NULL
 */
#define MPIU_Calloc0(count, type)               \
({                                              \
  size_t __size = (count) * sizeof(type);       \
  type* __p = MPIU_Malloc(__size);              \
  MPID_assert(__p != NULL);                     \
  if (__p != NULL)                              \
    memset(__p, 0, __size);                     \
  __p;                                          \
})

#define MPIU_TestFree(p)                        \
({                                              \
  if (*(p) != NULL)                             \
    {                                           \
      MPIU_Free(*(p));                          \
      *(p) = NULL;                              \
    }                                           \
})


#define MPID_VCR_GET_LPID(vcr, index)           \
({                                              \
  vcr[index];                                   \
})

#define MPID_GPID_Get(comm_ptr, rank, gpid)             \
({                                                      \
  gpid[1] = MPID_VCR_GET_LPID(comm_ptr->vcr, rank);     \
  gpid[0] = 0;                                          \
  MPI_SUCCESS; /* return success from macro */          \
})


static inline void
MPIDI_Context_post(pami_context_t       context,
                   pami_work_t        * work,
                   pami_work_function   fn,
                   void               * cookie)
{
#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
  if (likely(MPIDI_Process.perobj.context_post.active > 0))
    {
      pami_result_t rc;
      rc = PAMI_Context_post(context, work, fn, cookie);
      MPID_assert(rc == PAMI_SUCCESS);
    }
  else
    {
      /* Lock access to the context. For more information see discussion of the
       * simplifying assumption that the "per object" mpich lock mode does not
       * expect a completely single threaded run environment in the file
       * src/mpid/pamid/src/mpid_progress.h
       */
       PAMI_Context_lock(context);
       fn(context, cookie);
       PAMI_Context_unlock(context);
    }
#else /* (MPIU_THREAD_GRANULARITY != MPIU_THREAD_GRANULARITY_PER_OBJECT) */
  /*
   * It is not necessary to lock the context before access in the "global"
   * mpich lock mode because all threads, application and async progress,
   * must first acquire the global mpich lock upon entry into the library.
   */
  fn(context, cookie);
#endif
}

#if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
#define MPIDI_Send_post(__func, __req)                          \
({                                                              \
  pami_context_t context = MPIDI_Context_local(__req);          \
                                                                \
  if (likely(MPIDI_Process.perobj.context_post.active > 0))     \
    {                                                           \
      pami_result_t rc;                                         \
      rc = PAMI_Context_post(context,                           \
                             &(__req)->mpid.post_request,       \
                             __func,                            \
                             __req);                            \
      MPID_assert(rc == PAMI_SUCCESS);                          \
    }                                                           \
  else                                                          \
    {                                                           \
      PAMI_Context_lock(context);                               \
      __func(context, __req);                                   \
      PAMI_Context_unlock(context);                             \
    }                                                           \
})
#else /* (MPIU_THREAD_GRANULARITY != MPIU_THREAD_GRANULARITY_PER_OBJECT) */
#define MPIDI_Send_post(__func, __req)                          \
({                                                              \
  __func(MPIDI_Context[0], __req);                              \
})
#endif /* #if (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT) */

#endif
