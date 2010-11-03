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
  unsigned WinCtrl;
}      MPIDI_Protocol_t;

typedef struct
{
}      MPIDI_CollectiveProtocol_t;



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
typedef struct
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
typedef struct
  {
    void     * req;         /**< peer's request pointer */
    unsigned   MPItag;      /**< match tag              */
    unsigned   MPIrank;     /**< match rank             */
    uint16_t   MPIctxt;     /**< match context          */

    uint16_t   control:3;   /**< message type for control protocols */
    uint16_t   isSync:1;    /**< set for sync sends     */
    uint16_t   isRzv :1;    /**< use pt2pt rendezvous   */
} MPIDI_MsgInfo;

/** \brief Full Rendezvous msg info to be set as two quads of unexpected data. */
typedef struct
{
  MPIDI_MsgInfo    msginfo;
#ifdef USE_PAMI_RDMA
  pami_memregion_t memregion;
#else
  void           * data;
#endif
  size_t           length;
} MPIDI_MsgEnvelope;

/** \brief This defines the portion of MPID_Request that is specific to the Device */
struct MPIDI_Request
{
  MPIDI_MsgEnvelope     envelope;
  struct MPID_Request  *next;         /**< Link to next req. in queue */

  pami_work_t           post_request; /**<                            */

  void                 *userbuf;      /**< User buffer                */
  unsigned              userbufcount; /**< Userbuf data count         */
  MPI_Datatype          datatype;     /**< Data type of message       */
  pami_task_t           peerrank;     /**< The other guy's rank       */
  unsigned            cancel_pending; /**< Cancel status              */

  void                 *uebuf;        /**< Unexpected buffer          */
  unsigned              uebuflen;     /**< Length (bytes) of uebuf    */

  struct MPID_Datatype *datatype_ptr; /**< Info about the datatype    */

  MPIDI_REQUEST_PTYPE   ptype;        /**< The persistent msg type    */
  MPIDI_CA              ca;           /**< Completion action          */
#ifdef USE_PAMI_RDMA
  pami_memregion_t      memregion;    /**< Rendezvous recv memregion  */
#endif
};


/** \brief This defines the portion of MPID_Comm that is specific to the Device */
struct MPIDI_Comm
{
  pami_geometry_t geometry; /**< Geometry component for collectives      */
  pami_algorithm_t *coll_algorithm[PAMI_XFER_COUNT][2];
  pami_metadata_t *coll_metadata[PAMI_XFER_COUNT][2];
  char coll_count[PAMI_XFER_COUNT][2];
  pami_algorithm_t user_selected[PAMI_XFER_COUNT];
  /* no way to tell if user_selected[] is NULL */
  /* could probably union these two though? */
  char user_selectedvar[PAMI_XFER_COUNT];
  pami_metadata_t user_metadata[PAMI_XFER_COUNT];
  char last_algorithm[100][5];
  char last_algo_index;
  /* \todo Need to figure out how to deal with algorithms above the pami level */
  char allgathers[4];
  char allgathervs[4];
  char scattervs[2];
  char optgather, optscatter;
};



/** \brief Forward declaration of the MPID_Comm structure */
struct MPID_Comm;
/** \brief Forward declaration of the MPID_Win structure */
struct MPID_Win;
/** \brief Forward declaration of the MPID_Group structure */
struct MPID_Group;


struct MPIDI_Win_lock
{
  struct MPIDI_Win_lock *next;
  unsigned               rank;
  int                    type;
};
struct MPIDI_Win_queue
{
  struct MPIDI_Win_lock *head;
  struct MPIDI_Win_lock *tail;
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
struct MPIDI_Win_info
{
  void             *base_addr;     /**< Node's exposure window base address                  */
  struct MPID_Win  *win;
  uint32_t          disp_unit;     /**< Node's exposure window displacement units            */
  pami_memregion_t  memregion;     /**< Memory region descriptor for each node               */
};
/**
 * \brief Structure of PAMI extensions to MPID_Win structure
 */
struct MPIDI_Win
{
  struct MPID_Comm       *comm;    /**< saved pointer to window communicator           */
  struct MPIDI_Win_info  *info;    /**< allocated array of collective info             */
  struct MPIDI_Win_sync
  {
    uint32_t  type;   /**< current epoch type                                   */
    uint32_t  assert; /**< MPI_MODE_* bits asserted at epoch start              */

    uint32_t started;
    uint32_t complete;

    struct MPIDI_Win_sync_fence
    {
    } fence;
    struct MPIDI_Win_sync_pscw
    {
      struct MPID_Group *group;
      unsigned           count;
    } sc, pw;
    struct MPIDI_Win_sync_lock
    {
      struct
      {
        unsigned locked;
      } remote;
      struct
      {
        struct MPIDI_Win_queue requested;
        int                    type;
        unsigned               count;
      } local;
    } lock;
  } sync;
};


#endif
