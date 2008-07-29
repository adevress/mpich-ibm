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

/* Each #define below corresponds to a bit position in the Embedded_Info_Set */

#define DCMF_USE_OPT_BARRIER                                               0

#define DCMF_USE_LOCKBOX_LBARRIER                                          1

#define DCMF_USE_BINOM_LBARRIER                                            2

#define DCMF_USE_TREE_BCAST                                                3

#define DCMF_USE_RECT_BCAST                                                4

#define DCMF_USE_ARECT_BCAST                                               5

#define DCMF_USE_BINOM_BCAST                                               6

#define DCMF_USE_ABINOM_BCAST                                              7

#define DCMF_USE_OPT_BCAST                                                 8
                                              
#define DCMF_USE_TORUS_ALLTOALL                                            9

#define DCMF_USE_TORUS_ALLTOALLV                                          10

#define DCMF_USE_TORUS_ALLTOALLW                                          11
                                              
#define DCMF_USE_PREMALLOC_ALLTOALL                                       12
                                              
#define DCMF_USE_ALLREDUCE_ALLGATHER                                      13
                                              
#define DCMF_USE_BCAST_ALLGATHER                                          14
                                              
#define DCMF_USE_ALLTOALL_ALLGATHER                                       15
                                              
#define DCMF_USE_ABCAST_ALLGATHER                                         16

#define DCMF_USE_ARECT_BCAST_ALLGATHER                                    17

#define DCMF_USE_ABINOM_BCAST_ALLGATHER                                   18

#define DCMF_USE_PREALLREDUCE_ALLGATHER                                   19

#define DCMF_USE_OPT_ALLGATHER                                            20
                                              
#define DCMF_USE_ALLREDUCE_ALLGATHERV                                     21
                                              
#define DCMF_USE_BCAST_ALLGATHERV                                         22
                                              
#define DCMF_USE_ALLTOALL_ALLGATHERV                                      23
                                              
#define DCMF_USE_ABCAST_ALLGATHERV                                        24

#define DCMF_USE_PREALLREDUCE_ALLGATHERV                                  25

#define DCMF_USE_OPT_ALLGATHERV                                           26
                                              
#define DCMF_USE_BCAST_SCATTER                                            27  

#define DCMF_USE_OPT_SCATTER                                              28  

#define DCMF_USE_BCAST_SCATTERV                                           29   

#define DCMF_USE_ALLTOALL_SCATTERV                                        30 

#define DCMF_USE_ALLREDUCE_SCATTERV                                       31

#define DCMF_USE_PREALLREDUCE_SCATTERV                                    32

#define DCMF_USE_OPT_SCATTERV                                             33
       
#define DCMF_USE_REDUCESCATTER                                            34

#define DCMF_USE_OPT_REDUCESCATTER                                        35
                                              
#define DCMF_USE_REDUCE_GATHER                                            36
  
#define DCMF_USE_OPT_GATHER                                               37
                                           
#define DCMF_USE_STORAGE_ALLREDUCE                                        38
                                              
#define DCMF_USE_TREE_ALLREDUCE                                           39
                                              
#define DCMF_USE_CCMI_TREE_ALLREDUCE                                      40
                                              
#define DCMF_USE_PIPELINED_TREE_ALLREDUCE                                 41
                                              
#define DCMF_USE_RECT_ALLREDUCE                                           42
                                              
#define DCMF_USE_RECTRING_ALLREDUCE                                       43
                                              
#define DCMF_USE_BINOM_ALLREDUCE                                          44
                                              
#define DCMF_USE_OPT_ALLREDUCE                                            45

#define DCMF_USE_STORAGE_REDUCE                                           46
                                              
#define DCMF_USE_TREE_REDUCE                                              47
                                              
#define DCMF_USE_CCMI_TREE_REDUCE                                         48
                                              
#define DCMF_USE_RECT_REDUCE                                              49
                                              
#define DCMF_USE_RECTRING_REDUCE                                          50
                                             
#define DCMF_USE_BINOM_REDUCE                                             51
                                              
#define DCMF_USE_OPT_REDUCE                                               52
                                              
#define DCMF_USE_NOTREE_OPT_COLLECTIVES                                   53

#define DCMF_USE_MPICH                                                    54

#define DCMF_USE_GI_BARRIER                                               55

#define DCMF_USE_BINOM_BARRIER                                            56

#define DCMF_USE_MPICH_BARRIER                                            57

#define DCMF_USE_MPICH_BCAST                                              58

#define DCMF_USE_MPICH_ALLTOALL                                           59

#define DCMF_USE_MPICH_ALLTOALLW                                          60  

#define DCMF_USE_MPICH_ALLTOALLV                                          61  

#define DCMF_USE_MPICH_ALLGATHER                                          62  

#define DCMF_USE_MPICH_ALLGATHERV                                         63   

#define DCMF_USE_MPICH_ALLREDUCE                                          64  

#define DCMF_USE_MPICH_REDUCE                                             65

#define DCMF_USE_MPICH_GATHER                                             66

#define DCMF_USE_MPICH_SCATTER                                            67

#define DCMF_USE_MPICH_SCATTERV                                           68

#define DCMF_USE_MPICH_REDUCESCATTER                                      69

#define DCMF_USE_ARECT_BCAST_ALLGATHERV                                   70

#define DCMF_USE_ABINOM_BCAST_ALLGATHERV                                  71

/* if comm size is power of 2 */
#define DCMF_POF2_COMM                                                    72   

/* if comm size is even */
#define DCMF_EVEN_COMM                                                    73

/* Is comm in threaded mode? */
#define DCMF_THREADED_MODE                                                74

/* is comm a torus? */
#define DCMF_TORUS_COMM                                                   75

/* does comm support tree? */
#define DCMF_TREE_COMM                                                    76 

/* Is comm subbcomm? */
#define DCMF_SUB_COMM                                                     77

/* is comm a rect? */
#define DCMF_RECT_COMM                                                    78

/* Is comm 1D rect? */
#define DCMF_1D_RECT_COMM                                                 79

/* Is comm 2D rect? */
#define DCMF_2D_RECT_COMM                                                 80

/* Is comm 3D rect? */
#define DCMF_3D_RECT_COMM                                                 81

/* does comm have global context? */
#define DCMF_GLOBAL_CONTEXT                                               82

/* works with any comm */
#define DCMF_ANY_COMM                                                     83

/* alg works only with tree op/type */
#define DCMF_TREE_OP_TYPE                                                 84

/* alg works with supported op/type */
#define DCMF_TORUS_OP_TYPE                                                85

/* alg works with any op/type */
#define DCMF_ANY_OP_TYPE                                                  86

/* does send buff need to be contig? */
#define DCMF_SBUFF_CONTIG                                                 87

/* does recv buff need to be contig?*/
#define DCMF_RBUFF_CONTIG                                                 88

/* does recv buff need to be contin?*/
#define DCMF_RBUFF_CONTIN                                                 89

/* is buff size multiple of 4? */
#define DCMF_BUFF_SIZE_MUL4                                               90

#define DCMF_NONE                                                         91

#define DCMF_USE_ARECT_ALLREDUCE                                          92
                                              
#define DCMF_USE_ARECTRING_ALLREDUCE                                      93
                                              
#define DCMF_USE_ABINOM_ALLREDUCE                                         94

#define DCMF_USE_RECT_BCAST_ALLGATHER                                     95

#define DCMF_USE_BINOM_BCAST_ALLGATHER                                    96

#define DCMF_USE_RECT_BCAST_ALLGATHERV                                    97

#define DCMF_USE_BINOM_BCAST_ALLGATHERV                                   98

#define DCMF_IRREG_COMM                                                   99

#define DCMF_USE_RECT_BARRIER                                            100

enum DCMF_SUPPORTED {
  DCMF_TREE_SUPPORT        =  0,
  DCMF_TORUS_SUPPORT       =  1,
  DCMF_GENERIC_SUPPORT     = -1,
  DCMF_SUPPORT_NOT_NEEDED  = -2
};

#endif
