/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidpre.h
 * \brief The leading device header
 *
 * This file is included at the start of the other headers
 * (mpidimpl.h, mpidpost.h, and mpiimpl.h).  It generally contains
 * additions to MPI objects.
 */

#ifndef __include_mpidpre_h__
#define __include_mpidpre_h__

#include <mpid_config.h>
#include <mpid_dataloop.h>
#include <assert.h>
#include <pami.h>


#if defined(__BGQ__) || defined(__BGP__)
#define USE_PAMI_RDMA 1
#endif
#if defined(__BGQ__) && (MPIU_THREAD_GRANULARITY == MPIU_THREAD_GRANULARITY_PER_OBJECT)
#define USE_PAMI_COMM_THREADS 1
#endif


#define MPID_abort()    assert(0) /**< \brief Always exit--usually implies missing functionality */
#if ASSERT_LEVEL==0
#define MPID_assert(x)
#else
#define MPID_assert(x)  assert(x) /**< \brief Tests for likely problems--may not be active in performance code  */
#endif

/** \brief Creates a compile error if the condition is false. */
#define MPID_assert_static(expr) ({ switch(0){case 0:case expr:;} })


#include "mpidi_thread.h"
#include "mpidi_constants.h"
#include "mpidi_datatypes.h"
#include "mpidi_externs.h"
#include "mpidi_hooks.h"


#endif
