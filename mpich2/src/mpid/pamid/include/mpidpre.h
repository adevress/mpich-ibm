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
#include <unistd.h>

#include "mpid_dataloop.h"
#include <pami.h>

/* provides "pre" typedefs and such for NBC scheduling mechanism */
#include "mpid_sched_pre.h"

/** \brief Creates a compile error if the condition is false. */
#define MPID_assert_static(expr) ({ switch(0){case 0:case expr:;} })
#define MPID_assert_always(x) assert(x) /**< \brief Tests for likely problems--always active */
#define MPID_abort()          assert(0) /**< \brief Always exit--usually implies missing functionality */
#if ASSERT_LEVEL==0
#define MPID_assert(x)
#else
#define MPID_assert(x)        assert(x) /**< \brief Tests for likely problems--may not be active in performance code */
#endif

/* --BEGIN ERROR MACROS-- */
#ifdef MPIDI_NO_ASSERT
#define MPIU_ERR_CHKORASSERT(cond_,err_,class_,stmt_,msg_)              \
    if(!(cond_)) {                                                \
        MPIU_ERR_SETANDSTMT(err_,class_, stmt_, msg_); \
    }
#define MPIU_ERR_CHKORASSERT1(cond_, err_, class_, stmt_, gmsg_, smsg_, arg1_)         \
    if(!(cond_)) {                                                              \
        MPIU_ERR_SETANDSTMT1(err_,class_,stmt_, gmsg_,smsg_,arg1_);  \
    }
#elif  ASSERT_LEVEL==0
#define MPIU_ERR_CHKORASSERT(cond_,err_,class_,stmt_,msg)
#define MPIU_ERR_CHKORASSERT1(cond_, err_, class_,stmt_,  gmsg_, smsg_, arg1_)
#else
#define PRINT_ERROR_MSG(err_)   {                                     \
    if (err_ != MPI_SUCCESS) {                                        \
        int len_;                                                     \
        char err_str_[MPI_MAX_ERROR_STRING];                          \
        MPI_Error_string(err_, err_str_, &len_);                      \
        fprintf(stderr, __FILE__ " failed at line %d, err=%d: %s\n",  \
                __LINE__, err_, err_str_);                            \
        fflush(stderr);                                               \
    }                                                                 \
}
#define MPIU_ERR_CHKORASSERT(cond_,err_,class_,stmt_, msg_) { \
    if(!(cond_)) {                                          \
        MPIU_ERR_SET(err_,class_,msg_);                     \
        PRINT_ERROR_MSG(err_);                              \
        stmt_;                                              \
    }                                                       \
}
#define MPIU_ERR_CHKORASSERT1(cond_, err_, class_,stmt_,gmsg_, smsg_, arg1_) {   \
    if(!(cond_)) {                                          \
        MPIU_ERR_SET1(err_, class_, gmsg_, smsg_, arg1_);                     \
        PRINT_ERROR_MSG(err_);                              \
        stmt_;                                              \
    }                                                       \
}
#endif
/* --END ERROR MACROS-- */

#include "mpidi_platform.h"

#include "mpidi_constants.h"
#include "mpidi_datatypes.h"
#include "mpidi_externs.h"
#include "mpidi_hooks.h"
#include "mpidi_thread.h"
#include "mpidi_util.h"
#ifdef MPIDI_TRACE
#include "mpidi_trace.h"
#endif

#endif
