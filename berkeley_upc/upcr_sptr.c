
#include <upcr_internal.h>

#if UPCRI_SYMMETRIC_SEGMENTS
char	    *upcri_segsym_base = NULL;
char	    *upcri_segsym_end  = NULL;
ptrdiff_t    upcri_segsym_size = 0;
ptrdiff_t    upcri_segsym_region_size = 0;
uintptr_t    upcri_segsym_addrmask = (uintptr_t) -1;

uintptr_t upcri_segsym_size_shift = 0;
uintptr_t upcri_segsym_size_mask = 0;
uintptr_t upcri_segsym_region_size_mask = (uintptr_t) -1;
uintptr_t upcri_segsym_region_size_shift = 0;
#endif

const upcr_shared_ptr_t upcr_null_shared = UPCR_NULL_SHARED;
const upcr_pshared_ptr_t upcr_null_pshared = UPCR_NULL_PSHARED;

#if UPCR_USING_LINKADDRS
uintptr_t	upcri_linksegstart;
  #if !UPCRI_SINGLE_ALIGNED_REGIONS
  uintptr_t	*upcri_linkoffset;
  #endif
  #if !UPCRI_UPC_PTHREADS
  uintptr_t	upcri_linkoffset_single;
  #endif
#endif

upcr_handle_t
_upcr_do_memcpy(upcr_shared_ptr_t dst, upcr_shared_ptr_t src, 
	        size_t nbytes, int memcpy_type UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();
    gasnet_node_t snode = upcri_shared_nodeof(src);
    gasnet_node_t dnode = upcri_shared_nodeof(dst);
    gasnet_node_t thisnode = gasnet_mynode();
    void *saddr = upcri_shared_to_remote(src);
    void *daddr = upcri_shared_to_remote(dst);

    /* TODO: gasnet checks for node == thisnode anyway, so skip here */
    if (snode == dnode) {
	if (snode == thisnode) {
	    memcpy(daddr, saddr, nbytes);
	    return UPCR_INVALID_HANDLE;
	} else 
	    return upcri_do_remote_memcpy(snode, daddr, saddr, nbytes);
    } else if (snode == thisnode) {
	switch (memcpy_type) {
	    case upcri_memcpy_type_nb:
		return gasnet_put_nb_bulk(dnode, daddr, saddr, nbytes);
	    case upcri_memcpy_type_block:
		gasnet_put_bulk(dnode, daddr, saddr, nbytes);
		break;
	    case upcri_memcpy_type_nbi:
		gasnet_put_nbi_bulk(dnode, daddr, saddr, nbytes);
		break;
	}
    } else if (dnode == thisnode) {
	switch (memcpy_type) {
	    case upcri_memcpy_type_nb:
		return gasnet_get_nb_bulk(daddr, snode, saddr, nbytes);	
	    case upcri_memcpy_type_block:
		gasnet_get_bulk(daddr, snode, saddr, nbytes);
		break;
	    case upcri_memcpy_type_nbi:
		gasnet_get_nbi_bulk(daddr, snode, saddr, nbytes);
		break;
	}	
    } else {
	/* Third party memcpy w/src & dest on different nodes: will be slow,
	 * as we can't optimize to send a short msg to
	 * the source node telling it to do the bulk put (would violate Active
	 * Msg rule that handler can't initiate new request).  So instead we
	 * have to do two full copies of the data.  Not possible to make this
	 * truly nonblocking.  But we don't care much about the performance of
	 * 3rd party memcpy's anyway, as they should seldom be used by
	 * sensible programmers.
	 */
    	upcr_shared_ptr_t buf = upcr_local_alloc(1, nbytes);
	void *pbuf = upcr_shared_to_local(buf);
	upcr_memget(pbuf, src, nbytes);
	upcr_memput(dst, pbuf, nbytes);
	upcr_free(buf);
    }

    return UPCR_INVALID_HANDLE;
}

/* This AM handler runs on the remote node, and does the memcpy. 
 */
void 
upcri_remote_memcpy_SRequest(gasnet_token_t token, void *dst, void *src,
                             void *sz_as_ptr, void *trigger_addr)
{
    memcpy(dst, src, (size_t)(uintptr_t)sz_as_ptr);

    UPCRI_AM_CALL(UPCRI_SHORT_REPLY(1, 2,
                  (token, UPCRI_HANDLER_ID(upcri_remote_memcpy_SReply),
                        UPCRI_SEND_PTR(trigger_addr))));
}

/* This AM handler lets the originating node know when a remote memcpy it
 * ordered has been completed */
void 
upcri_remote_memcpy_SReply(gasnet_token_t token, void *trigger_addr)
{
    *((int *) trigger_addr) = UPCRI_REQUEST_DONE;
}

upcr_shared_ptr_t _bupc_local_to_shared(void *ptr, int thread, int phase)
{
    UPCR_BEGIN_FUNCTION();
    return upcr_mylocal_to_shared_withphase(ptr, phase, thread);
}

/* Loads a null-terminated string description of a shared pointer into a
 * buffer.  Returns the address of the buffer, or of an error string if the
 * buffer was too short.  The buffer passed should be at least
 * UPCRI_DUMP_MIN_LENGTH in length.
 */
int _bupc_dump_shared(upcr_shared_ptr_t ptr, char *buf, int maxlen) {
    UPCR_BEGIN_FUNCTION();
    int retval;

    if (maxlen < UPCRI_DUMP_MIN_LENGTH) {
	if (maxlen > 15)
	    strcpy(buf, "<buf too small>");
	errno = EINVAL;
	return -1;
    }

    if (upcr_isnull_shared(ptr)) {
	snprintf(buf, maxlen, "<NULL>");
    } else {
	snprintf(buf, maxlen, "<address=%p (addrfield=%p), thread=%u, phase=%u>",
		 (void *)upcri_shared_to_remote(ptr), 
		 (void *)upcr_addrfield_shared(ptr),
		 upcr_threadof_shared(ptr),
		 upcr_phaseof_shared(ptr));
    }

    return 0;
}

void
upcri_print_shared(upcr_shared_ptr_t sptr)
{
    UPCR_BEGIN_FUNCTION();

    fprintf(stdout, "<shared_ptr: addr=%p (offset=%p), thread=%u, phase=%u>\n",
	    (void *)upcri_shared_to_remote(sptr), 
	    (void *)upcr_addrfield_shared(sptr),
	    upcr_threadof_shared(sptr), 
	    upcr_phaseof_shared(sptr));
    fflush(stdout);
}

void
upcri_print_pshared(upcr_pshared_ptr_t sptr)
{
    UPCR_BEGIN_FUNCTION();

    fprintf(stdout, "<pshared_ptr: addr=%p (offset=%p), thread=%u>\n",
            (void *)upcri_pshared_to_remote(sptr), 
            (void *)upcr_addrfield_pshared(sptr),
            upcr_threadof_pshared(sptr)); 
    fflush(stdout);
}

#if UPCR_DEBUG && UPCRI_PACKED_SPTR
/* checks to see if addr/offset is too big to fit in address bits */ 
void 
upcri_check_addr_overflow(uintptr_t addrfield)
{
    if ((addrfield & (UPCRI_ADDR_MASK >> UPCRI_ADDR_SHIFT)) != addrfield)
	upcri_err("value %p would overflow shared pointer bits", 
		  (void *)addrfield);
}
#endif /* UPCR_DEBUG */

/* checks if argument looks like a valid shared pointer */
int _upcri_isvalid_shared(upcr_shared_ptr_t sptr) { 
    int retval = 1;
    upcr_thread_t th = upcr_threadof_shared(sptr);

    if (upcr_addrfield_shared(sptr) == 0) { /* null */
      if_pf (th != 0 || upcr_phaseof_shared(sptr) != 0)
        retval = 0;
    } else { 
      if_pf (th > upcri_threads) 
        retval = 0;
     #if UPCRI_SINGLE_ALIGNED_REGIONS
      else if_pf (upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_myregion_single + UPCR_PAGESIZE ||
                upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_myregion_single + upcri_perthread_segsize)
        retval = 0;
     #else
      else if_pf (upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_thread2region[th] + UPCR_PAGESIZE ||
                upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_thread2region[th] + upcri_perthread_segsize)
        retval = 0;
     #endif
    }

    return retval;
}

int _upcri_isvalid_pshared(upcr_pshared_ptr_t sptr) { 
    int retval = 1;
    upcr_thread_t th = upcr_threadof_pshared(sptr);

    if (upcr_addrfield_pshared(sptr) == 0) { /* null */
      if_pf (th != 0 || upcr_phaseof_pshared(sptr) != 0)
        retval = 0;
    } else {
      if_pf (th > upcri_threads) 
        retval = 0;
     #if UPCRI_PACKED_SPTR && !UPCRI_SYMMETRIC_PSHARED
      else if_pf ((sptr & UPCRI_PHASE_MASK) != 0) 
        retval = 0;
     #endif
     #if UPCRI_SINGLE_ALIGNED_REGIONS
      else if_pf (upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_myregion_single + UPCR_PAGESIZE ||
                upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_myregion_single + upcri_perthread_segsize)
        retval = 0;
     #else
      else if_pf (upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_thread2region[th] + UPCR_PAGESIZE ||
                upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_thread2region[th] + upcri_perthread_segsize)
        retval = 0;
     #endif
    }

    return retval;
}

#if UPCR_DEBUG 
  /* asserts argument looks like a valid shared pointer and returns it
   * works like isvalid, with an informative message on failure */
  upcr_shared_ptr_t _upcri_checkvalid_shared(upcr_shared_ptr_t sptr, 
                          const char *filename, int linenum) {
    const char *problem = NULL;
    upcr_thread_t th = upcr_threadof_shared(sptr);

    if (upcr_addrfield_shared(sptr) == 0) { /* null */
      if_pf (th != 0 || upcr_phaseof_shared(sptr) != 0)
        problem = "bad NULL";
    } else { 
      if_pf (th > upcri_threads) 
        problem = "bad thread field";
     #if UPCRI_SINGLE_ALIGNED_REGIONS
      else if_pf (upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_myregion_single + UPCR_PAGESIZE ||
                upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_myregion_single + upcri_perthread_segsize)
        problem = "addrfield out of range";
     #else
      else if_pf (upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_thread2region[th] + UPCR_PAGESIZE ||
                upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_thread2region[th] + upcri_perthread_segsize)
        problem = "addrfield out of range";
     #endif
    }

    if_pf (problem != NULL) 
      upcri_err("Attempt to use a bogus upcr_shared_ptr_t: %s\n"
                "  <shared_ptr: addr=%p (offset=%p), thread=%u, phase=%u>\n"
                "  at %s:%i\n",
            problem,
	    (void *)(upcr_addrfield_shared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th)), 
	    (void *)upcr_addrfield_shared(sptr),
	    (int)upcr_threadof_shared(sptr), 
	    (int)upcr_phaseof_shared(sptr),
            filename, linenum);
    return sptr;
  }

  upcr_shared_ptr_t _upcri_checkvalidlocal_shared(upcr_shared_ptr_t sptr, 
			    const char *filename, int linenum) 
  {
    UPCR_BEGIN_FUNCTION();
    _upcri_checkvalid_shared(sptr, filename, linenum);
    if (upcr_isnull_shared(sptr))
        upcri_err("Attempt to access a NULL shared pointer at %s:%d", 
		  filename, linenum);
    /* called by get/put functions, which should already have checked for local */
    upcri_assert(upcr_threadof_shared(sptr) == upcr_mythread());
    return sptr;
  }

  upcr_pshared_ptr_t _upcri_checkvalid_pshared(upcr_pshared_ptr_t sptr, 
                          const char *filename, int linenum) {
    const char *problem = NULL;
    upcr_thread_t th = upcr_threadof_pshared(sptr);

    #if UPCRI_SYMMETRIC_PSHARED
    if ((uintptr_t)(sptr) == 0) { /* null */
    }
    else if_pf (sptr < upcri_segsym_base || sptr >= upcri_segsym_end)
	problem = "pointer out of range";
    #else
    if (upcr_addrfield_pshared(sptr) == 0) { /* null */
      if_pf (th != 0 || upcr_phaseof_pshared(sptr) != 0)
        problem = "bad NULL";
    } 
    #endif
    else {
      if_pf (th > upcri_threads) 
        problem = "bad thread field";
     #if UPCRI_PACKED_SPTR && !UPCRI_SYMMETRIC_PSHARED
      else if_pf ((sptr & UPCRI_PHASE_MASK) != 0) 
        problem = "non-zero phase field in a pshared";
     #endif
     #if UPCRI_SINGLE_ALIGNED_REGIONS
      else if_pf (upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_myregion_single + UPCR_PAGESIZE ||
                upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_myregion_single + upcri_perthread_segsize)
        problem = "addrfield out of range";
     #else
      else if_pf (upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) < 
                upcri_thread2region[th] + UPCR_PAGESIZE ||
                upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th) > 
                upcri_thread2region[th] + upcri_perthread_segsize)
        problem = "addrfield out of range";
     #endif
    }

    if_pf (problem != NULL) 
      upcri_err("Attempt to use a bogus upcr_pshared_ptr_t: %s\n"
                "  <pshared_ptr: addr=%p (offset=%p), thread=%u, phase=%u>\n"
                "  at %s:%i\n",
            problem,
	  #if UPCRI_SYMMETRIC_PSHARED
	    (void *) sptr,
	  #else
	    (void *)(upcr_addrfield_pshared(sptr) UPCRI_PLUS_REMOTE_OFFSET(th)), 
	  #endif
	    (void *)upcr_addrfield_pshared(sptr),
	    (int)upcr_threadof_pshared(sptr), 
	  #if UPCRI_SYMMETRIC_PSHARED
	    0,
          #elif UPCRI_PACKED_SPTR
            (int)((sptr & UPCRI_PHASE_MASK) >> UPCRI_PHASE_SHIFT),
          #else
	    (int)upcr_phaseof_pshared(sptr),
          #endif
            filename, linenum);
    return sptr;
  }

  upcr_pshared_ptr_t _upcri_checkvalidlocal_pshared(upcr_pshared_ptr_t sptr, 
			    const char *filename, int linenum) 
  {
    UPCR_BEGIN_FUNCTION();
    _upcri_checkvalid_pshared(sptr, filename, linenum);
    if (upcr_isnull_pshared(sptr))
        upcri_err("Attempt to access a NULL shared pointer at %s:%d", 
		  filename, linenum);
    /* called by get/put functions, which should already have checked for local */
    upcri_assert(upcr_threadof_pshared(sptr) == upcr_mythread());
    return sptr;
  }
#endif
