#ifndef _UPC_BITS_H_
#define _UPC_BITS_H_

/* This file contains typedefs and constants needed by both the library
 * build and by application builds.  This file contains only C.
 */


/* upc_flag_t
 * Type used for synchronization mode flags (and possible extensions?)
 * Collective Spec section 3, item 5
 * Draft of UPC Language Spec 1.2 requires this type to be in upc.h
 */
typedef int	upc_flag_t;

/* The representation is unspecified in the Collective Spec.
 * However, draft of UPC Language Spec 1.2 restricts us to
 * use of the 6 least-significant bits.
 *
 * This non-obvious encoding (16 and 32 for the ALLSYNCs) is due to
 * an attempt to get some backward compatibility with the original
 * MTU reference implementation (on which we no longer depend).
 */
#define UPC_IN_NOSYNC   1
#define UPC_IN_MYSYNC   2
#define UPC_IN_ALLSYNC  16
#define UPC_OUT_NOSYNC	4
#define UPC_OUT_MYSYNC	8
#define UPC_OUT_ALLSYNC	32

/* flags for bupc_thread_distance */
#define BUPC_THREADS_SAME       0
#define BUPC_THREADS_VERYNEAR 100
#define BUPC_THREADS_NEAR     200
#define BUPC_THREADS_FAR      300
#define BUPC_THREADS_VERYFAR  400

/* flags for bupc_sem_alloc: */
#define BUPC_SEM_BOOLEAN    1
#define BUPC_SEM_INTEGER    2
#define BUPC_SEM_SPRODUCER  4   /* only a single thread will ever post this sem */
#define BUPC_SEM_MPRODUCER  8
#define BUPC_SEM_SCONSUMER  16  /* only a single thread will ever wait this sem */
#define BUPC_SEM_MCONSUMER  32
#define BUPC_SEM_MASK       63

#define BUPC_SEM_MAXVALUE   65535

#endif
