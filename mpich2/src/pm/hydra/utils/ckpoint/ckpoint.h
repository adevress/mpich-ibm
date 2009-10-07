/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef CKPOINT_H_INCLUDED
#define CKPOINT_H_INCLUDED

#include "hydra_utils.h"

struct HYDU_ckpoint_info {
    char *ckpointlib;
    char *ckpoint_prefix;
};

extern struct HYDU_ckpoint_info HYDU_ckpoint_info;

#endif /* CKPOINT_H_INCLUDED */
