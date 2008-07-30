#ifndef _UPC_COLLECTIVE_BITS_H_
#define _UPC_COLLECTIVE_BITS_H_

/* This file contains typedefs and constants needed by both the library
 * build and by application builds.  This file contains only C.
 */

/* upc_flag_t is defined under upc.h, as per 1.2 UPC Spec */
#include <upcr_preinclude/upc_bits.h>

/* upc_opt_t
 * Type used to select an operation in reduce and prefix_reduce
 */
typedef enum {
	UPC_ADD,
	UPC_MULT,
	UPC_AND,
	UPC_OR,
	UPC_XOR,
	UPC_LOGAND,
	UPC_LOGOR,
	UPC_MIN,
	UPC_MAX,
	UPC_FUNC,
	UPC_NONCOMM_FUNC
} upc_op_t;

#if (PLATFORM_OS_CATAMOUNT || PLATFORM_OS_CNL) && PLATFORM_COMPILER_GNU
  /* HACK: workaround the fact that gcc and gccupc-3 on Cray XT catamount and CNL crash 
     while building libupcr if long double is used for the collectives. 
     Therefore, long double collectives are treated as having type double for these configs.
     Note: above platform macros will not be defined for BUPC pre-trans, but that's fine
   */
  #define UPCRI_COLL_LONG_DOUBLE double
#else
  #define UPCRI_COLL_LONG_DOUBLE long double
#endif

#endif
