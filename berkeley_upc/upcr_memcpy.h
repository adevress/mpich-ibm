/*
 * Non-blocking memcpy extensions, a Berkeley UPC extension - 
 * see: Bonachea, D. "Proposal for Extending the UPC Memory Copy Library Functions"
 *  available at http://upc.lbl.gov/publications/
 *
 * $Id: upcr_memcpy.h,v 1.12 2006/09/26 04:34:26 bonachea Exp $
 * Dan Bonachea <bonachea@cs.berkeley.edu>
 */

#ifndef UPCR_MEMCPY_H
#define UPCR_MEMCPY_H
/*---------------------------------------------------------------------------------*/
typedef struct _upcri_eop {
  upcr_handle_t handle;
  struct _upcri_eop *next;
} upcri_eop;
#define upcri_eop_metadata(peop) ((char *)(((upcri_eop*)(peop))+1))

typedef struct _upcri_eop *bupc_handle_t;

#ifndef BUPC_COMPLETE_HANDLE
  #define BUPC_COMPLETE_HANDLE ((bupc_handle_t)NULL)
#endif

#if UPCR_DEBUG
  #define UPCRI_DEBUG_DO(code) do { code; } while(0)
#else
  #define UPCRI_DEBUG_DO(code) 
#endif

#ifndef BUPC_SG_DESIGN_A
#define BUPC_SG_DESIGN_A      1
#endif
#ifndef BUPC_SG_DESIGN_B
#define BUPC_SG_DESIGN_B      1
#endif
#ifndef BUPC_STRIDED_DESIGN_A
#define BUPC_STRIDED_DESIGN_A 1
#endif
#ifndef BUPC_STRIDED_DESIGN_B
#define BUPC_STRIDED_DESIGN_B 1
#endif

/*---------------------------------------------------------------------------------*/
extern void _bupc_waitsync(bupc_handle_t h UPCRI_PT_ARG);
extern int _bupc_trysync(bupc_handle_t h UPCRI_PT_ARG);
extern void _bupc_waitsync_all (bupc_handle_t *_ph, size_t _n UPCRI_PT_ARG);
extern int  _bupc_trysync_all  (bupc_handle_t *_ph, size_t _n UPCRI_PT_ARG);
extern void _bupc_waitsync_some(bupc_handle_t *_ph, size_t _n UPCRI_PT_ARG);
extern int  _bupc_trysync_some (bupc_handle_t *_ph, size_t _n UPCRI_PT_ARG);

extern bupc_handle_t _bupc_memcpy_async(upcr_shared_ptr_t dst, upcr_shared_ptr_t src, size_t n UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
extern bupc_handle_t _bupc_memget_async(void *dst, upcr_shared_ptr_t src, size_t n UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
extern bupc_handle_t _bupc_memput_async(upcr_shared_ptr_t dst, const void *src, size_t n UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
extern bupc_handle_t _bupc_memset_async(upcr_shared_ptr_t dst, int c, size_t n UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;

#define _bupc_waitsynci() upcr_wait_syncnbi_all()
#define _bupc_trysynci()  upcr_try_syncnbi_all()
#define _bupc_memget_asynci   upcr_nbi_memget
#define _bupc_memput_asynci   upcr_nbi_memput
#define _bupc_memcpy_asynci   upcr_nbi_memcpy
#define _bupc_memset_asynci   upcr_nbi_memset

#define _bupc_begin_accessregion() upcr_begin_nbi_accessregion()
extern bupc_handle_t _bupc_end_accessregion(UPCRI_PT_ARG_ALONE);

#if !UPCRI_LIBWRAP 
  #define bupc_waitsync(h) \
       (upcri_srcpos(), _bupc_waitsync(h UPCRI_PT_PASS))
  #define bupc_trysync(h) \
       (upcri_srcpos(), _bupc_trysync(h UPCRI_PT_PASS))

  #define bupc_waitsync_all(ph,n) \
       (upcri_srcpos(), _bupc_waitsync_all((ph),(n) UPCRI_PT_PASS))
  #define bupc_trysync_all(ph,n) \
       (upcri_srcpos(), _bupc_trysync_all((ph),(n) UPCRI_PT_PASS))
  #define bupc_waitsync_some(ph,n) \
       (upcri_srcpos(), _bupc_waitsync_some((ph),(n) UPCRI_PT_PASS))
  #define bupc_trysync_some(ph,n) \
       (upcri_srcpos(), _bupc_trysync_some((ph),(n) UPCRI_PT_PASS))

  #define bupc_memcpy_async(dst,src,n) \
       (upcri_srcpos(), _bupc_memcpy_async(dst,src,n UPCRI_PT_PASS))
  #define bupc_memget_async(dst,src,n) \
       (upcri_srcpos(), _bupc_memget_async(dst,src,n UPCRI_PT_PASS))
  #define bupc_memput_async(dst,src,n) \
       (upcri_srcpos(), _bupc_memput_async(dst,src,n UPCRI_PT_PASS))
  #define bupc_memset_async(dst,c,n) \
       (upcri_srcpos(), _bupc_memset_async(dst,c,n UPCRI_PT_PASS))

  #define bupc_waitsynci     _bupc_waitsynci
  #define bupc_trysynci      _bupc_trysynci
  #define bupc_memget_asynci _bupc_memget_asynci
  #define bupc_memput_asynci _bupc_memput_asynci
  #define bupc_memcpy_asynci _bupc_memcpy_asynci
  #define bupc_memset_asynci _bupc_memset_asynci

  #define bupc_begin_accessregion _bupc_begin_accessregion
  #define bupc_end_accessregion() \
       (upcri_srcpos(), _bupc_end_accessregion(UPCRI_PT_PASS_ALONE))

#elif _IN_UPCR_GLOBFILES_C
  #define bupc_waitsync     _bupc_waitsync
  #define bupc_trysync      _bupc_trysync

  #define bupc_waitsync_all  _bupc_waitsync_all
  #define bupc_trysync_all   _bupc_trysync_all
  #define bupc_waitsync_some _bupc_waitsync_some
  #define bupc_trysync_some  _bupc_trysync_some

  #define bupc_memcpy_async _bupc_memcpy_async
  #define bupc_memget_async _bupc_memget_async
  #define bupc_memput_async _bupc_memput_async
  #define bupc_memset_async _bupc_memset_async

  #define bupc_waitsynci     _bupc_waitsynci
  #define bupc_trysynci      _bupc_trysynci
  #define bupc_memget_asynci _bupc_memget_asynci
  #define bupc_memput_asynci _bupc_memput_asynci
  #define bupc_memcpy_asynci _bupc_memcpy_asynci
  #define bupc_memset_asynci _bupc_memset_asynci

  #define bupc_begin_accessregion _bupc_begin_accessregion
  #define bupc_end_accessregion   _bupc_end_accessregion

#endif
/*---------------------------------------------------------------------------------*/

#if BUPC_SG_DESIGN_A
  typedef struct bupc_smemvec_S {
    upcr_shared_ptr_t addr;
    size_t len;
  } bupc_smemvec_t;
  typedef gasnet_memvec_t bupc_pmemvec_t;

  /* upcr needs the struct definitions above in order to compile upc_memcpy.c as a library
     the translator spits out its own struct definitions for bupc_{p,s}memvec based on upc.h
     this goop is used to cast between the two struct types, which should be bitwise identical
   */
  #define UPCR_FIX_SMEMVECP(arg) (upcri_assert(sizeof(*(arg)) == sizeof(bupc_smemvec_t)), (bupc_smemvec_t*)(arg))
  #define UPCR_FIX_PMEMVECP(arg) (upcri_assert(sizeof(*(arg)) == sizeof(bupc_pmemvec_t)), (bupc_pmemvec_t*)(arg))
  #define UPCR_FIX_sMEMVECP UPCR_FIX_SMEMVECP
  #define UPCR_FIX_pMEMVECP UPCR_FIX_PMEMVECP

  #if UPCR_DEBUG
    GASNETT_INLINE(_upcri_smemvec_check)
    uintptr_t _upcri_smemvec_check(size_t count, bupc_smemvec_t const list[], char const *file, int line) {
      uintptr_t retval = 0;
      int i;
      for (i=0; i < count; i++) {
        if (list[i].len > 0) {
          upcri_checkvalid_shared(list[i].addr);
          if (upcr_isnull_shared(list[i].addr)) 
            upcri_err("NULL address in bupc_smemvec_t list, at entry %i, at %s:%i", i, file, line);
          retval += list[i].len;
        }
      }
      return retval;
    }
    GASNETT_INLINE(_upcri_pmemvec_check)
    uintptr_t _upcri_pmemvec_check(size_t count, bupc_pmemvec_t const list[], char const *file, int line) {
      uintptr_t retval = 0;
      int i;
      for (i=0; i < count; i++) {
        if (list[i].len > 0) {
          if (list[i].addr == NULL) 
            upcri_err("NULL address in bupc_pmemvec_t list, at entry %i, at %s:%i", i, file, line);
          retval += list[i].len;
        }
      }
      return retval;
    }
    #define upcri_check_vlists(dstps, dstcount, dstlist, srcps, srccount, srclist)                            \
      (_upcri_##dstps##memvec_check(dstcount, UPCR_FIX_##dstps##MEMVECP(dstlist), __FILE__, __LINE__) ==      \
       _upcri_##srcps##memvec_check(srccount, UPCR_FIX_##srcps##MEMVECP(srclist), __FILE__, __LINE__) ?       \
       (void)0 : (void)upcri_err("dstlist and srclist do not specify the same total amount of data at %s:%i", \
         __FILE__, __LINE__))
  #else
    #define upcri_check_vlists(dstps, dstcount, dstlist, srcps, srccount, srclist) ((void)0)
  #endif

  extern bupc_handle_t _bupc_memcpy_vlist_async(size_t dstcount, bupc_smemvec_t const dstlist[], 
                                     size_t srccount, bupc_smemvec_t const srclist[] UPCRI_PT_ARG) 
                                     GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memcpy_vlist_async(dstcount,dstlist,srccount,srclist)                 \
         (upcri_srcpos(), upcri_check_vlists(s,dstcount,dstlist,s,srccount,srclist), \
          _bupc_memcpy_vlist_async(dstcount,UPCR_FIX_SMEMVECP(dstlist),srccount,UPCR_FIX_SMEMVECP(srclist) UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memput_vlist_async(size_t dstcount, bupc_smemvec_t const dstlist[], 
                                     size_t srccount, bupc_pmemvec_t const srclist[] UPCRI_PT_ARG) 
                                     GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memput_vlist_async(dstcount,dstlist,srccount,srclist)                 \
         (upcri_srcpos(), upcri_check_vlists(s,dstcount,dstlist,p,srccount,srclist), \
          _bupc_memput_vlist_async(dstcount,UPCR_FIX_SMEMVECP(dstlist),srccount,UPCR_FIX_PMEMVECP(srclist) UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memget_vlist_async(size_t dstcount, bupc_pmemvec_t const dstlist[], 
                                     size_t srccount, bupc_smemvec_t const srclist[] UPCRI_PT_ARG) 
                                     GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memget_vlist_async(dstcount,dstlist,srccount,srclist)                 \
         (upcri_srcpos(), upcri_check_vlists(p,dstcount,dstlist,s,srccount,srclist), \
          _bupc_memget_vlist_async(dstcount,UPCR_FIX_PMEMVECP(dstlist),srccount,UPCR_FIX_SMEMVECP(srclist) UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memcpy_vlist)
  void _bupc_memcpy_vlist(size_t dstcount, bupc_smemvec_t const dstlist[], 
                        size_t srccount, bupc_smemvec_t const srclist[] UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memcpy_vlist_async(dstcount, dstlist, srccount, srclist UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memcpy_vlist(dstcount,dstlist,srccount,srclist)                       \
         (upcri_srcpos(), upcri_check_vlists(s,dstcount,dstlist,s,srccount,srclist), \
          _bupc_memcpy_vlist(dstcount,UPCR_FIX_SMEMVECP(dstlist),srccount,UPCR_FIX_SMEMVECP(srclist) UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memput_vlist)
  void _bupc_memput_vlist(size_t dstcount, bupc_smemvec_t const dstlist[], 
                       size_t srccount, bupc_pmemvec_t const srclist[] UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memput_vlist_async(dstcount, dstlist, srccount, srclist UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memput_vlist(dstcount,dstlist,srccount,srclist)                       \
         (upcri_srcpos(), upcri_check_vlists(s,dstcount,dstlist,p,srccount,srclist), \
          _bupc_memput_vlist(dstcount,UPCR_FIX_SMEMVECP(dstlist),srccount,UPCR_FIX_PMEMVECP(srclist) UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memget_vlist)
  void _bupc_memget_vlist(size_t dstcount, bupc_pmemvec_t const dstlist[], 
                       size_t srccount, bupc_smemvec_t const srclist[] UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memget_vlist_async(dstcount, dstlist, srccount, srclist UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memget_vlist(dstcount,dstlist,srccount,srclist)                       \
         (upcri_srcpos(), upcri_check_vlists(p,dstcount,dstlist,s,srccount,srclist), \
          _bupc_memget_vlist(dstcount,UPCR_FIX_PMEMVECP(dstlist),srccount,UPCR_FIX_SMEMVECP(srclist) UPCRI_PT_PASS))
#endif

#if BUPC_SG_DESIGN_B
  #if UPCR_DEBUG
    GASNETT_INLINE(_upcri_slist_check)
    uintptr_t _upcri_slist_check(size_t count, upcr_shared_ptr_t const *list, size_t len, char const *file, int line) {
      int i;
      if (len == 0) upcri_err("illegal zero length at at %s:%i", file, line);
      for (i=0; i < count; i++) {
        upcri_checkvalid_shared(list[i]);
        if (upcr_isnull_shared(list[i])) 
          upcri_err("NULL address in address list, at entry %i, at %s:%i", i, file, line);
      }
      return ((uintptr_t)count)*len;
    }
    GASNETT_INLINE(_upcri_plist_check)
    uintptr_t _upcri_plist_check(size_t count, const void * const *list, size_t len, char const *file, int line) {
      int i;
      if (len == 0) upcri_err("illegal zero length at at %s:%i", file, line);
      for (i=0; i < count; i++) {
        if (list[i] == NULL) 
          upcri_err("NULL address in address list, at entry %i, at %s:%i", i, file, line);
      }
      return ((uintptr_t)count)*len;
    }
    #define upcri_check_ilists(dstps, dstcount, dstlist, dstlen, srcps, srccount, srclist, srclen)            \
      (_upcri_##dstps##list_check(dstcount, dstlist, dstlen, __FILE__, __LINE__) ==                           \
       _upcri_##srcps##list_check(srccount, srclist, srclen, __FILE__, __LINE__) ?                            \
       (void)0 : (void)upcri_err("dstlist and srclist do not specify the same total amount of data at %s:%i", \
         __FILE__, __LINE__))
  #else
    #define upcri_check_ilists(dstps, dstcount, dstlist, dstlen, srcps, srccount, srclist, srclen) ((void)0)
  #endif

  extern bupc_handle_t _bupc_memcpy_ilist_async(size_t dstcount, upcr_shared_ptr_t const dstlist[], 
                                       size_t dstlen,
                                       size_t srccount, upcr_shared_ptr_t const srclist[], 
                                       size_t srclen UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memcpy_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen)                 \
         (upcri_srcpos(), upcri_check_ilists(s,dstcount,dstlist,dstlen,s,srccount,srclist,srclen), \
          _bupc_memcpy_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memput_ilist_async(size_t dstcount, upcr_shared_ptr_t const dstlist[], 
                                       size_t dstlen,
                                       size_t srccount,        const void * const srclist[], 
                                       size_t srclen UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memput_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen)                                \
         (upcri_srcpos(), upcri_check_ilists(s,dstcount,dstlist,dstlen,p,srccount,(const void **)srclist,srclen), \
          _bupc_memput_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memget_ilist_async(size_t dstcount,              void * const dstlist[], 
                                       size_t dstlen, 
                                       size_t srccount, upcr_shared_ptr_t const srclist[], 
                                       size_t srclen UPCRI_PT_ARG) GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memget_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen)                                \
         (upcri_srcpos(), upcri_check_ilists(p,dstcount,(const void **)dstlist,dstlen,s,srccount,srclist,srclen), \
          _bupc_memget_ilist_async(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))


  GASNETT_INLINE(_bupc_memcpy_ilist)
  void _bupc_memcpy_ilist(size_t dstcount, upcr_shared_ptr_t const dstlist[], size_t dstlen,
                        size_t srccount, upcr_shared_ptr_t const srclist[], size_t srclen UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memcpy_ilist_async(dstcount, dstlist, dstlen, srccount, srclist, srclen UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memcpy_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen)                       \
         (upcri_srcpos(), upcri_check_ilists(s,dstcount,dstlist,dstlen,s,srccount,srclist,srclen), \
          _bupc_memcpy_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memput_ilist)
  void _bupc_memput_ilist(size_t dstcount, upcr_shared_ptr_t const dstlist[], size_t dstlen,
                        size_t srccount,        const void * const srclist[], size_t srclen UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memput_ilist_async(dstcount, dstlist, dstlen, srccount, srclist, srclen UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memput_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen)                       \
         (upcri_srcpos(), upcri_check_ilists(s,dstcount,dstlist,dstlen,p,srccount,srclist,srclen), \
          _bupc_memput_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memget_ilist)
  void _bupc_memget_ilist(size_t dstcount,              void * const dstlist[], size_t dstlen, 
                        size_t srccount, upcr_shared_ptr_t const srclist[], size_t srclen UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memget_ilist_async(dstcount, dstlist, dstlen, srccount, srclist, srclen UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memget_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen)                       \
         (upcri_srcpos(), upcri_check_ilists(p,dstcount,dstlist,dstlen,s,srccount,srclist,srclen), \
          _bupc_memget_ilist(dstcount,dstlist,dstlen,srccount,srclist,srclen UPCRI_PT_PASS))
#endif
/*---------------------------------------------------------------------------------*/
#if BUPC_STRIDED_DESIGN_A
  #if UPCR_DEBUG
    GASNETT_INLINE(_upcri_scheck_fstrided)
    size_t _upcri_scheck_fstrided(upcr_shared_ptr_t addr, size_t chunklen, size_t chunkstride, size_t chunkcount, char const *file, int line) {
      size_t sz = chunklen*chunkcount;
      upcri_checkvalid_shared(addr);
      if (sz > 0) {
        if (chunkstride < chunklen)
          upcri_err("illegal chunkstride=%i < chunklen=%i at %s:%i", (int)chunkstride, (int)chunklen, file, line);
        if (upcr_isnull_shared(addr)) 
          upcri_err("illegal NULL address at %s:%i", file, line);
      }
      return sz;
    }
    GASNETT_INLINE(_upcri_pcheck_fstrided)
    size_t _upcri_pcheck_fstrided(const void *addr, size_t chunklen, size_t chunkstride, size_t chunkcount, char const *file, int line) {
      size_t sz = chunklen*chunkcount;
      if (sz > 0) {
        if (chunkstride < chunklen)
          upcri_err("illegal chunkstride=%i < chunklen=%i at %s:%i", (int)chunkstride, (int)chunklen, file, line);
        if (addr == NULL) 
          upcri_err("illegal NULL address at %s:%i", file, line);
      }
      return sz;
    }
    #define upcri_check_fstrided(dstps, dstaddr, dstchunklen, dstchunkstride, dstchunkcount, srcps, srcaddr, srcchunklen, srcchunkstride, srcchunkcount) \
      (_upcri_##dstps##check_fstrided(dstaddr, dstchunklen, dstchunkstride, dstchunkcount, __FILE__, __LINE__) ==                                        \
       _upcri_##srcps##check_fstrided(srcaddr, srcchunklen, srcchunkstride, srcchunkcount, __FILE__, __LINE__) ?                                         \
       (void)0 : (void)upcri_err("dst and src do not specify the same total amount of data at %s:%i",                                                   \
         __FILE__, __LINE__))
  #else
    #define upcri_check_fstrided(dstps, dstaddr, dstchunklen, dstchunkstride, dstchunkcount, srcps, srcaddr, srcchunklen, srcchunkstride, srcchunkcount) ((void)0)
  #endif

  extern bupc_handle_t _bupc_memcpy_fstrided_async(upcr_shared_ptr_t dstaddr,  size_t dstchunklen, 
                                          size_t dstchunkstride, size_t dstchunkcount,
                                          upcr_shared_ptr_t srcaddr,  size_t srcchunklen, 
                                          size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG) 
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memcpy_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                 \
         (upcri_srcpos(), upcri_check_fstrided(s,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,s,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memcpy_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memput_fstrided_async(upcr_shared_ptr_t dstaddr,  size_t dstchunklen, 
                                          size_t dstchunkstride, size_t dstchunkcount,
                                                 void *srcaddr,  size_t srcchunklen, 
                                          size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG)
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memput_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                 \
         (upcri_srcpos(), upcri_check_fstrided(s,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,p,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memput_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memget_fstrided_async(       void *dstaddr,  size_t dstchunklen, 
                                          size_t dstchunkstride, size_t dstchunkcount,
                                          upcr_shared_ptr_t srcaddr,  size_t srcchunklen, 
                                          size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG)
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memget_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                 \
         (upcri_srcpos(), upcri_check_fstrided(p,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,s,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memget_fstrided_async(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memcpy_fstrided)
  void _bupc_memcpy_fstrided(upcr_shared_ptr_t dstaddr,  size_t dstchunklen, 
                           size_t dstchunkstride, size_t dstchunkcount,
                           upcr_shared_ptr_t srcaddr,  size_t srcchunklen, 
                           size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memcpy_fstrided_async(dstaddr, dstchunklen, dstchunkstride, dstchunkcount,
                                            srcaddr, srcchunklen, srcchunkstride, srcchunkcount UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memcpy_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                       \
         (upcri_srcpos(), upcri_check_fstrided(s,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,s,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memcpy_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memput_fstrided)
  void _bupc_memput_fstrided(upcr_shared_ptr_t dstaddr,  size_t dstchunklen, 
                           size_t dstchunkstride, size_t dstchunkcount,
                                  void *srcaddr,  size_t srcchunklen, 
                           size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memput_fstrided_async(dstaddr, dstchunklen, dstchunkstride, dstchunkcount,
                                            srcaddr, srcchunklen, srcchunkstride, srcchunkcount UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memput_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                       \
         (upcri_srcpos(), upcri_check_fstrided(s,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,p,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memput_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memget_fstrided)
  void _bupc_memget_fstrided(       void *dstaddr,  size_t dstchunklen, 
                           size_t dstchunkstride, size_t dstchunkcount,
                           upcr_shared_ptr_t srcaddr,  size_t srcchunklen, 
                           size_t srcchunkstride, size_t srcchunkcount UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memget_fstrided_async(dstaddr, dstchunklen, dstchunkstride, dstchunkcount,
                                            srcaddr, srcchunklen, srcchunkstride, srcchunkcount UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memget_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount)                       \
         (upcri_srcpos(), upcri_check_fstrided(p,dstaddr,dstchunklen,dstchunkstride,dstchunkcount,s,srcaddr,srcchunklen,srcchunkstride, srcchunkcount), \
          _bupc_memget_fstrided(dstaddr,dstchunklen,dstchunkstride,dstchunkcount,srcaddr,srcchunklen,srcchunkstride,srcchunkcount UPCRI_PT_PASS))
#endif

#if BUPC_STRIDED_DESIGN_B
  #if UPCR_DEBUG
    GASNETT_INLINE(_upcri_scheck_strided)
    size_t _upcri_scheck_strided(upcr_shared_ptr_t addr, size_t const *strides, size_t const *count, size_t stridelevels, char const *file, int line) {
      int i;
      size_t sz = count[0];
      for (i=0; i < stridelevels; i++) {
        if (strides[i] < sz)
          upcri_err("illegal stride[%i]=%i at %s:%i", i, (int)strides[i], file, line);
        sz *= count[i+1];
      }
      upcri_checkvalid_shared(addr);
      if (sz > 0 && upcr_isnull_shared(addr)) 
        upcri_err("illegal NULL address at %s:%i", file, line);
      return sz;
    }
    GASNETT_INLINE(_upcri_pcheck_strided)
    size_t _upcri_pcheck_strided(const void *addr, size_t const *strides, size_t const *count, size_t stridelevels, char const *file, int line) {
      int i;
      size_t sz = count[0];
      for (i=0; i < stridelevels; i++) {
        if (strides[i] < sz)
          upcri_err("illegal stride[%i]=%i at %s:%i", i, (int)strides[i], file, line);
        sz *= count[i+1];
      }
      if (sz > 0 && addr == NULL) 
        upcri_err("illegal NULL address at %s:%i", file, line);
      return sz;
    }
    #define upcri_check_strided(dstps, dstaddr, dststrides, srcps, srcaddr, srcstrides, count, stridelevels) \
      (void)(_upcri_##dstps##check_strided(dstaddr, dststrides, count, stridelevels, __FILE__, __LINE__), \
             _upcri_##srcps##check_strided(srcaddr, srcstrides, count, stridelevels, __FILE__, __LINE__))
  #else
    #define upcri_check_strided(dstps, dstaddr, dststrides, srcps, srcaddr, srcstrides, count, stridelevels) ((void)0)
  #endif

  extern bupc_handle_t _bupc_memcpy_strided_async(upcr_shared_ptr_t dstaddr, const size_t dststrides[], 
                                          upcr_shared_ptr_t srcaddr, const size_t srcstrides[], 
                                          const size_t count[], size_t stridelevels UPCRI_PT_ARG)
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memcpy_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                \
         (upcri_srcpos(), upcri_check_strided(s,dstaddr,dststrides,s,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memcpy_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memput_strided_async(upcr_shared_ptr_t dstaddr, const size_t dststrides[], 
                                                 const void *srcaddr, const size_t srcstrides[], 
                                          const size_t count[], size_t stridelevels UPCRI_PT_ARG)
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memput_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                \
         (upcri_srcpos(), upcri_check_strided(s,dstaddr,dststrides,p,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memput_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))

  extern bupc_handle_t _bupc_memget_strided_async(             void *dstaddr, const size_t dststrides[], 
                                          upcr_shared_ptr_t srcaddr, const size_t srcstrides[], 
                                          const size_t count[], size_t stridelevels UPCRI_PT_ARG)
                                          GASNETT_WARN_UNUSED_RESULT;
  #define bupc_memget_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                \
         (upcri_srcpos(), upcri_check_strided(p,dstaddr,dststrides,s,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memget_strided_async(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memcpy_strided)
  void _bupc_memcpy_strided(upcr_shared_ptr_t dstaddr, const size_t dststrides[], 
                           upcr_shared_ptr_t srcaddr, const size_t srcstrides[], 
                           const size_t count[], size_t stridelevels UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memcpy_strided_async(dstaddr, dststrides,
                                            srcaddr, srcstrides, 
                                            count, stridelevels UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memcpy_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                      \
         (upcri_srcpos(), upcri_check_strided(s,dstaddr,dststrides,s,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memcpy_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memput_strided)
  void _bupc_memput_strided(upcr_shared_ptr_t dstaddr, const size_t dststrides[], 
                                  const void *srcaddr, const size_t srcstrides[], 
                           const size_t count[], size_t stridelevels UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memput_strided_async(dstaddr, dststrides,
                                            srcaddr, srcstrides, 
                                            count, stridelevels UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memput_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                      \
         (upcri_srcpos(), upcri_check_strided(s,dstaddr,dststrides,p,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memput_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))

  GASNETT_INLINE(_bupc_memget_strided)
  void _bupc_memget_strided(             void *dstaddr, const size_t dststrides[], 
                           upcr_shared_ptr_t srcaddr, const size_t srcstrides[], 
                           const size_t count[], size_t stridelevels UPCRI_PT_ARG) {
    _bupc_waitsync(_bupc_memget_strided_async(dstaddr, dststrides,
                                            srcaddr, srcstrides, 
                                            count, stridelevels UPCRI_PT_PASS) UPCRI_PT_PASS);
  }
  #define bupc_memget_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels)                      \
         (upcri_srcpos(), upcri_check_strided(p,dstaddr,dststrides,s,srcaddr,srcstrides,count,stridelevels), \
          _bupc_memget_strided(dstaddr,dststrides,srcaddr,srcstrides,count,stridelevels UPCRI_PT_PASS))
#endif
/*---------------------------------------------------------------------------------*/

#endif /* UPCR_MEMCPY_H */
