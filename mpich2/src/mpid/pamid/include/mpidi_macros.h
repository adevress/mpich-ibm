/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpidi_macros.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_macros_h__
#define __include_mpidi_macros_h__


/* Best results are achieved when your expression evaluates to 1 or 0. */
#define   likely(x) __builtin_expect(x,1)
#define unlikely(x) __builtin_expect(x,0)

#ifdef TRACE_ON
#ifdef __GNUC__
#define TRACE_ALL(fd, format, ...) fprintf(fd, "%s:%u (%d) " format, __FILE__, __LINE__, MPIR_Process.comm_world->rank, ##__VA_ARGS__)
#define TRACE_OUT(format, ...) TRACE_ALL(stdout, format, ##__VA_ARGS__)
#define TRACE_ERR(format, ...) TRACE_ALL(stderr, format, ##__VA_ARGS__)
#else
#define TRACE_OUT(format...) fprintf(stdout, format)
#define TRACE_ERR(format...) fprintf(stderr, format)
#endif
#else
#define TRACE_OUT(format...)
#define TRACE_ERR(format...)
#endif


/**
 * \addtogroup MPID_REQUEST
 * \{
 */
void    MPIDI_Request_complete(MPID_Request *req);
#define MPIDI_Request_decrement_cc(_req, _inuse) ({ *(_inuse) = --(*(_req)->cc_ptr);                                 })
#define MPIDI_Request_increment_cc(_req)         ({             ++(*(_req)->cc_ptr);                                 })
#define MPIDI_Request_get_cc(_req)               ({                *(_req)->cc_ptr;                                  })

#define MPIDI_Request_getCA(_req)                ({ (_req)->mpid.ca;                                                 })
#define MPIDI_Request_isSelf(_req)               ({ (_req)->mpid.isSelf;                                             })
#define MPIDI_Request_getPeerRank(_req)          ({ (_req)->mpid.peerrank;                                           })
#define MPIDI_Request_getPType(_req)             ({ (_req)->mpid.ptype;                                              })
#define MPIDI_Request_getControl(_req)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.control;          })
#define MPIDI_Request_isSync(_req)               ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync;           })
#define MPIDI_Request_isRzv(_req)                ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv;            })
#define MPIDI_Request_getMatchTag(_req)          ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag;           })
#define MPIDI_Request_getMatchRank(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank;          })
#define MPIDI_Request_getMatchCtxt(_req)         ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt;          })

#define MPIDI_Request_setCA(_req, _ca)           ({ (_req)->mpid.ca                                         = (_ca); })
#define MPIDI_Request_setSelf(_req,_t)           ({ (_req)->mpid.isSelf                                     = (_t);  })
#define MPIDI_Request_setPeerRank(_req,_r)       ({ (_req)->mpid.peerrank                                   = (_r);  })
#define MPIDI_Request_setPType(_req,_t)          ({ (_req)->mpid.ptype                                      = (_t);  })
#define MPIDI_Request_setControl(_req,_t)        ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.control  = (_t);  })
#define MPIDI_Request_setSync(_req,_t)           ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isSync   = (_t);  })
#define MPIDI_Request_setRzv(_req,_t)            ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.isRzv    = (_t);  })
#define MPIDI_Request_setMatch(_req,_tag,_rank,_ctxtid)                 \
({                                                                      \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPItag=(_tag);         \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIrank=(_rank);       \
  (_req)->mpid.envelope.envelope.msginfo.msginfo.MPIctxt=(_ctxtid);     \
})

#define MPIDI_Request_getPeerRequest(_req)      ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req;                     })
#define MPIDI_Msginfo_getPeerRequest(_msg)      ({                                (_msg)->msginfo.req;                     })
#define MPIDI_Request_setPeerRequest(_req,_r)   ({ (_req)->mpid.envelope.envelope.msginfo.msginfo.req = (_r); MPI_SUCCESS; })
#define MPIDI_Msginfo_cpyPeerRequest(_dst,_src) ({ (_dst)->msginfo.req = (_src)->msginfo.req;                 MPI_SUCCESS; })
#define MPIDI_Request_cpyPeerRequest(_dst,_src)   MPIDI_Msginfo_cpyPeerRequest(&(_dst)->mpid.envelope.envelope.msginfo,_src)
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

/* Add some error checking for size eventually */
#define MPIDI_Update_last_algorithm(_comm, _name) \
{ strncpy( (_comm)->mpid.last_algorithm, (_name), strlen((_name))+1); }

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


static inline unsigned
MPIDI_Context_hash(pami_task_t rank, unsigned ctxt)
{
  return (( rank + ctxt ) % MPIDI_Process.avail_contexts);
}
static inline void
MPIDI_Context_endpoint(MPID_Request * req, pami_endpoint_t * e)
{
  pami_task_t remote = MPIDI_Request_getPeerRank(req);
  pami_task_t local  = MPIR_Process.comm_world->rank;
  unsigned    rctxt  = MPIDI_Context_hash(local, req->comm->context_id);

  pami_result_t rc;
  rc = PAMI_Endpoint_create(MPIDI_Client, remote, rctxt, e);
  MPID_assert(rc == PAMI_SUCCESS);
}
static inline pami_context_t
MPIDI_Context_local(MPID_Request * req)
{
  pami_task_t remote = MPIDI_Request_getPeerRank(req);
  unsigned    lctxt  = MPIDI_Context_hash(remote, req->comm->context_id);
  MPID_assert(lctxt < MPIDI_Process.avail_contexts);
  return MPIDI_Context[lctxt];
}


static inline void
MPIDI_SendMsg(MPID_Request * sreq)
{
  pami_context_t context = MPIDI_Context_local(sreq);

  pami_result_t rc;
  rc = PAMI_Context_post(context, &sreq->mpid.post_request, MPIDI_SendMsg_handoff, sreq);
  MPID_assert(rc == PAMI_SUCCESS);
}


/**
 * \brief Macro for allocating memory
 *
 * \param[in] count Number of elements to allocate
 * \param[in] type  The type of the memory, excluding "*"
 * \return Address or NULL
 */
#define MPIU_Calloc0(count, type)               \
({                                              \
  size_t __size = (count) * sizeof(type);       \
  type* __p = MPIU_Malloc(__size);              \
  MPID_assert(__p != NULL);                     \
  if (__p != NULL)                              \
    memset(__p, 0, __size);                     \
  __p;                                          \
})

#define MPIU_TestFree(p)                        \
({                                              \
  if(*(p))                                      \
    {                                           \
      MPIU_Free(*(p));                          \
      *(p) = NULL;                              \
    }                                           \
})


/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state The previously seen state of advance
 */
#define MPID_Progress_start(state) ({ (*(state)).val = MPIDI_Progress_requests; })

/**
 * \brief Unused, provided since MPI calls it.
 * \param[in] state The previously seen state of advance
 */
#define MPID_Progress_end(state)

/**
 * \brief Signal MPID_Progress_wait() that something is done/changed
 *
 * It is therefore important that the ADI layer include a call to
 * MPIDI_Progress_signal() whenever something occurs that a node might
 * be waiting on.
 */
#define MPIDI_Progress_signal() ({ ++MPIDI_Progress_requests; })


#endif
