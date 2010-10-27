/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_request.h
 * \brief ???
 */

#ifndef __src_mpid_request_h__
#define __src_mpid_request_h__


/**
 * \addtogroup MPID_REQUEST
 * \{
 */

#define MPID_Request_create MPID_Request_create_inline


extern MPIU_Object_alloc_t MPID_Request_mem;


void    MPIDI_Request_uncomplete(MPID_Request *req);
void    MPIDI_Request_complete(MPID_Request *req);
#if MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_THREAD_LOCAL
void    MPIDI_Request_allocate_pool();
#endif

#define MPIDI_Request_getCA(_req)                ({ (_req)->mpid.ca;                                })
#define MPIDI_Request_getPeerRank(_req)          ({ (_req)->mpid.peerrank;                          })
#define MPIDI_Request_getPType(_req)             ({ (_req)->mpid.ptype;                             })
#define MPIDI_Request_getControl(_req)           ({ (_req)->mpid.envelope.msginfo.control;          })
#define MPIDI_Request_isSync(_req)               ({ (_req)->mpid.envelope.msginfo.isSync;           })
#define MPIDI_Request_isRzv(_req)                ({ (_req)->mpid.envelope.msginfo.isRzv;            })
#define MPIDI_Request_getMatchTag(_req)          ({ (_req)->mpid.envelope.msginfo.MPItag;           })
#define MPIDI_Request_getMatchRank(_req)         ({ (_req)->mpid.envelope.msginfo.MPIrank;          })
#define MPIDI_Request_getMatchCtxt(_req)         ({ (_req)->mpid.envelope.msginfo.MPIctxt;          })

#define MPIDI_Request_setCA(_req, _ca)           ({ (_req)->mpid.ca                        = (_ca); })
#define MPIDI_Request_setPeerRank(_req,_r)       ({ (_req)->mpid.peerrank                  = (_r);  })
#define MPIDI_Request_setPType(_req,_t)          ({ (_req)->mpid.ptype                     = (_t);  })
#define MPIDI_Request_setControl(_req,_t)        ({ (_req)->mpid.envelope.msginfo.control  = (_t);  })
#define MPIDI_Request_setSync(_req,_t)           ({ (_req)->mpid.envelope.msginfo.isSync   = (_t);  })
#define MPIDI_Request_setRzv(_req,_t)            ({ (_req)->mpid.envelope.msginfo.isRzv    = (_t);  })
#define MPIDI_Request_setMatch(_req,_tag,_rank,_ctxtid) \
({                                                      \
  (_req)->mpid.envelope.msginfo.MPItag=(_tag);          \
  (_req)->mpid.envelope.msginfo.MPIrank=(_rank);        \
  (_req)->mpid.envelope.msginfo.MPIctxt=(_ctxtid);      \
})

#define MPIDI_Request_getPeerRequest(_req)      ({ (_req)->mpid.envelope.msginfo.req;                     })
#define MPIDI_Msginfo_getPeerRequest(_msg)      ({                       (_msg)->req;                     })
#define MPIDI_Request_setPeerRequest(_req,_r)   ({ (_req)->mpid.envelope.msginfo.req = (_r); MPI_SUCCESS; })
#define MPIDI_Msginfo_cpyPeerRequest(_dst,_src) ({                (_dst)->req = (_src)->req; MPI_SUCCESS; })
#define MPIDI_Request_cpyPeerRequest(_dst,_src) MPIDI_Msginfo_cpyPeerRequest(&(_dst)->mpid.envelope.msginfo,_src)


#define MPIU_HANDLE_ALLOCATION_MUTEX         0
#define MPIU_HANDLE_ALLOCATION_THREAD_LOCAL  1

/* XXX DJG for TLS hack */
#define MPID_REQUEST_TLS_MAX 128

#if MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_THREAD_LOCAL

extern __thread MPID_Request * MPID_PAMID_Thread_request_handles;
extern __thread int MPID_PAMID_Thread_request_handle_count;

#  define MPIDI_Request_tls_alloc(req)                                  \
({                                                                      \
  if (unlikely(!MPID_PAMID_Thread_request_handles)) {                   \
    MPIDI_Request_allocate_pool();                                      \
  }                                                                     \
  (req) = MPID_PAMID_Thread_request_handles;                            \
  MPID_PAMID_Thread_request_handles = req->mpid.next;                   \
  MPID_PAMID_Thread_request_handle_count -= 1;                          \
})

#  define MPIDI_Request_tls_free(req)                                   \
({                                                                      \
  if (MPID_PAMID_Thread_request_handle_count < MPID_REQUEST_TLS_MAX) {  \
    /* push request onto the top of the stack */                        \
    req->mpid.next = MPID_PAMID_Thread_request_handles;                 \
    MPID_PAMID_Thread_request_handles = req;                            \
    MPID_PAMID_Thread_request_handle_count += 1;                        \
  }                                                                     \
  else {                                                                \
    MPIU_Handle_obj_free(&MPID_Request_mem, req);                       \
  }                                                                     \
})

#elif MPIU_HANDLE_ALLOCATION_METHOD == MPIU_HANDLE_ALLOCATION_MUTEX
#  define MPIDI_Request_tls_alloc(req)                                  \
({                                                                      \
  (req) = MPIU_Handle_obj_alloc(&MPID_Request_mem);                     \
  if (req == NULL)                                                      \
    MPID_Abort(NULL, MPI_ERR_NO_SPACE, -1, "Cannot allocate Request");  \
})

#  define MPIDI_Request_tls_free(req) MPIU_Handle_obj_free(&MPID_Request_mem, (req))
#else
#  error MPIU_HANDLE_ALLOCATION_METHOD not defined
#endif


/**
 * \brief Create a very generic request
 * \note  This should only ever be called by more specific allocators
 */
static inline MPID_Request *
MPIDI_Request_create_basic()
{
  MPID_Request * req = NULL;

  MPIDI_Request_tls_alloc(req);
  MPID_assert(req != NULL);
  MPID_assert(HANDLE_GET_MPI_KIND(req->handle) == MPID_REQUEST);
  MPID_cc_set_1(&req->cc);
  req->cc_ptr = &req->cc;

#if 0
  /* This will destroy the MPID part of the request.  Use this to
     check for fields that are not being correctly initialized. */
  memset(&req->mpid, 0xFFFFFFFF, sizeof(struct MPIDI_Request));
#endif

  return req;
}


/**
 * \brief Create new request without initalizing
 */
static inline MPID_Request *
MPIDI_Request_create2_fast()
{
  MPID_Request * req;
  req = MPIDI_Request_create_basic();
  MPIU_Object_set_ref(req, 2);

  return req;
}


/**
 * \brief Create and initialize a new request
 */
static inline void
MPIDI_Request_initialize(MPID_Request * req)
{
  req->status.count      = 0;
  req->status.cancelled  = FALSE;
  req->status.MPI_SOURCE = MPI_UNDEFINED;
  req->status.MPI_TAG    = MPI_UNDEFINED;
  req->status.MPI_ERROR  = MPI_SUCCESS;

  struct MPIDI_Request* mpid = &req->mpid;
  mpid->cancel_pending   = FALSE;
  mpid->next             = NULL;
  mpid->datatype_ptr     = NULL;
  mpid->uebuf            = NULL;
  mpid->uebuflen         = 0;
  MPIDI_Request_setCA(req, MPIDI_CA_COMPLETE);

  MPIDI_Request_setRzv(req, 0);
}


/**
 * \brief Create and initialize a new request
 */
static inline MPID_Request *
MPID_Request_create_inline()
{
  MPID_Request * req;
  req = MPIDI_Request_create_basic();
  MPIU_Object_set_ref(req, 1);

  MPIDI_Request_initialize(req);
  req->comm=NULL;

  return req;
}


/**
 * \brief Create and initialize a new request
 */
static inline MPID_Request *
MPIDI_Request_create2()
{
  MPID_Request * req;
  req = MPID_Request_create();
  MPIU_Object_set_ref(req, 2);

  return req;
}


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

/** \} */


#endif
