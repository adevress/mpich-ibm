/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_hooks.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_hooks_h__
#define __include_mpidi_hooks_h__


typedef int                 MPID_VCR;
typedef struct MPIDI_VCRT * MPID_VCRT;
typedef size_t MPIDI_msg_sz_t;


#define MPID_Irsend     MPID_Isend
#define MPID_Rsend      MPID_Send
#define MPID_Rsend_init MPID_Send_init


/**
 * \brief Declare hook(s) for Datatype create/destroy
 *
 * multiple hooks could be defined, for example:
 * #define ...hook(a)   { func1(a); func2(a); ... }
 */
#define MPID_Dev_datatype_create_hook(a)
#define MPID_Dev_datatype_destroy_hook(a) {             \
        extern void MPIDU_dtc_free(MPID_Datatype*);     \
        MPIDU_dtc_free(a);                              \
}

#define MPID_GPID_Get(comm_ptr, rank, gpid)     \
{                                               \
  gpid[0] = 0;                                  \
  gpid[1] = comm_ptr->vcr[rank];                \
}

/** \brief Our progress engine does not require state */
#define MPID_PROGRESS_STATE_DECL

/** \brief This defines the portion of MPID_Request that is specific to the Device */
#define MPID_DEV_REQUEST_DECL struct MPIDI_Request mpid;

/** \brief This defines the portion of MPID_Comm that is specific to the Device */
#define MPID_DEV_COMM_DECL struct MPIDI_Comm mpid;

#define HAVE_DEV_COMM_HOOK
#define MPID_Dev_comm_create_hook(a)  ({ void MPIDI_Comm_create (MPID_Comm *comm); MPIDI_Comm_create (a); })
#define MPID_Dev_comm_destroy_hook(a) ({ void MPIDI_Comm_destroy(MPID_Comm *comm); MPIDI_Comm_destroy(a); })

/**
 * \brief Code-snippet macro to add PAMI extensions to MPID_Win object structure
 */
#define MPID_DEV_WIN_DECL struct MPIDI_Win mpid;


#endif
