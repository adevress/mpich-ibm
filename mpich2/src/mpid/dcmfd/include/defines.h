/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/defines.h
 * \brief ???
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_

#define DCMF_ISPOF2(x)   (!(((x) - 1) & (x)) ? 1 : 0)
#define DCMF_ISEVEN(x)   (((x) % 2) ? 0 : 1)

/* denotes the end of arguments in variadic functions */
#define DCMF_END_ARGS -1

/*
  We will use bit patterns to represents features, conditions, and properties
  of any object in the comm stack.  The following constructs the building
  block of an Embedded_Info_Set object.  The idea here is to save space / mem
  and number of variables to represents different properties.

  Author: Ahmad Faraj.
  Date: 05/13/08
 */

typedef int DCMF_Embedded_Info_Mask;

#define DCMF_INFO_TOTAL_BITS (8 * sizeof (DCMF_Embedded_Info_Mask))
#define DCMF_INFO_ELMT(d)    ((d) / DCMF_INFO_TOTAL_BITS)
#define DCMF_INFO_MASK(d)    ((DCMF_Embedded_Info_Mask) 1 << ((d) % DCMF_INFO_TOTAL_BITS))

/* embedded_info_set of informative bits */
typedef struct
{
  DCMF_Embedded_Info_Mask info_bits[8]; /* initially 256 bits */
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

/* sets a bit in the embedded_info_set */
#define DCMF_INFO_SET(s, d)   (DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] |= DCMF_INFO_MASK(d))

/* unsets a bit in the embedded_info_set */
#define DCMF_INFO_UNSET(s, d) (DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] &= ~DCMF_INFO_MASK(d))

/* checks a bit in the embedded_info_set */
#define DCMF_INFO_ISSET(s, d) ((DCMF_INFO_BITS (s)[DCMF_INFO_ELMT(d)] & DCMF_INFO_MASK(d)) != 0)

extern int DCMF_CHECK_INFO(DCMF_Embedded_Info_Set * set, ...);
extern void DCMF_SET_INFO(DCMF_Embedded_Info_Set * set, ...);

/* Each #define below corresponds to a bit position in the Embedded_Info_Set */


/* if comm size is power of 2 */
#define DCMF_POF2                                                         0   

/* if comm size is even */
#define DCMF_EVEN                                                         1

/* Is comm in threaded mode? */
#define DCMF_THREADED                                                     2

/* is comm a torus? */
#define DCMF_TORUS                                                        3

/* does comm support tree? */
#define DCMF_TREE                                                         4 

/* Is comm subbcomm? */
#define DCMF_SUBCOMM                                                      5

/* is comm a rect? */
#define DCMF_RECT                                                         6

/* Is comm 1D rect? */
#define DCMF_RECT1                                                        7

/* Is comm 2D rect? */
#define DCMF_RECT2                                                        8

/* Is comm 3D rect? */
#define DCMF_RECT3                                                        9

/* does comm have global context? */
#define DCMF_GLOBAL_CONTEXT                                               10

/* works with any comm */
#define DCMF_ANY_COMM                                                     11

/* can we use tree bcast? */
#define DCMF_TREE_BCAST                                                   12

/* can we use rect bcast? */
#define DCMF_RECT_BCAST                                                   13

/* can we use binomial bcast? */
#define DCMF_BINOM_BCAST                                                  14

/* can we use async rect bcast? */
#define DCMF_ASYNC_RECT_BCAST                                             15

/* can we use async binomial bcast? */
#define DCMF_ASYNC_BINOM_BCAST                                            16

/* can we use tree allred? */
#define DCMF_TREE_ALLREDUCE                                               17

/* can we use tree pipelined allred? */
#define DCMF_TREE_PIPE_ALLREDUCE                                          18

/* can we use CCMI allreduce? */
#define DCMF_TREE_CCMI_ALLREDUCE                                          19

/* can we use binomial allreduce? */
#define DCMF_BINOM_ALLREDUCE                                              20

/* can we use rectangle allreduce? */
#define DCMF_RECT_ALLREDUCE                                               21

/* can we use rectangle ring allreduce? */
#define DCMF_RECTRING_ALLREDUCE                                           22

/* can we use tree reduce? */
#define DCMF_TREE_REDUCE                                                  23

/* can we use tree ccmi reduce? */
#define DCMF_TREE_CCMI_REDUCE                                             24

/* can we use binomial reduce? */
#define DCMF_BINOM_REDUCE                                                 25

/* can we use rect reduce? */
#define DCMF_RECT_REDUCE                                                  26

/* can we use rectring reduce? */
#define DCMF_RECTRING_REDUCE                                              27

/* can we use optimized alltoall? */
#define DCMF_TORUS_ALLTOALL                                               28

/* can we use opt torus alltoallv? */
#define DCMF_TORUS_ALLTOALLV                                              29

/* can we use opt torus alltoallw? */
#define DCMF_TORUS_ALLTOALLW                                              30

/* can we use lockbox barrier */
#define DCMF_LOCKBOX_BARRIER                                              31

/* can we use binomial barrier? */
#define DCMF_BINOM_BARRIER                                                32

/* can we use GI network? */
#define DCMF_GI                                                           33

/* alg works only with tree op/type */
#define DCMF_TREE_OP_TYPE                                                 34

/* alg works with supported op/type */
#define DCMF_TORUS_OP_TYPE                                                35

/* alg works with any op/type */
#define DCMF_ANY_OP_TYPE                                                  36

/* can we use allreduce allgather? */
#define DCMF_ALLREDUCE_ALLGATHER                                          37

/* can we use alltoall allgather? */
#define DCMF_ALLTOALL_ALLGATHER                                           38

/* can we use bcast based allgather? */
#define DCMF_BCAST_ALLGATHER                                              39

/* can we use async binom bcast based allgather? */
#define DCMF_ASYNC_BINOM_BCAST_ALLGATHER                                  40

/* can we use async rect bcast based allgather? */
#define DCMF_ASYNC_RECT_BCAST_ALLGATHER                                   41

/* can we use allreduce allgatherv? */
#define DCMF_ALLREDUCE_ALLGATHERV                                         42

/* can we use alltoall allgatherv? */
#define DCMF_ALLTOALL_ALLGATHERV                                          43

/* can we use bcast based allgatherv? */
#define DCMF_BCAST_ALLGATHERV                                             44

/* can we use async binom bcast based allgather? */
#define DCMF_ASYNC_BINOM_BCAST_ALLGATHERV                                 45

/* can we use async rect bcast based allgather? */
#define DCMF_ASYNC_RECT_BCAST_ALLGATHERV                                  46

/* can we use reduce based gather? */
#define DCMF_REDUCE_GATHER                                                47

/* can we use bcast based scatter? */
#define DCMF_BCAST_SCATTER                                                48

/* does send buff need to be contig? */
#define DCMF_SBUFF_CONTIG                                                 49

/* does recv buff need to be contig?*/
#define DCMF_RBUFF_CONTIG                                                 50

/* does recv buff need to be contin?*/
#define DCMF_RBUFF_CONTIN                                                 51

/* is buff size multiple of 4? */
#define DCMF_BUFF_SIZE_MUL4                                               52

/* can we use bcast based scatterv? */
#define DCMF_BCAST_SCATTERV                                               53

/* can we use alltoallv based scatterv? */
#define DCMF_ALLTOALLV_SCATTERV                                           54

/* can we use alltoallv based scatterv? */
#define DCMF_TREE_TORUS_REDUCE_SCATTER                                    55

/* no conditions needed to check */
#define DCMF_NONE                                                         56

/* currently, we can have #define go upto 255, use below as a template */
/* is ? */
/* #define DCMF_                                                          56 */


enum DCMF_SUPPORTED {
  DCMF_TREE_SUPPORT        =  0,
  DCMF_TORUS_SUPPORT       =  1,
  DCMF_GENERIC_SUPPORT     = -1,
  DCMF_SUPPORT_NOT_NEEDED  = -2
};

#endif
