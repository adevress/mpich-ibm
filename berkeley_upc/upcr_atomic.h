/*   $Source: /var/local/cvs/upcr/upcr_atomic.h,v $
 *     $Date: 2006/12/20 18:46:06 $
 * $Revision: 1.15 $
 * Description:
 *  Berkeley UPC atomic extensions - types and prototypes 
 *
 * Copyright 2006, Dan Bonachea <bonachea@cs.berkeley.edu>
 */


#ifndef UPCR_ATOMIC_H
#define UPCR_ATOMIC_H

/*---------------------------------------------------------------------------------*/

/* Scalar results are written to this structure.
 * Vector results (if/when implemented) will probably be written to
 * the client's buffer (by the AM handler or the Get).
 */
struct upcri_atomic_result {
  union {
    uint64_t      _bupc_atomic_val_U64;
    uint32_t      _bupc_atomic_val_U32;
    int           _bupc_atomic_val_I;
    unsigned int  _bupc_atomic_val_UI;
    long          _bupc_atomic_val_L;
    unsigned long _bupc_atomic_val_UL;
    float         _bupc_atomic_val_F;
    double        _bupc_atomic_val_D;
  } value;
  int flag;
};

/*---------------------------------------------------------------------------------*/
/* Convenience macros */

/* Flags to apply to local atomics based on strictness */
#define _bupc_atomic_read_flags(isstrict) \
  (isstrict ? (GASNETT_ATOMIC_MB_PRE|GASNETT_ATOMIC_RMB_POST) : 0)
#define _bupc_atomic_set_flags(isstrict) \
  (isstrict ? (GASNETT_ATOMIC_WMB_PRE|GASNETT_ATOMIC_MB_POST) : 0)
#define _bupc_atomic_cas_flags(isstrict) \
  (isstrict ? (GASNETT_ATOMIC_MB_PRE|GASNETT_ATOMIC_MB_POST) : 0)

/*---------------------------------------------------------------------------------*/
/* Enum for naming types (not all implemented yet) */

enum {
	/* 64-bit integer types: */
	_BUPC_ATOMIC_TYPE_U64,
	_BUPC_ATOMIC_TYPE_I64 = _BUPC_ATOMIC_TYPE_U64,
#if SIZEOF_INT == 8
	_BUPC_ATOMIC_TYPE_I = _BUPC_ATOMIC_TYPE_U64,
	_BUPC_ATOMIC_TYPE_UI = _BUPC_ATOMIC_TYPE_U64,
#endif
#if SIZEOF_LONG == 8
	_BUPC_ATOMIC_TYPE_L = _BUPC_ATOMIC_TYPE_U64,
	_BUPC_ATOMIC_TYPE_UL = _BUPC_ATOMIC_TYPE_U64,
#endif

	/* 32-bit integer types: */
	_BUPC_ATOMIC_TYPE_U32,
	_BUPC_ATOMIC_TYPE_I32 = _BUPC_ATOMIC_TYPE_U32,
#if SIZEOF_INT == 4
	_BUPC_ATOMIC_TYPE_I = _BUPC_ATOMIC_TYPE_U32,
	_BUPC_ATOMIC_TYPE_UI = _BUPC_ATOMIC_TYPE_U32,
#endif
#if SIZEOF_LONG == 4
	_BUPC_ATOMIC_TYPE_L = _BUPC_ATOMIC_TYPE_U32,
	_BUPC_ATOMIC_TYPE_UL = _BUPC_ATOMIC_TYPE_U32,
#endif

	/* Floating point type: */
	_BUPC_ATOMIC_TYPE_F,
	_BUPC_ATOMIC_TYPE_D
};

/*---------------------------------------------------------------------------------*/
/* Tracing macros */
#if GASNET_TRACE
  #if UPCR_DEBUG
    /* Evaluation of "data" must be free of size effects */
    #define _BUPC_ATOMIC_TRACE_FMT_DATA(data,nbytes)                           \
	char _datastr[80];                                                     \
	do {                                                                   \
	    const char *_data = (const char *)(data);                          \
	    char *_p_out = _datastr;                                           \
	    int _i;                                                            \
	    strcpy(_p_out,"  data: "); _p_out += strlen(_p_out);               \
	    for (_i = 0; _i < _nbytes; _i++) {                                 \
	        sprintf(_p_out,"%02x ", (unsigned int)_data[_i]); _p_out += 3; \
	    }                                                                  \
	} while (0)
  #else
    #define _BUPC_ATOMIC_TRACE_FMT_DATA(data,nbytes) \
	const char *_datastr = ""
  #endif

  #define _BUPC_ATOMIC_TRACE_GETPUT(name,node,data,isstrict) do {    \
	int _islocal = (node == gasnet_mynode());                    \
	if (!_islocal || !upcri_trace_suppresslocal) {               \
	    int _nbytes = (int)sizeof(*(data));                      \
	    _BUPC_ATOMIC_TRACE_FMT_DATA(data,_nbytes);               \
            UPCRI_TRACE_STRICT(isstrict);                            \
            UPCRI_TRACE_PRINTF_NOPOS((#name "_ATOMIC%s: sz = %6i%s", \
				     (_islocal ? "_LOCAL" : ""),     \
				     _nbytes, _datastr));            \
	}                                                            \
    } while(0)
  #define _BUPC_ATOMIC_TRACE_PUT(node,data,isstrict) \
	_BUPC_ATOMIC_TRACE_GETPUT(PUT,node,data,isstrict)
  #define _BUPC_ATOMIC_TRACE_GET(node,data,isstrict) \
	_BUPC_ATOMIC_TRACE_GETPUT(GET,node,data,isstrict)
  #define _BUPC_ATOMIC_TRACE_RMW(node,op,result,isstrict) do {     \
	int _isstrict = (isstrict);                                \
	gasnet_node_t _node = (node);                              \
	_BUPC_ATOMIC_TRACE_GETPUT(PUT_RMW,_node,op,_isstrict);     \
	_BUPC_ATOMIC_TRACE_GETPUT(GET_RMW,_node,result,_isstrict); \
  } while(0)
#else
  #define _BUPC_ATOMIC_TRACE_GET(node,data,isstrict)      ((void)0)
  #define _BUPC_ATOMIC_TRACE_PUT(node,data,isstrict)      ((void)0)
  #define _BUPC_ATOMIC_TRACE_RMW(node,op,result,isstrict) ((void)0)
#endif

/*---------------------------------------------------------------------------------*/
/* Typeless data-movement operations */

#if 1 /* XXX: Must assume network Gets are subject to word-tearing (unless proven otherwise). */

  /* These are Get/Put for *typeless* 32- and 64-bit scalars.
   * Replacing or augmenting the AM handlers with Mediums will allow for vectors.
   */

  GASNETT_INLINE(_bupc_atomic32_read)
  uint32_t
  _bupc_atomic32_read(upcr_shared_ptr_t src, int isstrict UPCRI_PT_ARG) {
    UPCRI_PASS_GAS();
    uint32_t retval;
    upcri_local_t local = upcri_s_islocal(src);
    upcri_checkvalid_shared(src);
  
    /* See comments in upcr_shaccess.h for description of fencing a "strict_GET()" */
    if_pt (local) {
      gasnett_atomic32_t * const p = (gasnett_atomic32_t *)upcri_s2local(local,src);
      retval = gasnett_atomic32_read(p, _bupc_atomic_read_flags(isstrict));
    } else {
      struct upcri_atomic_result result;
      void* srcaddr = upcri_shared_to_remote(src);
      result.flag = 0;
      gasnett_local_wmb(); /* Needed for both 'flag' and for STRICT */
      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(3, 5,
	            (upcri_shared_nodeof(src),
		     UPCRI_HANDLER_ID(upcri_atomic_read_SRequest), 
	             4, UPCRI_SEND_PTR(srcaddr),
		     UPCRI_SEND_PTR(&result))));
      GASNET_BLOCKUNTIL(result.flag != 0); /* Includes the RMB for STRICT */
      retval = result.value._bupc_atomic_val_U32;
    }
    _BUPC_ATOMIC_TRACE_GET(upcri_shared_nodeof(src), &retval, isstrict);
    return retval;
  }
  
  GASNETT_INLINE(_bupc_atomic32_set)
  void
  _bupc_atomic32_set(upcr_shared_ptr_t dest, uint32_t val, int isstrict UPCRI_PT_ARG) {
    UPCRI_PASS_GAS();
    upcri_local_t local = upcri_s_islocal(dest);
    upcri_checkvalid_shared(dest);
  
    /* See comments in upcr_shaccess.h for description of fencing a "strict_PUT()" */
    if_pt (local) {
      gasnett_atomic32_t * const p = (gasnett_atomic32_t *)upcri_s2local(local,dest);
      gasnett_atomic32_set(p, val, _bupc_atomic_set_flags(isstrict));
    } else {
      struct upcri_atomic_result result;
      void* destaddr = upcri_shared_to_remote(dest);
      result.flag = 0;
      gasnett_local_wmb(); /* Needed for both 'flag' and for STRICT */
      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(5, 7,
	            (upcri_shared_nodeof(dest),
		     UPCRI_HANDLER_ID(upcri_atomic_set_SRequest), 
		     4, 0, val, UPCRI_SEND_PTR(destaddr),
		     UPCRI_SEND_PTR(&result))));
      GASNET_BLOCKUNTIL(result.flag != 0); /* Includes the RMB for STRICT */
    }
    _BUPC_ATOMIC_TRACE_PUT(upcri_shared_nodeof(dest), &val, isstrict);
  }

  GASNETT_INLINE(_bupc_atomic64_read)
  uint64_t
  _bupc_atomic64_read(upcr_shared_ptr_t src, int isstrict UPCRI_PT_ARG) {
    UPCRI_PASS_GAS();
    uint64_t retval;
    upcri_local_t local = upcri_s_islocal(src);
    upcri_checkvalid_shared(src);
  
    /* See comments in upcr_shaccess.h for description of fencing a "strict_GET()" */
    if_pt (local) {
      gasnett_atomic64_t * const p = (gasnett_atomic64_t *)upcri_s2local(local,src);
      retval = gasnett_atomic64_read(p, _bupc_atomic_read_flags(isstrict));
    } else {
      struct upcri_atomic_result result;
      void* srcaddr = upcri_shared_to_remote(src);
      result.flag = 0;
      gasnett_local_wmb(); /* Needed for both 'flag' and for STRICT */
      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(3, 5,
	            (upcri_shared_nodeof(src),
		     UPCRI_HANDLER_ID(upcri_atomic_read_SRequest), 
	             8, UPCRI_SEND_PTR(srcaddr),
		     UPCRI_SEND_PTR(&result))));
      GASNET_BLOCKUNTIL(result.flag != 0); /* Includes the RMB for STRICT */
      retval = result.value._bupc_atomic_val_U64;
    }
    _BUPC_ATOMIC_TRACE_GET(upcri_shared_nodeof(src), &retval, isstrict);
    return retval;
  }
  
  GASNETT_INLINE(_bupc_atomic64_set)
  void
  _bupc_atomic64_set(upcr_shared_ptr_t dest, uint64_t val, int isstrict UPCRI_PT_ARG) {
    UPCRI_PASS_GAS();
    upcri_local_t local = upcri_s_islocal(dest);
    upcri_checkvalid_shared(dest);
  
    /* See comments in upcr_shaccess.h for description of fencing a "strict_PUT()" */
    if_pt (local) {
      gasnett_atomic64_t * const p = (gasnett_atomic64_t *)upcri_s2local(local,dest);
      gasnett_atomic64_set(p, val, _bupc_atomic_set_flags(isstrict));
    } else {
      struct upcri_atomic_result result;
      void* destaddr = upcri_shared_to_remote(dest);
      result.flag = 0;
      gasnett_local_wmb(); /* Needed for both 'flag' and for STRICT */
      UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(5, 7,
	            (upcri_shared_nodeof(dest),
		     UPCRI_HANDLER_ID(upcri_atomic_set_SRequest), 
		     8, (uint32_t)(val >> 32), (uint32_t)val,
	             UPCRI_SEND_PTR(destaddr),
		     UPCRI_SEND_PTR(&result))));
      GASNET_BLOCKUNTIL(result.flag != 0); /* Includes the RMB for STRICT */
    }
    _BUPC_ATOMIC_TRACE_PUT(upcri_shared_nodeof(dest), &val, isstrict);
  }
#else
  /* This code is *NOT* currently being maintained */

  GASNETT_INLINE(_bupc_atomic32_read)
  uint32_t
  _bupc_atomic32_read(upcr_shared_ptr_t src, int isstrict UPCRI_PT_ARG) {
    return (uint32_t)_upcr_get_shared_val(src, 0, 4, isstrict UPCRI_PT_PASS);
  }

  GASNETT_INLINE(_bupc_atomic32_set)
  void
  _bupc_atomic32_set(upcr_shared_ptr_t dest, uint32_t val, int isstrict UPCRI_PT_ARG) {
    _upcr_put_shared_val(dest, 0, val, 4, isstrict UPCRI_PT_PASS);
  }

  GASNETT_INLINE(_bupc_atomic64_read)
  uint64_t
  _bupc_atomic64_read(upcr_shared_ptr_t src, int isstrict UPCRI_PT_ARG) {
    uint64_t retval;
    #if SIZEOF_UPCR_REGISTER_VALUE_T >= 8
      retval = (uint64_t)_upcr_get_shared_val(src, 0, 8, isstrict UPCRI_PT_PASS);
    #else
      _upcr_get_shared(&retval, src, 0, 8, isstrict UPCRI_PT_PASS);
    #endif
    return retval;
  }

  GASNETT_INLINE(_bupc_atomic64_set)
  void
  _bupc_atomic64_set(upcr_shared_ptr_t dest, uint64_t val, int isstrict UPCRI_PT_ARG) {
    #if SIZEOF_UPCR_REGISTER_VALUE_T >= 8
      _upcr_put_shared_val(dest, 0, val, 8, isstrict UPCRI_PT_PASS);
    #else
      _upcr_put_shared(dest, 0, &val, 8, isstrict UPCRI_PT_PASS);
    #endif
  }
#endif

/*---------------------------------------------------------------------------------*/
/* Operations on [u]int64_t: */

GASNETT_INLINE(_bupc_atomicU64_fetchadd_local)
uint64_t
_bupc_atomicU64_fetchadd_local(void *addr, uint64_t op, int flags) {
  gasnett_atomic64_t * const p = (gasnett_atomic64_t *)addr;
  uint64_t oldval, newval;
  do {
    newval = (oldval = gasnett_atomic64_read(p, 0)) + op;
  } while(!gasnett_atomic64_compare_and_swap(p, oldval, newval, flags));
  return oldval;
}

GASNETT_INLINE(_bupc_atomicU64_fetchadd)
uint64_t
_bupc_atomicU64_fetchadd(upcr_shared_ptr_t dest, uint64_t op, int isstrict UPCRI_PT_ARG) {
  UPCRI_PASS_GAS();
  uint64_t retval;
  upcri_local_t local = upcri_s_islocal(dest);
  upcri_checkvalid_shared(dest);

  /* Strict fencing is the union of strict get/put operations */
  if_pt (local) {
    retval = _bupc_atomicU64_fetchadd_local(upcri_s2local(local,dest), op, _bupc_atomic_cas_flags(isstrict));
  } else {
    struct upcri_atomic_result result;
    void* destaddr = upcri_shared_to_remote(dest);
    result.flag = 0;
    gasnett_local_wmb(); /* For both 'flag' and for STRICT */
    UPCRI_AM_CALL(UPCRI_SHORT_REQUEST(5, 7,
	          (upcri_shared_nodeof(dest),
		   UPCRI_HANDLER_ID(upcri_atomic_fetchandX_SRequest), 
		   _BUPC_ATOMIC_TYPE_U64, (uint32_t)(op >> 32), (uint32_t)op,
	           UPCRI_SEND_PTR(destaddr),
		   UPCRI_SEND_PTR(&result))));
    GASNET_BLOCKUNTIL(result.flag != 0); /* Includes the RMB for STRICT */
    retval = result.value._bupc_atomic_val_U64;
  }
  _BUPC_ATOMIC_TRACE_RMW(upcri_shared_nodeof(dest), &op, &retval, isstrict);
  return retval;
}

/* XXX: Assume 2s complement arithmetic: */
GASNETT_INLINE(_bupc_atomicI64_fetchadd)
int64_t
_bupc_atomicI64_fetchadd(upcr_shared_ptr_t dest, int64_t op, int isstrict UPCRI_PT_ARG) {
  return (int64_t)_bupc_atomicU64_fetchadd(dest, (uint64_t)op, isstrict UPCRI_PT_PASS);
}
/*---------------------------------------------------------------------------------*/
/* External interface: */

/* Apply strictness, casts, srcpos() and PT argument as needed */
#if UPCRI_LIBWRAP
  #define _bupc_atomic_set_fixed(_type,_bits,_dst,_val,_isstrict) \
	_bupc_atomic##_bits##_set(_dst,(_type)(_val),_isstrict)
  #define _bupc_atomic_set_floating  ERROR__floating_point_atomics_unimplemented

  #define _bupc_atomic_read_fixed(_type,_bits,_src,_isstrict) \
	((_type)_bupc_atomic##_bits##_read(_src,_isstrict))
  #define _bupc_atomic_read_floating ERROR__floating_point_atomics_unimplemented

  #define _bupc_atomic_fetchop(_code,_operation,_dst,_operand,_isstrict) \
  	_bupc_atomic##_code##_fetch##_operation(_dst,_operand,_isstrict)
#else
  #define _bupc_atomic_set_fixed(_type,_bits,_dst,_val,_isstrict) \
	(upcri_srcpos(),_bupc_atomic##_bits##_set(_dst,_val,_isstrict UPCRI_PT_PASS))
  #define _bupc_atomic_set_floating  ERROR__floating_point_atomics_unimplemented

  #define _bupc_atomic_read_fixed(_type,_bits,_src,_isstrict) \
	(upcri_srcpos(),(_type)_bupc_atomic##_bits##_read(_src,_isstrict UPCRI_PT_PASS))
  #define _bupc_atomic_read_floating ERROR__floating_point_atomics_unimplemented

  #define _bupc_atomic_fetchop(_code,_operation,_dst,_operand,_isstrict) \
	(upcri_srcpos(),_bupc_atomic##_code##_fetch##_operation(_dst,_operand,_isstrict UPCRI_PT_PASS))
#endif


#if UPCRI_LIBWRAP
  #define _bupc_atomicU64_set_relaxed(_dst,_val)       _bupc_atomic_set_fixed(uint64_t,64,_dst,_val,0)
  #define _bupc_atomicU64_set_strict(_dst,_val)        _bupc_atomic_set_fixed(uint64_t,64,_dst,_val,1)
  #define _bupc_atomicU64_read_relaxed(_src)           _bupc_atomic_read_fixed(uint64_t,64,_src,0)
  #define _bupc_atomicU64_read_strict(_src)            _bupc_atomic_read_fixed(uint64_t,64,_src,1)
  #define _bupc_atomicU64_fetchadd_relaxed(_dst,_op)   _bupc_atomic_fetchop(U64,add,_dst,_op,0)
  #define _bupc_atomicU64_fetchadd_strict(_dst,_op)    _bupc_atomic_fetchop(U64,add,_dst,_op,1)

  #define _bupc_atomicI64_set_relaxed(_dst,_val)       _bupc_atomic_set_fixed(int64_t,64,_dst,_val,0)
  #define _bupc_atomicI64_set_strict(_dst,_val)        _bupc_atomic_set_fixed(int64_t,64,_dst,_val,1)
  #define _bupc_atomicI64_read_relaxed(_src)           _bupc_atomic_read_fixed(int64_t,64,_src,0)
  #define _bupc_atomicI64_read_strict(_src)            _bupc_atomic_read_fixed(int64_t,64,_src,1)
  #define _bupc_atomicI64_fetchadd_relaxed(_dst,_op)   _bupc_atomic_fetchop(I64,add,_dst,_op,0)
  #define _bupc_atomicI64_fetchadd_strict(_dst,_op)    _bupc_atomic_fetchop(I64,add,_dst,_op,1)
#else
  #define bupc_atomicU64_set_relaxed(_dst,_val)       _bupc_atomic_set_fixed(uint64_t,64,_dst,_val,0)
  #define bupc_atomicU64_set_strict(_dst,_val)        _bupc_atomic_set_fixed(uint64_t,64,_dst,_val,1)
  #define bupc_atomicU64_read_relaxed(_src)           _bupc_atomic_read_fixed(uint64_t,64,_src,0)
  #define bupc_atomicU64_read_strict(_src)            _bupc_atomic_read_fixed(uint64_t,64,_src,1)
  #define bupc_atomicU64_fetchadd_relaxed(_dst,_op)   _bupc_atomic_fetchop(U64,add,_dst,_op,0)
  #define bupc_atomicU64_fetchadd_strict(_dst,_op)    _bupc_atomic_fetchop(U64,add,_dst,_op,1)

  #define bupc_atomicI64_set_relaxed(_dst,_val)       _bupc_atomic_set_fixed(int64_t,64,_dst,_val,0)
  #define bupc_atomicI64_set_strict(_dst,_val)        _bupc_atomic_set_fixed(int64_t,64,_dst,_val,1)
  #define bupc_atomicI64_read_relaxed(_src)           _bupc_atomic_read_fixed(int64_t,64,_src,0)
  #define bupc_atomicI64_read_strict(_src)            _bupc_atomic_read_fixed(int64_t,64,_src,1)
  #define bupc_atomicI64_fetchadd_relaxed(_dst,_op)   _bupc_atomic_fetchop(I64,add,_dst,_op,0)
  #define bupc_atomicI64_fetchadd_strict(_dst,_op)    _bupc_atomic_fetchop(I64,add,_dst,_op,1)
#endif
/*---------------------------------------------------------------------------------*/

#endif /* UPCR_ATOMIC_H */
