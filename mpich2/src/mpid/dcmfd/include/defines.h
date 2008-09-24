/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/defines.h
 * \brief ???
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_

#include "mpido_properties.h"

#define DCMF_ISPOF2(x)   (!(((x) - 1) & (x)) ? 1 : 0)
#define DCMF_ISEVEN(x)   (((x) % 2) ? 0 : 1)

/* denotes the end of arguments in variadic functions */
#define DCMF_END_ARGS -1

typedef int DCMF_Embedded_Info_Mask;

#define DCMF_INFO_TOTAL_BITS (8 * sizeof (DCMF_Embedded_Info_Mask))
#define DCMF_INFO_ELMT(d)    ((d) / DCMF_INFO_TOTAL_BITS)
#define DCMF_INFO_MASK(d)                                                     \
        ((DCMF_Embedded_Info_Mask) 1 << ((d) % DCMF_INFO_TOTAL_BITS))

/* embedded_info_set of informative bits */
typedef struct
{
  DCMF_Embedded_Info_Mask info_bits[4]; /* initially 128 bits */
} DCMF_Embedded_Info_Set;

# define DCMF_INFO_BITS(set) ((set) -> info_bits)

/* unset all bits in the embedded_info_set */
#define DCMF_INFO_ZERO(s)                                                     \
  do                                                                          \
  {                                                                           \
    unsigned int __i, __j;                                                    \
    DCMF_Embedded_Info_Set * __arr = (s);                                     \
    __j = sizeof(DCMF_Embedded_Info_Set) / sizeof(DCMF_Embedded_Info_Mask);   \
    for (__i = 0; __i < __j; ++__i)                                           \
      DCMF_INFO_BITS (__arr)[__i] = 0;                                        \
  } while (0)

#define DCMF_INFO_OR(s, d)                                                    \
  do                                                                          \
  {                                                                           \
    unsigned int __i, __j;                                                    \
    DCMF_Embedded_Info_Set * __src = (s);                                     \
    DCMF_Embedded_Info_Set * __dst = (d);                                     \
    __j = sizeof(DCMF_Embedded_Info_Set) / sizeof(DCMF_Embedded_Info_Mask);   \
    for (__i = 0; __i < __j; ++__i)                                           \
      DCMF_INFO_BITS (__dst)[__i] |= DCMF_INFO_BITS (__src)[__i];             \
  } while (0)



/* sets a bit in the embedded_info_set */
#define DCMF_INFO_SET(s, d)                                                   \
        (DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] |= DCMF_INFO_MASK(d))

/* unsets a bit in the embedded_info_set */
#define DCMF_INFO_UNSET(s, d)                                                 \
        (DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] &= ~DCMF_INFO_MASK(d))

/* checks a bit in the embedded_info_set */
#define DCMF_INFO_ISSET(s, d)                                                 \
        ((DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] & DCMF_INFO_MASK(d)) != 0)

/* this sets multiple bits in one call */
extern void DCMF_MSET_INFO(DCMF_Embedded_Info_Set * set, ...);

/* this sees if bits pattern in s are in d */
extern int DCMF_INFO_MET(DCMF_Embedded_Info_Set *s, DCMF_Embedded_Info_Set *d);

enum DCMF_SUPPORTED {
  DCMF_TREE_SUPPORT        =  0,
  DCMF_TORUS_SUPPORT       =  1,
  DCMF_NOT_SUPPORTED       = -1,
  DCMF_SUPPORT_NOT_NEEDED  = -2
};

#endif
