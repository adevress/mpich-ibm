/*  (C)Copyright IBM Corp.  2007, 2011  */
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

#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include <mpid_config.h>
#include <mpid_dataloop.h>
#include <pami.h>


/** \brief Creates a compile error if the condition is false. */
#define MPID_assert_static(expr) ({ switch(0){case 0:case expr:;} })
#define MPID_assert_always(x) assert(x) /**< \brief Tests for likely problems--always active */
#define MPID_abort()          assert(0) /**< \brief Always exit--usually implies missing functionality */
#if ASSERT_LEVEL==0
#define MPID_assert(x)
#else
#define MPID_assert(x)        assert(x) /**< \brief Tests for likely problems--may not be active in performance code */
#endif


#include "mpidi_platform.h"

#include "mpidi_constants.h"
#include "mpidi_datatypes.h"
#include "mpidi_externs.h"
#include "mpidi_hooks.h"
#include "mpidi_thread.h"
#include "mpidi_util.h"

#endif
