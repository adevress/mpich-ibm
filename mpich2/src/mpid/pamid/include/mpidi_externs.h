/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_externs.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_externs_h__
#define __include_mpidi_externs_h__


extern pami_client_t  MPIDI_Client;
extern pami_context_t MPIDI_Context[];

extern MPIDI_Process_t MPIDI_Process;
extern int MPIX_Statistics_write (FILE *);
extern int MPIX_Statistics_zero ();


#endif
