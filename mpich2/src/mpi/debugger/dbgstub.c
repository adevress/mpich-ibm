/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*  
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
#include "mpidimpl.h"
/* These are pointers to the static variables in src/mpid/ch3/src/ch3u_recvq.c
   that contains the *addresses* of the posted and unexpected queue head 
   pointers */
extern MPID_Request ** const MPID_Recvq_posted_head_ptr, 
    ** const MPID_Recvq_unexpected_head_ptr;

#include "mpi_interface.h"

/* This is from dbginit.c; it is not exported to other files */
typedef struct MPIR_Comm_list {
	int sequence_number;   
	MPID_Comm *head;
} MPIR_Comm_list;
extern MPIR_Comm_list MPIR_All_communicators;
extern MPID_Request *MPIR_Sendq_head;

/* 
   This file contains emulation routines for the methods and functions normally
   provided by the debugger.  This file is only used for testing the 
   dll_mpich2.c debugger interface.
 */

/* These are mock-ups of the find type and find offset routines.  Since 
   there are no portable routines to access the symbol table in an image,
   we hand-code the specific types that we use in this code. 
   These routines (more precisely, the field_offset routine) need to 
   known the layout of the internal data structures */
enum { TYPE_UNKNOWN = 0, 
       TYPE_MPID_COMM = 1, 
       TYPE_MPIR_COMM_LIST = 2, 
       TYPE_MPIDI_REQUEST = 3, 		// NOT USED
       TYPE_MPIDI_MESSAGE_MATCH = 4,	// NOT USED
       TYPE_MPID_REQUEST = 5, 
       TYPE_MPIR_SENDQ = 6,		// NOT USED
} KnownTypes;

/* The dll_mpich2.c has a few places where it doesn't always use the most 
   recent type, so a static current type will not work.  Instead, we
   have an example of each type, and return that value. */

static int knownTypesArray[] = { TYPE_UNKNOWN, TYPE_MPID_COMM, 
				 TYPE_MPIR_COMM_LIST, TYPE_MPIDI_REQUEST, 
				 TYPE_MPIDI_MESSAGE_MATCH, TYPE_MPID_REQUEST, 
				 TYPE_MPIR_SENDQ };

mqs_type * dbgrI_find_type(mqs_image *image, char *name, 
			   mqs_lang_code lang)
{
    int curType = TYPE_UNKNOWN;

    if (strcmp(name,"MPID_Comm") == 0) {
	curType = TYPE_MPID_COMM;
    }
    else if (strcmp( name, "MPIR_Comm_list" ) == 0) {
	curType = TYPE_MPIR_COMM_LIST;
    }
    else if (strcmp( name, "MPID_Request" ) == 0) {
	curType = TYPE_MPID_REQUEST;
    }
    else {
	curType = TYPE_UNKNOWN;
    }
    return (mqs_type *)&knownTypesArray[curType];
}
#define OFFSET_OF(type,field)		((unsigned)&((type *)0)->field)
#define OFFSET_OF_MACRO(type,macro)	((unsigned)&macro((type *)0))
int dbgrI_field_offset(mqs_type *type, char *name)
{
    int off = -1;

    int curType = *(int*)type;

    /* printf ( "curtype is %d\n", curType ); */

    switch (curType) {
    case TYPE_MPID_COMM:
	{
	    if (strcmp( name, "name" ) == 0) {
		off = OFFSET_OF(MPID_Comm, name);
	    } else if (strcmp( name, "comm_next" ) == 0) {
		off = OFFSET_OF(MPID_Comm, comm_next);
	    } else if (strcmp( name, "remote_size" ) == 0) {
		off = OFFSET_OF(MPID_Comm, remote_size);
	    } else if (strcmp( name, "rank" ) == 0) {
		off = OFFSET_OF(MPID_Comm, rank);
	    } else if (strcmp( name, "context_id" ) == 0) {
		off = OFFSET_OF(MPID_Comm, context_id);
	    }
	    else if (strcmp( name, "recvcontext_id" ) == 0) {
		off = ((char*)&c.recvcontext_id - (char*)&c.handle);
	    }
	    else {
		printf( "Panic! Unrecognized COMM field %s\n", name );
	    }
	}
	break;
    case TYPE_MPIR_COMM_LIST:
	{
	    if (strcmp( name, "sequence_number" ) == 0) {
		off = OFFSET_OF(MPIR_Comm_list, sequence_number);
	    } else if (strcmp( name, "head" ) == 0) {
		off = OFFSET_OF(MPIR_Comm_list, head);
	    }
	    else {
		printf( "Panic! Unrecognized message match field %s\n", name );
	    }
	}
	break;
    case TYPE_MPID_REQUEST:
	{
	    if (strcmp( name, "status" ) == 0) {
		off = OFFSET_OF(MPID_Request, status);
	    } else if (strcmp( name, "cc" ) == 0) {
		off = OFFSET_OF(MPID_Request, cc);
	    } else if (strcmp( name, "next" ) == 0) {
		off = OFFSET_OF(MPID_Request, dcmf.next);
	    } else if (strcmp( name, "tag" ) == 0) {
		off = OFFSET_OF_MACRO(MPID_Request, MPID_Request_getMatchTag);
	    } else if (strcmp( name, "rank" ) == 0) {
		off = OFFSET_OF_MACRO(MPID_Request, MPID_Request_getMatchRank);
	    } else if (strcmp( name, "context_id" ) == 0) {
		off = OFFSET_OF_MACRO(MPID_Request, MPID_Request_getMatchCtxt);
	    }
	    else {
		printf( "Panic! Unrecognized Sendq field %s\n", name );
	    }
	}
	break;
    case TYPE_UNKNOWN:
	off = -1;
	break;
    default:
	off = -1;
	break;
    }
    return off;
}

/* Simulate converting name to the address of a variable/symbol */
int dbgrI_find_symbol( mqs_image *image, char *name, mqs_taddr_t * loc )
{
    if (strcmp( name, "MPIR_All_communicators" ) == 0) {
	*loc = (mqs_taddr_t)&MPIR_All_communicators;
	return mqs_ok;
    }
    else if (strcmp( name, "MPID_Recvq_posted_head_ptr" ) == 0) {
	*loc = (mqs_taddr_t)&MPID_Recvq_posted_head_ptr;
	printf( "Address of ptr to posted head ptr = %p\n", &MPID_Recvq_posted_head_ptr );
	printf( "Address of posted head ptr = %p\n", MPID_Recvq_posted_head_ptr );
	return mqs_ok;
    }
    else if (strcmp( name, "MPID_Recvq_unexpected_head_ptr" ) == 0) {
	*loc = (mqs_taddr_t)&MPID_Recvq_unexpected_head_ptr;
	return mqs_ok;
    }
    else if (strcmp( name, "MPIR_Sendq_head" ) == 0) {
	*loc = (mqs_taddr_t)&MPIR_Sendq_head;
	return mqs_ok;
    }
    else {
	printf( "Panic! Unrecognized symbol %s\n", name );
    }
    return 1;
}
