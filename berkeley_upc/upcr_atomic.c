/*   $Source: /var/local/cvs/upcr/upcr_atomic.c,v $
 *     $Date: 2006/08/30 20:59:47 $
 * $Revision: 1.9 $
 * Description: 
 *  Berkeley UPC atomic extensions
 *
 * Copyright 2006, Dan Bonachea <bonachea@cs.berkeley.edu>
 */

#include <upcr.h>
#include <upcr_internal.h>

/*---------------------------------------------------------------------------------*/

/* For now the following three handlers are just for typeless 32- and 64-bit scalars.
 * Vector results could be handled by either replacing or augmenting these
 * handlers with Mediums.
 */

void
upcri_atomic_SReply(gasnet_token_t token, int bytes, uint32_t hi32, uint32_t lo32, void *result) {
  switch (bytes) {
    case 4:
      ((struct upcri_atomic_result *)result)->value._bupc_atomic_val_U32 = lo32;
      break;
    case 8:
      ((struct upcri_atomic_result *)result)->value._bupc_atomic_val_U64 = ((uint64_t)hi32 << 32) | lo32;
      break;
    default:
      upcri_err("Invalid width in upcri_atomic_SReply");
  }
  gasnett_local_wmb();
  ((struct upcri_atomic_result *)result)->flag = 1;
}

void
upcri_atomic_set_SRequest(gasnet_token_t token, int bytes, uint32_t hi32, uint32_t lo32,
			  void *addr, void *result) {
  switch (bytes) {
    case 4:
      gasnett_atomic32_set((gasnett_atomic32_t *)addr, lo32, 0);
      break;
    case 8:
    {
      const uint64_t value = ((uint64_t)hi32 << 32) | lo32;
      gasnett_atomic64_set((gasnett_atomic64_t *)addr, value, 0);
      break;
    }
    default:
      upcri_err("Invalid width in upcri_atomic_set_SRequest");
  }
  UPCRI_AM_CALL(UPCRI_SHORT_REPLY(4, 5, 
		(token, UPCRI_HANDLER_ID(upcri_atomic_SReply),
		 bytes, hi32, lo32, UPCRI_SEND_PTR(result))));
}

void
upcri_atomic_read_SRequest(gasnet_token_t token, int bytes, void *addr, void *result) {
  uint32_t hi32;
  uint32_t lo32;
  switch (bytes) {
    case 4:
#if UPCR_DEBUG
      hi32 = 0xdeadbeef;	/* avoid uninitialized variable warning */
#endif
      lo32 = gasnett_atomic32_read((gasnett_atomic32_t *)addr, 0);
      break;
    case 8:
    {
      const uint64_t value = gasnett_atomic64_read((gasnett_atomic64_t *)addr, 0);
      hi32 = (uint32_t)(value >> 32);
      lo32 = (uint32_t)value;
      break;
    }
    default:
      upcri_err("Invalid width in upcri_atomic_set_SRequest");
  }
  UPCRI_AM_CALL(UPCRI_SHORT_REPLY(4, 5, 
		(token, UPCRI_HANDLER_ID(upcri_atomic_SReply),
		 bytes, hi32, lo32, UPCRI_SEND_PTR(result))));
}

/* For now this is only fetch-and-add of uint{32,64}_t.
 * However, as the interface grows, this one handler will grow
 * "op", "type" and "count" arguments to add generality without
 * the need to add additional handlers.
 */
void
upcri_atomic_fetchandX_SRequest(gasnet_token_t token, int type,
				uint32_t hi32, uint32_t lo32,
			        void *addr, void *result) {
  int bytes;
  switch (type) {
    case _BUPC_ATOMIC_TYPE_U64:
    {
      const uint64_t op = ((uint64_t)hi32 << 32) | lo32;
      const uint64_t oldval = _bupc_atomicU64_fetchadd_local(addr, op, 0);
      hi32 = (uint32_t)(oldval >> 32);
      lo32 = (uint32_t)oldval,
      bytes = 8;
      break;
    }
    case _BUPC_ATOMIC_TYPE_U32:
    {
      lo32 = _bupc_atomicU64_fetchadd_local(addr, lo32, 0);
      bytes = 4;
      break;
    }
    default:
#if UPCR_DEBUG
      bytes = 0;	/* avoid uninitialized variable warning */
#endif
      upcri_err("Invalid type in upcri_atomic_fetchandX_SRequest");
  }

  UPCRI_AM_CALL(UPCRI_SHORT_REPLY(4, 5, 
		(token, UPCRI_HANDLER_ID(upcri_atomic_SReply),
		 bytes, hi32, lo32, UPCRI_SEND_PTR(result))));
}

/*---------------------------------------------------------------------------------*/
