/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpid_macros.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpid_macros_h__
#define __include_mpid_macros_h__


/**
 * \addtogroup MPID_REQUEST
 * \{
 */
void    MPIDI_Request_complete(MPID_Request *req);
#define MPIDI_Request_add_ref(_req)                                     \
({                                                                      \
  MPID_assert(HANDLE_GET_MPI_KIND((_req)->handle) == MPID_REQUEST);     \
  MPIU_Object_add_ref(_req);                                            \
})

#define MPIDI_Request_decrement_cc(_req, _inuse) ({ *(_inuse) = --(*(_req)->cc_ptr);                                 })
#define MPIDI_Request_increment_cc(_req)         ({             ++(*(_req)->cc_ptr);                                 })
#define MPIDI_Request_get_cc(_req)               ({                *(_req)->cc_ptr;                                  })

#define MPIDI_Request_getCA(_req)                ({ (_req)->mpid.ca;                                                 })
#define MPIDI_Request_isSelf(_req)               ({ (_req)->mpid.isSelf;                                             })
#define MPIDI_Request_getPeerRank(_req)          ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.peerrank;         })
#define MPIDI_Request_getPeerRequest(_req)       ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req;              })
#define MPIDI_Request_getType(_req)              ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.type;             })
#define MPIDI_Request_isSync(_req)               ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync;           })
#define MPIDI_Request_isRzv(_req)                ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv;            })
#define MPIDI_Request_getMatchTag(_req)          ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag;           })
#define MPIDI_Request_getMatchRank(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank;          })
#define MPIDI_Request_getMatchCtxt(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt;          })

#define MPIDI_Request_setCA(_req, _ca)           ({ (_req)->mpid.ca                                         = (_ca); })
#define MPIDI_Request_setSelf(_req,_t)           ({ (_req)->mpid.isSelf                                     = (_t);  })
#define MPIDI_Request_setPeerRank(_req,_r)       ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.peerrank = (_r);  })
#define MPIDI_Request_setPeerRequest(_req,_r)    ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req      = (_r);  })
#define MPIDI_Request_setType(_req,_t)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.type     = (_t);  })
#define MPIDI_Request_setSync(_req,_t)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync   = (_t);  })
#define MPIDI_Request_setRzv(_req,_t)            ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv    = (_t);  })
#define MPIDI_Request_setMatch(_req,_tag,_rank,_ctxtid)                 \
({                                                                      \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag=(_tag);         \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank=(_rank);       \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt=(_ctxtid);     \
})
/** \} */


/**
 * \brief Gets significant info regarding the datatype
 * Used in mpid_send, mpidi_send.
 */
#define MPIDI_Datatype_get_info(_count, _datatype,              \
_dt_contig_out, _data_sz_out, _dt_ptr, _dt_true_lb)             \
({                                                              \
  if (HANDLE_GET_KIND(_datatype) == HANDLE_KIND_BUILTIN)        \
    {                                                           \
        (_dt_ptr)        = NULL;                                \
        (_dt_contig_out) = TRUE;                                \
        (_dt_true_lb)    = 0;                                   \
        (_data_sz_out)   = (_count) *                           \
        MPID_Datatype_get_basic_size(_datatype);                \
    }                                                           \
  else                                                          \
    {                                                           \
        MPID_Datatype_get_ptr((_datatype), (_dt_ptr));          \
        (_dt_contig_out) = (_dt_ptr)->is_contig;                \
        (_dt_true_lb)    = (_dt_ptr)->true_lb;                  \
        (_data_sz_out)   = (_count) * (_dt_ptr)->size;          \
    }                                                           \
})


/**
 * \brief Mark a request as cancel-pending
 * \param[in]  _req  The request to cancel
 * \param[out] _flag The previous state
 */
#define MPIDI_Request_cancel_pending(_req, _flag)       \
({                                                      \
  *(_flag) = (_req)->mpid.cancel_pending;               \
  (_req)->mpid.cancel_pending = TRUE;                   \
})


#endif
