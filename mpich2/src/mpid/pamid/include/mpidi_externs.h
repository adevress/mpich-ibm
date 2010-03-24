/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpid_externs.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpid_externs_h__
#define __include_mpid_externs_h__


extern size_t        NUM_CONTEXTS;
extern pami_client_t  MPIDI_Client;
extern pami_context_t MPIDI_Context[];

extern MPIDI_Process_t MPIDI_Process;
extern MPIDI_Protocol_t MPIDI_Protocols;
extern MPIDI_CollectiveProtocol_t MPIDI_CollectiveProtocols;


#endif
