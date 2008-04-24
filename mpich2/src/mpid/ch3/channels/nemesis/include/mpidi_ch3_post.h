/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#if !defined(MPICH_MPIDI_CH3_POST_H_INCLUDED)
#define MPICH_MPIDI_CH3_POST_H_INCLUDED

/* #define MPIDI_CH3_EAGER_MAX_MSG_SIZE (1500 - sizeof(MPIDI_CH3_Pkt_t)) */
#define MPIDI_CH3_EAGER_MAX_MSG_SIZE   (128*1024)

#if !defined(MPICH_IS_THREADED)
#define MPIDI_CH3_Progress_start(state)
#define MPIDI_CH3_Progress_end(state)
#else
#define MPIDI_CH3_Progress_start(progress_state_)                                       \
{                                                                                       \
    (progress_state_)->ch.completion_count = MPIDI_CH3I_progress_completion_count;      \
}
#define MPIDI_CH3_Progress_end(progress_state_)
#endif

enum {
    MPIDI_CH3_start_packet_handler_id = 128,
    MPIDI_CH3_continue_packet_handler_id = 129,
    MPIDI_CH3_CTS_packet_handler_id = 130,
    MPIDI_CH3_reload_IOV_or_done_handler_id = 131,
    MPIDI_CH3_reload_IOV_reply_handler_id = 132
};


int MPIDI_CH3I_Progress(MPID_Progress_state *progress_state, int blocking);
MPID_Request *MPIDI_CH3_Progress_poke_with_matching(int,int,MPID_Comm *comm,int,int *,void *,int, MPI_Datatype, MPI_Status *);
MPID_Request *MPIDI_CH3_Progress_ipoke_with_matching(int,int,MPID_Comm *comm,int,int *,void *,int, MPI_Datatype, MPI_Status *);
#define MPIDI_CH3_Progress_test() MPIDI_CH3I_Progress(NULL, FALSE)
#define MPIDI_CH3_Progress_wait(progress_state) MPIDI_CH3I_Progress(progress_state, TRUE)
#define MPIDI_CH3_Progress_poke() MPIDI_CH3I_Progress(NULL, FALSE)

int MPIDI_CH3I_Posted_recv_enqueued (MPID_Request *rreq);
int MPIDI_CH3I_Posted_recv_dequeued (MPID_Request *rreq);

/*
 * Enable optional functionality
 */
#define MPIDI_CH3_Comm_Spawn MPIDI_CH3_Comm_Spawn

#include "mpid_nem_post.h"

/* communicator creation/descruction hooks */

int MPIDI_CH3I_comm_create (MPID_Comm *new_comm);
int MPIDI_CH3I_comm_destroy (MPID_Comm *new_comm);

/* rendezvous hooks */
int MPID_nem_lmt_RndvSend(MPID_Request **sreq_p, const void * buf, int count, MPI_Datatype datatype, int dt_contig,
                          MPIDI_msg_sz_t data_sz, MPI_Aint dt_true_lb, int rank, int tag, MPID_Comm * comm, int context_offset);
int MPID_nem_lmt_RndvRecv(struct MPIDI_VC *vc, MPID_Request *rreq);

#endif /* !defined(MPICH_MPIDI_CH3_POST_H_INCLUDED) */
