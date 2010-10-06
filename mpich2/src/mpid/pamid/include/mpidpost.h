/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidpost.h
 * \brief The trailing device header
 *
 * This file is included after the rest of the headers
 * (mpidimpl.h, mpidpre.h, and mpiimpl.h)
 */

#ifndef __include_mpidpost_h__
#define __include_mpidpost_h__

#include <mpid_datatype.h>
#include "mpidi_prototypes.h"
#include "mpidi_macros.h"

#include "../src/mpid_progress.h"
#include "../src/mpid_request.h"
#include "../src/pt2pt/mpid_isend.h"
#include "../src/pt2pt/mpid_send.h"
#include "../src/pt2pt/mpid_irecv.h"

#endif
