/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2011 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/* This file contains glue code that ROMIO needs to call/use in order to access
 * certain internals of MPICH2.  This allows us stop using "mpiimpl.h" (and all
 * the headers it includes) directly inside of ROMIO. */

#include "mpiimpl.h"
#include "glue_romio.h"

int MPIR_Ext_dbg_romio_terse_enabled = 0;
int MPIR_Ext_dbg_romio_typical_enabled = 0;
int MPIR_Ext_dbg_romio_verbose_enabled = 0;

/* to be called early by ROMIO's initialization process in order to setup init-time
 * glue code that cannot be initialized statically */
int MPIR_Ext_init(void)
{
    if (MPIU_DBG_SELECTED(ROMIO,TERSE))
        MPIR_Ext_dbg_romio_terse_enabled = 1;
    if (MPIU_DBG_SELECTED(ROMIO,TYPICAL))
        MPIR_Ext_dbg_romio_typical_enabled = 1;
    if (MPIU_DBG_SELECTED(ROMIO,VERBOSE))
        MPIR_Ext_dbg_romio_verbose_enabled = 1;

    return MPI_SUCCESS;
}


int MPIR_Ext_assert_fail(const char *cond, const char *file_name, int line_num)
{
    return MPIR_Assert_fail(cond, file_name, line_num);
}

/* These two routines export the ALLFUNC CS_ENTER/EXIT macros as functions so
 * that ROMIO can use them.  These routines only support the GLOBAL granularity
 * of MPICH2 threading; other accommodations must be made for finer-grained
 * threading strategies. */
void MPIR_Ext_cs_enter_allfunc(void)
{
    MPIU_THREAD_CS_ENTER(ALLFUNC,);
}

void MPIR_Ext_cs_exit_allfunc(void)
{
    MPIU_THREAD_CS_EXIT(ALLFUNC,);
}

