/*   $Source: /var/local/cvs/upcr/upcr_locks.c,v $
 *     $Date: 2007/09/08 19:51:26 $
 * $Revision: 1.49 $
 * Description: UPC lock implementation on GASNet
 * Copyright 2002, Dan Bonachea <bonachea@cs.berkeley.edu>
 */

#include <upcr_internal.h>
#include <upcr_handlers.h>

#ifndef UPCRI_USE_SMPLOCKS
#define UPCRI_USE_SMPLOCKS 1
#endif
#ifndef UPCRI_CACHEPAD_SMPLOCKS
#define UPCRI_CACHEPAD_SMPLOCKS 1
#endif

/* -------------------------------------------------------------------------- */
#if GASNET_CONDUIT_SMP && UPCRI_USE_SMPLOCKS
  /* implement upc_lock_t using bare pthread mutexes to save some overhead
     this is *only* safe because smp-conduit has a no-op AMPoll
   */
  #if UPCRI_SUPPORT_PTHREADS
    #define UPCRI_MUTEX_LOCKT               pthread_mutex_t
    #define _UPCRI_MUTEX_LOCKINIT(pmutex)    (memset(pmutex,0,sizeof(*(pmutex))), \
                                              pthread_mutex_init((pmutex),NULL))
    #define _UPCRI_MUTEX_LOCK(pmutex) do {                                \
      if_pf (upcri_polite_wait) pthread_mutex_lock(pmutex);               \
      else while (pthread_mutex_trylock(pmutex)) gasnett_spinloop_hint(); \
    } while (0)
    #define _UPCRI_MUTEX_TRYLOCK(pmutex)     (!pthread_mutex_trylock(pmutex))
    #define _UPCRI_MUTEX_UNLOCK(pmutex)      pthread_mutex_unlock(pmutex)
    #if PLATFORM_OS_NETBSD
      /* bug 1476: destroying a locked mutex has undefined effects by POSIX, and some
       * systems whine about it. So instead use the following sequence which
       * accomplishes the same effect, but leaks the mutex if it's held by another
       * thread (unlocking another thread's held lock also has undefined effects). 
       */
      #define _UPCRI_MUTEX_DESTROY(pmutex)     do {       \
          pthread_mutex_t *_pm = (pmutex);                \
          if (!pthread_mutex_trylock(_pm)) { /* got it */ \
            pthread_mutex_unlock(_pm);                    \
            pthread_mutex_destroy(_pm);                   \
          } else { /* already held by someone */          \
           memset(_pm,0,sizeof(*_pm)); /* clobber */      \
          }                                               \
        } while (0)
    #else
      #define _UPCRI_MUTEX_DESTROY(pmutex)     pthread_mutex_destroy(pmutex)
    #endif
  #else
    #undef UPCRI_CACHEPAD_SMPLOCKS
    #define UPCRI_CACHEPAD_SMPLOCKS 0 /* no point in wasting space */
    #define UPCRI_MUTEX_LOCKT                char
    #define _UPCRI_MUTEX_LOCKINIT(pmutex)    ((void)0)
    #define _UPCRI_MUTEX_LOCK(pmutex)        ((void)0)
    #define _UPCRI_MUTEX_TRYLOCK(pmutex)     (1)
    #define _UPCRI_MUTEX_UNLOCK(pmutex)      ((void)0)
    #define _UPCRI_MUTEX_DESTROY(pmutex)     ((void)0)
  #endif
  #if UPCR_LOCK_DEBUG
    #define UPCRI_LOCKC_LOCKT          \
      typedef struct _upcri_lock_t {   \
        UPCRI_MUTEX_LOCKT mutex;       \
        uint8_t islocked;              \
        upcr_thread_t owner_thread_id; \
        gasnett_atomic_t num_waiters;  \
      } upcri_lock_t
    #define UPCRI_LOCKC_LOCKINIT(lockaddr) do {             \
        (lockaddr)->islocked = 0;                           \
        (lockaddr)->owner_thread_id = (upcr_thread_t)-1;    \
        gasnett_atomic_set(&((lockaddr)->num_waiters), 0, 0); \
        _UPCRI_MUTEX_LOCKINIT(&((lockaddr)->mutex));        \
      } while (0)
    #define UPCRI_MUTEX_LOCK(lockaddr) do {                                   \
        gasnett_atomic_increment(&((lockaddr)->num_waiters), 0);              \
        _UPCRI_MUTEX_LOCK(&((lockaddr)->mutex));                              \
        upcri_assert(!(lockaddr)->islocked &&                                 \
                       (lockaddr)->owner_thread_id == (upcr_thread_t)-1);     \
        upcri_assert(gasnett_atomic_read(&((lockaddr)->num_waiters), 0) > 0); \
        gasnett_atomic_decrement(&((lockaddr)->num_waiters), 0);              \
        (lockaddr)->islocked = 1;                                             \
        (lockaddr)->owner_thread_id = upcr_mythread();                        \
      } while (0)
    #define UPCRI_MUTEX_TRYLOCK(lockaddr,success) do {                    \
        success = _UPCRI_MUTEX_TRYLOCK(&((lockaddr)->mutex));             \
        if (success) {                                                    \
          upcri_assert(!(lockaddr)->islocked &&                           \
                       (lockaddr)->owner_thread_id == (upcr_thread_t)-1); \
          (lockaddr)->islocked = 1;                                       \
          (lockaddr)->owner_thread_id = upcr_mythread();                  \
        }                                                                 \
      } while (0)
    #define UPCRI_MUTEX_UNLOCK(lockaddr) do {                        \
       upcri_assert((lockaddr)->islocked);                           \
       upcri_assert((lockaddr)->owner_thread_id == upcr_mythread()); \
       (lockaddr)->islocked = 0;                                     \
       (lockaddr)->owner_thread_id = (upcr_thread_t)-1;              \
       _UPCRI_MUTEX_UNLOCK(&((lockaddr)->mutex));                    \
      } while (0)
    #define UPCRI_MUTEX_DESTROY(lockptr, lockaddr) do {                  \
        upcr_thread_t _tmp;                                              \
        _UPCRI_MUTEX_TRYLOCK(&((lockaddr)->mutex)); /* reduce races */   \
        _tmp = (lockaddr)->owner_thread_id;                              \
        if (_tmp != (upcr_thread_t)-1)                                   \
          upcri_lock_unlink(upcri_getlockinfo(_tmp), lockptr, 1);        \
        if (gasnett_atomic_read(&((lockaddr)->num_waiters), 0) > 0)      \
          upcri_err("upc_lock_free() was called on a upc_lock_t *,"      \
                    " while one or more threads were blocked trying to " \
                    "acquire it in upc_lock()");                         \
        _UPCRI_MUTEX_DESTROY(&((lockaddr)->mutex));                      \
      } while (0)
  #else
    #if UPCRI_CACHEPAD_SMPLOCKS
      #define UPCRI_LOCKC_LOCKT                                                             \
        typedef struct _upcri_lock_t {                                                      \
          UPCRI_MUTEX_LOCKT mutex;                                                          \
          char _pad[MAX(8,(int)GASNETT_CACHE_LINE_BYTES - (int)sizeof(UPCRI_MUTEX_LOCKT))]; \
        } upcri_lock_t
      #define UPCRI_LOCKC_LOCKINIT(lockaddr)          _UPCRI_MUTEX_LOCKINIT(&((lockaddr)->mutex))
      #define UPCRI_MUTEX_LOCK(lockaddr)              _UPCRI_MUTEX_LOCK(&((lockaddr)->mutex))
      #define UPCRI_MUTEX_TRYLOCK(lockaddr, success)  success = _UPCRI_MUTEX_TRYLOCK(&((lockaddr)->mutex))
      #define UPCRI_MUTEX_UNLOCK(lockaddr)            _UPCRI_MUTEX_UNLOCK(&((lockaddr)->mutex))
      #define UPCRI_MUTEX_DESTROY(lockptr, lockaddr)  _UPCRI_MUTEX_DESTROY(&((lockaddr)->mutex))
    #else
      #define UPCRI_LOCKC_LOCKT                       typedef UPCRI_MUTEX_LOCKT upcri_lock_t
      #define UPCRI_LOCKC_LOCKINIT(lockaddr)          _UPCRI_MUTEX_LOCKINIT(lockaddr)
      #define UPCRI_MUTEX_LOCK(lockaddr)              _UPCRI_MUTEX_LOCK(lockaddr)
      #define UPCRI_MUTEX_TRYLOCK(lockaddr, success)  success = _UPCRI_MUTEX_TRYLOCK(lockaddr)
      #define UPCRI_MUTEX_UNLOCK(lockaddr)            _UPCRI_MUTEX_UNLOCK(lockaddr)
      #define UPCRI_MUTEX_DESTROY(lockptr, lockaddr)  _UPCRI_MUTEX_DESTROY(lockaddr)
    #endif
  #endif
  
  #define UPCRI_LOCKC_LOCK(li, node, lockaddr, isblocking, success) do { \
      upcri_assert(node == gasnet_mynode());                             \
      if (isblocking) {                                                  \
          UPCRI_MUTEX_LOCK(lockaddr);                                    \
          success = 1;                                                   \
      } else UPCRI_MUTEX_TRYLOCK(lockaddr,success);                      \
    } while (0)
  #define UPCRI_LOCKC_UNLOCK(li, node, lockaddr) do { \
      upcri_assert(node == gasnet_mynode());          \
      UPCRI_MUTEX_UNLOCK(lockaddr);                   \
    } while (0)
  #define UPCRI_LOCKC_DESTROY(lockptr, lockaddr) do { \
      UPCRI_MUTEX_DESTROY(lockptr, lockaddr);         \
      _upcr_free(lockptr UPCRI_PT_PASS);              \
    } while (0)
#else
  #define UPCRI_AMLOCKS  1
#endif
/* -------------------------------------------------------------------------- */

typedef enum {
    UPCRI_LOCKSTATE_WAITING,	/* waiting for a reply */
    UPCRI_LOCKSTATE_GRANTED,	/* lock attempt granted */
    UPCRI_LOCKSTATE_FAILED,	/* lock attempt failed */
    UPCRI_LOCKSTATE_HANDOFF,	/* unlock op complete--handoff in progress */
    UPCRI_LOCKSTATE_DONE	/* unlock op complete */
} upclock_state_t;


typedef struct _upcri_lock_waiter {
    struct _upcri_lock_waiter *next;
    upcr_thread_t thread_id;
} upcri_lock_waiter_t;

#ifdef UPCRI_LOCKC_LOCKT
  UPCRI_LOCKC_LOCKT;
#else
  typedef struct _upcri_lock_t {
      gasnet_hsl_t hsl;
      uint8_t islocked;
      upcri_lock_waiter_t *waiters; /* threads blocking for the lock */
      upcri_lock_waiter_t *freelist; /* freelist of waiter entries */
  #if UPCR_LOCK_DEBUG
      int pending_free;
      upcr_thread_t owner_thread_id;
  #endif
  } upcri_lock_t;
#endif

#if UPCR_LOCK_DEBUG
typedef struct _upcri_locklist_t {
    upcr_shared_ptr_t lock;
    struct _upcri_locklist_t *next;
} upcri_locklist_t;
#endif

/* -------------------------------------------------------------------------- */
extern void _upcri_locksystem_init(UPCRI_PT_ARG_ALONE) { /* called by each pthread during startup */
  #if UPCR_LOCK_DEBUG
    gasnet_hsl_init(&(upcri_auxdata()->lock_info.locksheld_hsl));
  #endif
}
/* -------------------------------------------------------------------------- */
#if UPCR_LOCK_DEBUG
  /* linked-list management for lock debug sanity checking */
  void upcri_lock_check(upcri_lockinfo_t *li, upcr_shared_ptr_t lockptr) {
    gasnet_hsl_lock(&(li->locksheld_hsl));
    { upcri_locklist_t *p = (upcri_locklist_t *) li->upclock_locksheld;
      while (p) {
	  if (upcr_isequal_shared_shared(p->lock, lockptr)) {
	      upcri_err("Attempted to acquire a upc_lock that was already held");
	  }
	  p = p->next;
      }
    }
    gasnet_hsl_unlock(&(li->locksheld_hsl));
  }
  void upcri_lock_link(upcri_lockinfo_t *li, upcr_shared_ptr_t lockptr) {
    upcri_locklist_t **p = (upcri_locklist_t **) &(li->upclock_locksheld);
    upcri_locklist_t *newentry =
	(upcri_locklist_t *) upcri_checkmalloc(sizeof(upcri_locklist_t));
    gasnet_hsl_lock(&(li->locksheld_hsl));
      newentry->lock = lockptr;
      newentry->next = *p;
      *p = newentry;
    gasnet_hsl_unlock(&(li->locksheld_hsl));
  }
  void upcri_lock_unlink(upcri_lockinfo_t *li, upcr_shared_ptr_t lockptr, int ignorefailure) {
      upcri_locklist_t **p = (upcri_locklist_t **) & (li->upclock_locksheld);
      int found = 0;

    gasnet_hsl_lock(&(li->locksheld_hsl));
      while (*p) {
	  if (upcr_isequal_shared_shared((*p)->lock, lockptr)) {
	      upcri_locklist_t *temp = *p;
	      *p = (*p)->next;
	      upcri_free(temp);
	      found = 1;
	      break;
	  }
	  p = &((*p)->next);
      }
      if (!found && !ignorefailure)
	  upcri_err("Attempted to release a upc_lock that was not held");
    gasnet_hsl_unlock(&(li->locksheld_hsl));
  }
#endif
/* -------------------------------------------------------------------------- */
#define upcri_createlock() _upcri_createlock(UPCRI_PT_PASS_ALONE)

static upcr_shared_ptr_t _upcri_createlock(UPCRI_PT_ARG_ALONE) {
    upcr_shared_ptr_t retval = upcr_local_alloc(1, sizeof(upcri_lock_t));
    upcri_lock_t *ul = upcri_shared_to_remote(retval);

  #ifdef UPCRI_LOCKC_LOCKINIT
    UPCRI_LOCKC_LOCKINIT(ul);
  #else
    gasnet_hsl_init(&(ul->hsl));
    ul->islocked = 0;
    ul->waiters = NULL;
    ul->freelist = NULL;
    #if UPCR_LOCK_DEBUG
      ul->pending_free = 0;
    #endif
  #endif
    return retval;
}

/* -------------------------------------------------------------------------- */
void upcri_do_local_free(upcr_shared_ptr_t sptr);

void upcri_SRQ_lockdestroy(gasnet_token_t token, gasnet_handlerarg_t lock_threadid, 
                           gasnet_handlerarg_t freer_threadid, void *_lock) {
  #if !UPCRI_AMLOCKS
    upcri_err("Bad call to upcri_SRQ_lockdestroy");
  #else
    upcri_lock_t *lock = (upcri_lock_t *) _lock;
    upcri_lock_waiter_t *fe;
    int freemem = 1;
    
    if (lock->waiters != NULL) {
      upcri_err("upc_lock_free() was called on a upc_lock_t *,"
                " while one or more threads were blocked trying to acquire it in upc_lock()");
    }
    #if UPCR_LOCK_DEBUG
    { int lock_holder = -1;
      upclock_state_t replystate = UPCRI_LOCKSTATE_DONE;
      gasnet_hsl_lock(&(lock->hsl));
        if (lock->islocked) lock_holder = lock->owner_thread_id;
        lock->pending_free = 1; /* tell the world we're about to free it */
      gasnet_hsl_unlock(&(lock->hsl));
      if (lock_holder != -1) { /* one thread freed the lock while another was holding it */
        /* pass the remaining work back to the freer, 
           who will execute a remote upc_free for the memory once the lock is dead
         */
        freemem = 0;
        replystate = UPCRI_LOCKSTATE_HANDOFF;
      }
      UPCRI_AM_CALL(UPCRI_SHORT_REPLY(3, 3,
		   (token, UPCRI_HANDLER_ID(upcri_SRP_setlockstate),
		    freer_threadid, replystate, lock_holder)));
    }
    #endif
    gasnet_hsl_destroy(&(lock->hsl));
    fe = lock->freelist;
    while (fe) {
      upcri_lock_waiter_t *tmp = fe;
      fe = tmp->next;
      upcri_free(tmp);
    }

    /* call the AM-safe version of shared free, 
       which only works for locally-managed shared heaps */
    if (freemem) {
      upcr_shared_ptr_t ul_sptr = upcr_local_to_shared_withphase(lock,0,lock_threadid);
      upcri_do_local_free(ul_sptr);
    }
  #endif
}

void upcri_SRQ_freeheldlock(gasnet_token_t token, 
                            gasnet_handlerarg_t lock_threadid,
                            gasnet_handlerarg_t owner_thread_id, 
                            gasnet_handlerarg_t freer_threadid,
                            void *_lock) {
  #if UPCR_LOCK_DEBUG && UPCRI_AMLOCKS
    upcri_lock_t *lock = (upcri_lock_t *) _lock;
    upcr_shared_ptr_t ul_sptr = upcr_local_to_shared_withphase(lock,0,lock_threadid);
    upcri_lockinfo_t * const his_li = upcri_getlockinfo(owner_thread_id);

    /* we ignore failures here to prevent a race where the unlock happenned after the
      free but before this AM arrived, to ensure the final error message 
      is "unlock of freed lock" */
    upcri_lock_unlink(his_li, ul_sptr, 1);

    UPCRI_AM_CALL(UPCRI_SHORT_REPLY(3, 3,
		 (token, UPCRI_HANDLER_ID(upcri_SRP_setlockstate),
		  freer_threadid, UPCRI_LOCKSTATE_DONE, 0)));
  #else
    upcri_err("illegal call to upcri_SRQ_freeheldlock");
  #endif
}
/* -------------------------------------------------------------------------- */
upcr_shared_ptr_t _upcr_global_lock_alloc(UPCRI_PT_ARG_ALONE) {
    upcr_shared_ptr_t retval;
    UPCRI_TRACE_PRINTF(("HEAPOP upc_global_lock_alloc()"));
    #if GASNET_STATS
      upcri_auxdata()->lock_info.alloccnt++;
    #endif
    #define UPCRI_PEVT_ARGS 
    upcri_pevt_start(GASP_UPC_GLOBAL_LOCK_ALLOC);
    #undef UPCRI_PEVT_ARGS

    retval = upcri_createlock();

    #define UPCRI_PEVT_ARGS , &retval
    upcri_pevt_end(GASP_UPC_GLOBAL_LOCK_ALLOC);
    #undef UPCRI_PEVT_ARGS

    return upcri_checkvalid_shared(retval);
}

upcr_shared_ptr_t _upcr_all_lock_alloc(UPCRI_PT_ARG_ALONE) {
    upcr_shared_ptr_t retval;

    UPCRI_TRACE_PRINTF(("HEAPOP upc_all_lock_alloc()"));
    #define UPCRI_PEVT_ARGS 
    upcri_pevt_start(GASP_UPC_ALL_LOCK_ALLOC);
    #undef UPCRI_PEVT_ARGS

    if (upcr_mythread() == 0) {
	retval = upcri_createlock();
        #if GASNET_STATS
          upcri_auxdata()->lock_info.alloccnt++;
        #endif
    }
    upcri_broadcast(0, &retval, sizeof(retval));

    #define UPCRI_PEVT_ARGS , &retval
    upcri_pevt_end(GASP_UPC_ALL_LOCK_ALLOC);
    #undef UPCRI_PEVT_ARGS

    return upcri_checkvalid_shared(retval);
}

void _upcr_lock_free(upcr_shared_ptr_t lockptr UPCRI_PT_ARG) {
  #define UPCRI_PEVT_ARGS , &lockptr
  upcri_pevt_start(GASP_UPC_LOCK_FREE);
  {
    upcr_thread_t lock_threadaff = upcr_threadof_shared(lockptr);
    gasnet_node_t node = upcri_thread_to_node(lock_threadaff);
    upcri_lock_t *lockaddr = upcri_shared_to_remote(lockptr);
  #ifdef GASNET_TRACE
    char ptrstr[UPCRI_DUMP_MIN_LENGTH];
    upcri_dump_shared(lockptr, ptrstr, UPCRI_DUMP_MIN_LENGTH);
    UPCRI_TRACE_PRINTF(("HEAPOP upc_lock_free(%s)", ptrstr));
  #endif
    if (upcr_isnull_shared(lockptr)) return; /* upc_lock_free(NULL) is a no-op */
  #if GASNET_STATS
    upcri_auxdata()->lock_info.freecnt++;
  #endif

#ifdef UPCRI_LOCKC_DESTROY
  UPCRI_LOCKC_DESTROY(lockptr, lockaddr);
#else
  #if UPCR_LOCK_DEBUG
    upcri_auxdata()->lock_info.upclock_state = (int) UPCRI_LOCKSTATE_WAITING;
    /* this memory barrier prevents a race against GASNet handler on upclock_state */
    gasnett_local_wmb();
  #endif

  UPCRI_AM_CALL(
      UPCRI_SHORT_REQUEST(3, 4, 
	  (node, UPCRI_HANDLER_ID(upcri_SRQ_lockdestroy), 
           lock_threadaff, upcr_mythread(),
	   UPCRI_SEND_PTR(lockaddr))));

  #if UPCR_LOCK_DEBUG
    { /* need to block here to ensure everything got destroyed cleanly and 
       no additional help from us is necessary. This could actually be made
       non-blocking with a more sophisticated algorithm, but it's not worth
       tuning the performance because it currently only affects UPCR_LOCK_DEBUG mode
       */
      upcri_lockinfo_t * const li = &(upcri_auxdata()->lock_info);
      GASNET_BLOCKUNTIL(li->upclock_state != (int) UPCRI_LOCKSTATE_WAITING);
      if ((upclock_state_t)(li->upclock_state) == UPCRI_LOCKSTATE_HANDOFF) {
        upcr_thread_t lock_owner = (upcr_thread_t)li->upclock_handoffarg;
        node = upcri_thread_to_node(lock_owner);
        li->upclock_state = (int) UPCRI_LOCKSTATE_WAITING;
        /* this memory barrier prevents a race against GASNet handler on upclock_state */
        gasnett_local_wmb();
        UPCRI_AM_CALL(
            UPCRI_SHORT_REQUEST(4, 5, 
	        (node, UPCRI_HANDLER_ID(upcri_SRQ_freeheldlock), 
                 lock_threadaff, lock_owner, upcr_mythread(),
	         UPCRI_SEND_PTR(lockaddr))));
        GASNET_BLOCKUNTIL(li->upclock_state != (int) UPCRI_LOCKSTATE_WAITING);
        _upcr_free(lockptr UPCRI_PT_PASS); /* now safe to free lock data */
      }
      upcri_assert((upclock_state_t)(li->upclock_state) == UPCRI_LOCKSTATE_DONE);
    }
  #endif
#endif
  }

  upcri_pevt_end(GASP_UPC_LOCK_FREE);
  #undef UPCRI_PEVT_ARGS
}

/* -------------------------------------------------------------------------- */
GASNETT_INLINE(upcri_doSRP_setlockstate)
void upcri_doSRP_setlockstate( gasnet_handlerarg_t threadid,
			       gasnet_handlerarg_t state,
			       gasnet_handlerarg_t handoffarg) {
    upcri_lockinfo_t * const his_li = upcri_getlockinfo(threadid);

    if ((upclock_state_t)state == UPCRI_LOCKSTATE_HANDOFF) {
      his_li->upclock_handoffarg = (upcr_thread_t) handoffarg;
      gasnett_local_wmb();
    }
    his_li->upclock_state = (int) state;
}
void upcri_SRP_setlockstate(gasnet_token_t token,
			    gasnet_handlerarg_t threadid,
			    gasnet_handlerarg_t state,
			    gasnet_handlerarg_t handoffarg) {
  upcri_doSRP_setlockstate(threadid, state, handoffarg);
}
/* -------------------------------------------------------------------------- */
GASNETT_INLINE(upcri_doSRQ_lock)
void upcri_doSRQ_lock(gasnet_handlerarg_t threadid,
	        gasnet_handlerarg_t isblocking, upcri_lock_t *lock,
                int *replystate) {
  #if !UPCRI_AMLOCKS
    *replystate = 0; /* suppress uninitialized var warning */
    upcri_err("Bad call to upcri_doSRQ_lock");
  #else
    if (!isblocking && lock->islocked) { /* bug 1209: reduce HSL contention from failed attempts */
       *replystate = UPCRI_LOCKSTATE_FAILED;
       return;
    }
    gasnet_hsl_lock(&(lock->hsl));
    if (lock->islocked) {
        #if UPCR_LOCK_DEBUG
	  upcri_assert(lock->owner_thread_id != threadid);
        #endif
	if (isblocking) {	/* add waiter to end of linked list */
	    upcri_lock_waiter_t **p = &(lock->waiters);
	    upcri_lock_waiter_t *entry;
            if ((entry = lock->freelist)) {
              lock->freelist = entry->next;
            } else {
              entry = upcri_checkmalloc(sizeof(upcri_lock_waiter_t));
            }
	    entry->thread_id = threadid;
	    entry->next = NULL;
	    while (*p) /* TODO: under high contention, we really want a tail ptr here */
		p = &((*p)->next);
	    *p = entry;
            *replystate = UPCRI_LOCKSTATE_WAITING;
	} else {
	    *replystate = UPCRI_LOCKSTATE_FAILED;
	}
    } else {
	upcri_assert(lock->waiters == NULL);
	lock->islocked = 1;
        #if UPCR_LOCK_DEBUG
	  lock->owner_thread_id = threadid;
        #endif
	*replystate = UPCRI_LOCKSTATE_GRANTED;
    }
    gasnet_hsl_unlock(&(lock->hsl));
  #endif
}

void upcri_SRQ_lock(gasnet_token_t token, gasnet_handlerarg_t threadid,
	        gasnet_handlerarg_t isblocking, void *_lock) {
    upclock_state_t replystate;
    upcri_doSRQ_lock(threadid, isblocking, (upcri_lock_t *)_lock, (int*)&replystate);
    if (replystate != UPCRI_LOCKSTATE_WAITING) {
	UPCRI_AM_CALL(UPCRI_SHORT_REPLY(3, 3,
		     (token, UPCRI_HANDLER_ID(upcri_SRP_setlockstate),
		      threadid, replystate, 0)));
    }
}


/* -------------------------------------------------------------------------- */
GASNETT_INLINE(upcri_doSRQ_unlock)
void upcri_doSRQ_unlock(gasnet_handlerarg_t threadid, 
		      upcri_lock_t *lock, int *replystate, int *replyarg) {
  #if !UPCRI_AMLOCKS
    *replystate = *replyarg = 0; /* suppress uninitialized var warnings */
    upcri_err("Bad call to upcri_doSRQ_unlock");
  #else
    #if UPCR_LOCK_DEBUG
      /* this check must precede acquiring the hsl, which may already have been destroyed */
      if (lock->pending_free) /* note - this *only* catches the case where the free is still pending */
        upcri_err("upc_unlock() was called on a upc_lock_t that was freed using upc_lock_free()");
    #endif
    gasnet_hsl_lock(&(lock->hsl));
    upcri_assert(lock->islocked);
    #if UPCR_LOCK_DEBUG
      upcri_assert(lock->owner_thread_id == threadid);
    #endif
    if (lock->waiters) {	/* someone waiting - handoff ownership */
	upcri_lock_waiter_t *entry = lock->waiters;
	lock->waiters = entry->next;
        entry->next = lock->freelist;
        lock->freelist = entry;
	*replyarg = entry->thread_id;
	*replystate = UPCRI_LOCKSTATE_HANDOFF;
        #if UPCR_LOCK_DEBUG
	  lock->owner_thread_id = *replyarg;
        #endif
    } else {			/* nobody waiting - unlock */
	lock->islocked = 0;
	*replystate = UPCRI_LOCKSTATE_DONE;
    }
    gasnet_hsl_unlock(&(lock->hsl));
  #endif
}

void upcri_SRQ_unlock(gasnet_token_t token, gasnet_handlerarg_t threadid, 
		      void *_lock) {
    int replystate, replyarg;
    upcri_doSRQ_unlock(threadid, (upcri_lock_t *) _lock, &replystate, &replyarg);
    UPCRI_AM_CALL(UPCRI_SHORT_REPLY(3, 3,
		  (token, UPCRI_HANDLER_ID(upcri_SRP_setlockstate),
		   threadid, replystate, replyarg)));
}


/* -------------------------------------------------------------------------- */
#define upcri_lock(lockptr, isblocking) \
       _upcri_lock(lockptr, isblocking UPCRI_PT_PASS)

GASNETT_INLINE(_upcri_lock)
int _upcri_lock(upcr_shared_ptr_t lockptr, int isblocking UPCRI_PT_ARG) {
    gasnet_node_t node = upcri_shared_nodeof(lockptr);
    upcri_lock_t *lockaddr = upcri_shared_to_remote(lockptr);
    upcri_lockinfo_t * const li = &(upcri_auxdata()->lock_info);
    int success;
    #if GASNET_STATS
      gasnett_tick_t starttime = gasnett_ticks_now();
    #endif

    #if UPCR_LOCK_DEBUG
      upcri_lock_check(li, lockptr);
    #endif

  #ifdef UPCRI_LOCKC_LOCK
    UPCRI_LOCKC_LOCK(li, node, lockaddr, isblocking, success);
  #else
    if (node == gasnet_mynode()) {
      upcri_doSRQ_lock(upcr_mythread(), isblocking, lockaddr, (int*)&li->upclock_state);
      if (!isblocking && li->upclock_state == UPCRI_LOCKSTATE_FAILED) {
	  /* ensure progress: unlock may be waiting in AM queue */
	  upcr_poll();
      }
    } else {
      li->upclock_state = (int) UPCRI_LOCKSTATE_WAITING;

      /* this memory barrier prevents a race against GASNet handler on upclock_state,
         and provides the wmb half of the memory barrier semantics required by upc_lock() 
         not required in local case - synchronous HSL critical section takes care of it
       */
      gasnett_local_wmb();

      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(3, 4, 
		  (node, UPCRI_HANDLER_ID(upcri_SRQ_lock),
		   upcr_mythread(), isblocking, UPCRI_SEND_PTR(lockaddr))));
    }

    /* GASNET_BLOCKUNTIL provides rmb for upc_lock */
    GASNET_BLOCKUNTIL(li->upclock_state != (int) UPCRI_LOCKSTATE_WAITING);
    {
	upclock_state_t state = (upclock_state_t) li->upclock_state;

	if (isblocking)
	    upcri_assert(state == UPCRI_LOCKSTATE_GRANTED);
	else
	    upcri_assert(state == UPCRI_LOCKSTATE_GRANTED
		   || state == UPCRI_LOCKSTATE_FAILED);
        
        success = (state == UPCRI_LOCKSTATE_GRANTED);
    }
  #endif

  #if GASNET_STATS
    li->waittime += (gasnett_ticks_now() - starttime);
  #endif
  if (success) {
    #if UPCR_LOCK_DEBUG
      upcri_lock_link(li, lockptr);
    #endif
    #if GASNET_STATS
      if (node == gasnet_mynode()) (li->locklocal_cnt)++;
      else (li->lockremote_cnt)++;
    #endif
    return 1;
  } else {
    #if GASNET_STATS
      if (node == gasnet_mynode()) (li->lockfaillocal_cnt)++;
      else (li->lockfailremote_cnt)++;
    #endif
    return 0;
  }
}

void _upcr_lock(upcr_shared_ptr_t lockptr UPCRI_PT_ARG) {
  #ifdef GASNET_TRACE
    char ptrstr[UPCRI_DUMP_MIN_LENGTH];
    upcri_dump_shared(lockptr, ptrstr, UPCRI_DUMP_MIN_LENGTH);
    UPCRI_TRACE_PRINTF(("LOCK upc_lock(%s)", ptrstr));
  #endif
  #define UPCRI_PEVT_ARGS , &lockptr
  upcri_pevt_start(GASP_UPC_LOCK);
    upcri_lock(lockptr, 1);
  upcri_pevt_end(GASP_UPC_LOCK);
  #undef UPCRI_PEVT_ARGS
}

int _upcr_lock_attempt(upcr_shared_ptr_t lockptr UPCRI_PT_ARG) {
  int retval;
  #ifdef GASNET_TRACE
    char ptrstr[UPCRI_DUMP_MIN_LENGTH];
    upcri_dump_shared(lockptr, ptrstr, UPCRI_DUMP_MIN_LENGTH);
    UPCRI_TRACE_PRINTF(("LOCK upc_lock_attempt(%s)", ptrstr));
  #endif
  #define UPCRI_PEVT_ARGS , &lockptr
  upcri_pevt_start(GASP_UPC_LOCK_ATTEMPT);
  #undef UPCRI_PEVT_ARGS

  retval = upcri_lock(lockptr, 0);

  #define UPCRI_PEVT_ARGS , &lockptr, retval
  upcri_pevt_end(GASP_UPC_LOCK_ATTEMPT);
  #undef UPCRI_PEVT_ARGS

  return retval;
}

/* -------------------------------------------------------------------------- */
void _upcr_unlock(upcr_shared_ptr_t lockptr UPCRI_PT_ARG) {
  #define UPCRI_PEVT_ARGS , &lockptr
  upcri_pevt_start(GASP_UPC_UNLOCK);
  {
    gasnet_node_t node = upcri_shared_nodeof(lockptr);
    upcri_lock_t *lockaddr = upcri_shared_to_remote(lockptr);
    upcri_lockinfo_t * const li = &(upcri_auxdata()->lock_info);
  #ifdef GASNET_TRACE
    char ptrstr[UPCRI_DUMP_MIN_LENGTH];
    upcri_dump_shared(lockptr, ptrstr, UPCRI_DUMP_MIN_LENGTH);
    UPCRI_TRACE_PRINTF(("LOCK upc_unlock(%s)", ptrstr));
  #endif

  #if UPCR_LOCK_DEBUG
    upcri_lock_unlink(li, lockptr, 0);
  #endif

  #if GASNET_STATS
    if (node == gasnet_mynode()) (li->unlocklocal_cnt)++;
    else (li->unlockremote_cnt)++;
  #endif

  #ifdef UPCRI_LOCKC_UNLOCK
    UPCRI_LOCKC_UNLOCK(li, node, lockaddr);
  #else
    if (node == gasnet_mynode()) {
      upcr_poll(); /* bug 1531 - for fairness, be sure we notice incoming lock requests */
      upcri_doSRQ_unlock(upcr_mythread(), lockaddr, 
            (int*)&li->upclock_state, (int*)&li->upclock_handoffarg);
    } else {
      li->upclock_state = (int) UPCRI_LOCKSTATE_WAITING;
      /* this memory barrier prevents a race against GASNet handler on upclock_state,
         and provides the wmb half of the memory barrier semantics required by upc_unlock()
         not required in local case - synchronous HSL critical section takes care of it
       */
      gasnett_local_wmb();

      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(2, 3, 
		  (node, UPCRI_HANDLER_ID(upcri_SRQ_unlock),
		   upcr_mythread(), UPCRI_SEND_PTR(lockaddr))));
    }

    /* GASNET_BLOCKUNTIL provides rmb for upc_unlock */
    GASNET_BLOCKUNTIL(li->upclock_state != (int) UPCRI_LOCKSTATE_WAITING);
    {
	upclock_state_t state = (upclock_state_t) li->upclock_state;

	upcri_assert(state == UPCRI_LOCKSTATE_DONE
	       || state == UPCRI_LOCKSTATE_HANDOFF);

	if (state == UPCRI_LOCKSTATE_HANDOFF) {	
	    /* tell the next locker that he acquired */
	    int newthreadid = li->upclock_handoffarg;
	    gasnet_node_t newnode = upcri_thread_to_node(newthreadid);

            if (newnode == gasnet_mynode()) {
               upcri_doSRP_setlockstate(newthreadid, UPCRI_LOCKSTATE_GRANTED, 0);
            } else {
	       UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(3, 3,
			(newnode, UPCRI_HANDLER_ID(upcri_SRP_setlockstate),
			 newthreadid, UPCRI_LOCKSTATE_GRANTED, 0)));
            }
	}
    }
  #endif
  }
  upcri_pevt_end(GASP_UPC_UNLOCK);
  #undef UPCRI_PEVT_ARGS
}
