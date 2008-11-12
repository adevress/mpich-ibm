
#include <upcr_internal.h>

/******************
 * Global variables 
 ******************/

upcr_thread_t    upcri_threads = 0;
upcr_thread_t	 upcri_mynode = 0;

#if UPCRI_UPC_PTHREADS
/* thread local data key */
GASNETT_THREADKEY_DEFINE(upcri_pthread_key);
gasnet_node_t	*upcri_thread2node;     /* maps UPC threads to nodes    */
upcri_pthread_t *upcri_thread2pthread;  /* UPC thread->local pthread num */
upcri_pthread_t *upcri_node1stthread;   /* # of first UPC thread on node */
upcri_pthread_t *upcri_node2pthreads;   /* # of pthreads per node	*/
int		 upcri_mypthread_cnt;	/* # of pthreads on this node   */
upcri_pthreadinfo_t **upcri_pthreadtoinfo;  /* map of all pthread infos */
#else
uintptr_t        upcri_myregion_single; /* start of this thread's shmem  */
upcri_auxdata_t  _upcri_auxdata;
#endif

#if ! UPCRI_SINGLE_ALIGNED_REGIONS
uintptr_t	*upcri_thread2region;
uintptr_t       *upcri_thread2local;
#endif

uintptr_t upcri_perthread_segsize;	/* Size of each UPC thread's share of
					   shared memory segment */
int upcri_polite_wait = 0;

uintptr_t upcri_stacksz = 0;

#if UPCR_DEBUG
/* stack check support - warn the user if their stack grows too large,
   because on many pthread implementations stack overflow will 
   lead to silent corruption of random heap locations (a problem we've
   encountered as mysterious crashes in several deployed UPC codes)
 */
uintptr_t upcri_stacksz_threshhold = (uintptr_t)(0.95*UPCRI_STACK_DEFAULT);
extern void upcri_stack_check(upcri_stackinfo_t *si, upcr_thread_t mythreadid, const char *filename, int linenum) {
  static int already_warned = 0;
  #if UPCRI_SUPPORT_PTHREADS
    static gasnett_mutex_t warn_lock = GASNETT_MUTEX_INITIALIZER;
  #endif
  char x;
  #ifdef UPCRI_STACK_GROWS_UP
    if (&x > si->hot) {
      uintptr_t size = &x - si->cold;
  #else
    if (&x < si->hot) {
      uintptr_t size = si->cold - &x;
  #endif
      si->hot = &x;
      if (size > upcri_stacksz_threshhold && !already_warned) {
        #if UPCRI_SUPPORT_PTHREADS
          gasnett_mutex_lock(&warn_lock);
        #endif
        if (!already_warned) {
          upcri_warn("The program stack on thread %i has grown beyond %llu KB at %s:%i.\n"
            " Huge program stacks are non-portable and may lead to stack overflow and random crashes.\n"
            " Large stack growth is usually caused by huge arrays declared as automatic (ie non-static) local variables, "
            "or overly-deep function call recursion.",
            mythreadid, (unsigned long long)(size/1024), filename, linenum);
          already_warned = 1;
        }
        #if UPCRI_SUPPORT_PTHREADS
          gasnett_mutex_unlock(&warn_lock);
        #endif
      }
    }
}
#endif

static uint64_t *upcri_hostname_checksums;
void upcri_thread_distance_init() {
  const char *hostname = gasnett_gethostname();
  uint64_t mysum;
  mysum = gasnett_checksum(hostname, strlen(hostname));
  upcri_hostname_checksums = UPCRI_XMALLOC(uint64_t, upcr_threads());
  gasnet_coll_gather_all(GASNET_TEAM_ALL, upcri_hostname_checksums, &mysum, sizeof(uint64_t),
                          GASNET_COLL_LOCAL | GASNET_COLL_IN_MYSYNC | GASNET_COLL_OUT_MYSYNC);
}

unsigned int _bupc_thread_distance(int x, int y UPCRI_PT_ARG) {
  if_pf (x < 0 || x >= upcr_threads() || y < 0 || y >= upcr_threads())
    upcri_err("bupc_thread_distance called with out-of-bounds thread values: %i,%i (must be in 0..%i)",
              x, y, upcr_threads()-1);
  if (x == y) return BUPC_THREADS_SAME;
  else if (upcri_thread_to_node(x) == upcri_thread_to_node(y)) {
    /* TODO: add differentiation based on CPU affinity (eg hyperthreaded CPUs) 
     * and network topology 
     */
    return BUPC_THREADS_VERYNEAR; 
  } else if (upcri_hostname_checksums[x] == upcri_hostname_checksums[y]) {
    return BUPC_THREADS_NEAR; 
  } else return BUPC_THREADS_VERYFAR;
}

/* Arbitrary thread-local variables that the translator needs 
 *  - use regular global variable if pthreads not used: otherwise this is
 *    needed to store the initial value. 
 */
#undef  UPCR_TRANSLATOR_TLD
#define UPCR_TRANSLATOR_TLD(type, name, initval)  type name = initval;
#include <upcr_translator_tld.h>

