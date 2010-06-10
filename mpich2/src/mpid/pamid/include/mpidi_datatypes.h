/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_datatypes.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_datatypes_h__
#define __include_mpidi_datatypes_h__


/**
 * \brief MPI Process descriptor
 *
 * This structure contains global configuration flags.
 */
typedef struct
{
  struct
  {
    unsigned topology;     /**< Enable optimized topology functions.   */
    unsigned collectives;  /**< Enable optimized collective functions. */
  }
  optimized;
  struct
  {
    unsigned rank;
    unsigned size;
  }
  global;
  unsigned short_limit;
  unsigned eager_limit;
  unsigned rma_pending;    /**< The max num outstanding requests during an RMA op    */
  unsigned verbose;        /**< The current level of verbosity for end-of-job stats. */
  unsigned statistics;     /**< The current level of stats collection.               */
  unsigned use_interrupts; /**< Should interrupts be turned on.                      */
  pami_geometry_t world_geometry;
  unsigned avail_contexts;
} MPIDI_Process_t;

typedef struct
{
  unsigned Send;
  unsigned RTS;
  unsigned Cancel;
  unsigned Control;
}      MPIDI_Protocol_t;

typedef struct
{
}      MPIDI_CollectiveProtocol_t;



/**
 * ******************************************************************
 * \brief MPI Onesided operation device declarations (!!! not used)
 * Is here only because mpiimpl.h needs it.
 * ******************************************************************
 */
typedef struct MPIDI_RMA_ops {
    struct MPIDI_RMA_ops *next;  /* pointer to next element in list */
    int type;  /* MPIDI_RMA_PUT, MPID_REQUEST_GET,
                  MPIDI_RMA_ACCUMULATE, MPIDI_RMA_LOCK */
    void *origin_addr;
    int origin_count;
    MPI_Datatype origin_datatype;
    int target_rank;
    MPI_Aint target_disp;
    int target_count;
    MPI_Datatype target_datatype;
    MPI_Op op;  /* for accumulate */
    int lock_type;  /* for win_lock */
} MPIDI_RMA_ops;

/* to send derived datatype across in RMA ops */
typedef struct MPIDI_RMA_dtype_info
{
  int      is_contig;
  int      n_contig_blocks;
  int      size;
  MPI_Aint extent;
  int      dataloop_size;
  void    *dataloop;
  int      dataloop_depth;
  int      eltype;
  MPI_Aint ub;
  MPI_Aint lb;
  MPI_Aint true_ub;
  MPI_Aint true_lb;
  int      has_sticky_ub;
  int      has_sticky_lb;
  int      unused0;
  int      unused1;
}
MPIDI_RMA_dtype_info;


/**
 * \brief This defines the type of message being sent/received
 * mpid_startall() invokes the correct start based on the type of the request
 */
typedef enum
  {
    MPIDI_REQUEST_PTYPE_RECV,
    MPIDI_REQUEST_PTYPE_SEND,
    MPIDI_REQUEST_PTYPE_BSEND,
    MPIDI_REQUEST_PTYPE_SSEND,
  }
MPIDI_REQUEST_PTYPE;


typedef enum
  {
    MPIDI_CONTROL_SSEND_ACKNOWLEDGE,
    MPIDI_CONTROL_CANCEL_REQUEST,
    MPIDI_CONTROL_CANCEL_ACKNOWLEDGE,
    MPIDI_CONTROL_CANCEL_NOT_ACKNOWLEDGE,
    MPIDI_CONTROL_RENDEZVOUS_ACKNOWLEDGE,
  }
MPIDI_CONTROL;


typedef enum
  {
    /** This must be 0, since new requests are memset to 0. */
    MPIDI_INITIALIZED=0,
    MPIDI_SEND_COMPLETE,
    MPIDI_ACKNOWLEGED,
    MPIDI_REQUEST_DONE_CANCELLED
  }
MPIDI_REQUEST_STATE;


/** \brief Request completion actions */
typedef enum
  {
    /** Just complete the request (this must be 0, since new requests
        are memset to 0). */
    MPIDI_CA_COMPLETE = 0,
    MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE,         /**< Unpack uebuf, then complete. */
    MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE,  /**< Unpack uebuf (do not free), then complete. */
  }
MPIDI_CA;


/**
 * \brief MPIDI_Message_match contains enough information to match an
 * MPI message.
 */
typedef struct MPIDI_Message_match
{
  int   tag;        /**< match tag     */
  int   rank;       /**< match rank    */
  int   context_id; /**< match context */
}
MPIDI_Message_match;


/**
 * \brief Message Info (has to be exactly 128 bits long) and associated data types
 * \note sizeof(MPIDI_MsgInfo) == 16
 */
struct MPIDI_MsgInfo_t
  {
    void     * req;         /**< peer's request pointer */
    unsigned   MPItag;      /**< match tag              */
    unsigned   MPIrank;     /**< match rank             */
    uint16_t   MPIctxt;     /**< match context          */

    uint16_t   control:3;   /**< message type for control protocols */
    uint16_t   isSync:1;    /**< set for sync sends     */
    uint16_t   isRzv :1;    /**< use pt2pt rendezvous   */
};

typedef union MPIDI_MsgInfo
{
  struct MPIDI_MsgInfo_t msginfo;
  /* DCQuad quad[DCQuad_sizeof(struct MPIDI_MsgInfo_t)]; */
}
MPIDI_MsgInfo;

/** \brief Full Rendezvous msg info to be set as two quads of unexpected data. */
typedef union
{
  struct MPIDI_MsgEnvelope_t
  {
    MPIDI_MsgInfo    msginfo;
#ifdef USE_PAMI_RDMA
    pami_memregion_t memregion;
#else
    void           * data;
#endif
    size_t           length;
  } envelope;
  /* DCQuad quad[DCQuad_sizeof(struct MPIDI_MsgEnvelope_t)]; */
} MPIDI_MsgEnvelope;

/** \brief This defines the portion of MPID_Request that is specific to the Device */
struct MPIDI_Request
{
  MPIDI_MsgEnvelope     envelope;
  struct MPID_Request  *next;         /**< Link to next req. in queue */

  pami_work_t           post_request; /**<                            */
  pami_task_t           peerrank;     /**< The other guy's rank       */

  void                 *userbuf;      /**< User buffer                */
  unsigned              userbufcount; /**< Userbuf data count         */
  void                 *uebuf;        /**< Unexpected buffer          */
  unsigned              uebuflen;     /**< Length (bytes) of uebuf    */

  MPI_Datatype          datatype;     /**< Data type of message       */
  struct MPID_Datatype *datatype_ptr; /**< Info about the datatype    */

  int                   isSelf;       /**< message sent to self       */
  int                 cancel_pending; /**< Cancel State               */
  MPIDI_REQUEST_PTYPE   ptype;        /**< The persistent msg type    */
  MPIDI_REQUEST_STATE   state;        /**< The tranfser state         */
  MPIDI_CA              ca;           /**< Completion action          */
#ifdef USE_PAMI_RDMA
  pami_memregion_t      memregion;    /**< Rendezvous recv memregion  */
#endif
};


/** \brief This defines the portion of MPID_Comm that is specific to the Device */
struct MPIDI_Comm
{
  pami_geometry_t geometry; /**< Geometry component for collectives      */
  /* It is unlikely these should stay in here, but more thought needs put in
   * to this after 5/1
   */
  pami_algorithm_t *bcasts;
  pami_algorithm_t *barriers;
  pami_algorithm_t *allreduces;
  pami_metadata_t *bcast_metas; /* is there one per algorithm? */
  pami_metadata_t *barrier_metas;
  pami_metadata_t *allreduce_metas;
};


/**
 * \brief Collective information related to a window
 *
 * This structure is used to share information about a local window with
 * all nodes in the window communicator. Part of that information includes
 * statistics about RMA operations during access/exposure epochs.
 *
 * The structure is allocated as an array sized for the window communicator.
 * Each entry in the array corresponds directly to the node of the same rank.
 */
struct MPID_Win_coll_info
{
  void            *base_addr;  /**< Node's exposure window base address                  */
  int              disp_unit;  /**< Node's exposure window displacement units            */
  MPI_Win          win_handle; /**< Node's exposure window handle (local to target node) */
  int              rma_sends;  /**< Count of RMA operations that target node             */
  pami_memregion_t mem_region; /**< Memory region descriptor for each node               */
};


struct MPID_Comm;
/**
 * \brief Structure of BG extensions to MPID_Win structure
 */
struct MPID_Dev_win_decl
{
  struct MPID_Win_coll_info *coll_info; /**< allocated array of collective info             */
  struct MPID_Comm *comm_ptr;           /**< saved pointer to window communicator           */
  volatile int      lock_granted;       /**< window lock                                    */
  unsigned long     _lock_queue[4];     /**< opaque structure used for lock wait queue      */
  unsigned long     _unlk_queue[4];     /**< opaque structure used for unlock wait queue    */
  volatile int      my_sync_begin;      /**< counter of POST messages received              */
  volatile int      my_sync_done;       /**< counter of COMPLETE messages received          */
  volatile int      my_rma_recvs;       /**< counter of RMA operations received             */
  volatile int      my_rma_pends;       /**< counter of RMA operations queued to send       */
  volatile int      my_get_pends;       /**< counter of GET operations queued               */
  volatile int      epoch_type;         /**< current epoch type                             */
  volatile int      epoch_size;         /**< current epoch size (or target for LOCK)        */
  int               epoch_assert;       /**< MPI_MODE_* bits asserted at epoch start        */
  int               epoch_rma_ok;       /**< flag indicating an exposure epoch is in affect */
};


#endif
