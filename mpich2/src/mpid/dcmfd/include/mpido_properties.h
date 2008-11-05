/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/defines.h
 * \brief ???
 */

#ifndef _MPIDO_PROPERTIES_H_
#define _MPIDO_PROPERTIES_H_


/* Each #define below corresponds to a bit position in the Embedded_Info_Set */

/****************************************************************************/
/* Properties bits
 * Each #define corresponds to a bit position in the Embedded_Info_Set and is
 * used for picking proper algorithms for collectives, etc
 ****************************************************************************/

#define DCMF_MAX_NUM_BITS 128
/****************************/
/* Communicator properties  */
/****************************/

/* if comm size is power of 2 */
#define DCMF_POF2_COMM                                                    0  
/* Is comm in threaded mode? */
#define DCMF_SINGLE_THREAD_MODE                                           1
/* is comm a torus? */
#define DCMF_TORUS_COMM                                                   2
/* does comm support tree? */
#define DCMF_TREE_COMM                                                    3 
#define DCMF_RECT_COMM                                                    4
#define DCMF_IRREG_COMM                                                   5
/* does comm have global context? */
#define DCMF_GLOBAL_CONTEXT                                               6
/* alg works only with tree op/type */
#define DCMF_TREE_OP_TYPE                                                 7
/* alg works with supported op/type */
#define DCMF_TORUS_OP_TYPE                                                8
/* alg works with any op/type */
/* #warning this might not actually be used. Ahmad needs to find out if it is */
#define DCMF_ANY_OP_TYPE                                                  9
/* does send buff need to be contig? */
#define DCMF_SBUFF_CONTIG                                                 10
/* does recv buff need to be contig?*/
#define DCMF_RBUFF_CONTIG                                                 11
/* does recv buff need to be contin?*/
#define DCMF_RBUFF_CONTIN                                                 12
/* is buff size multiple of 4? */
#define DCMF_BUFF_SIZE_MUL4                                               13
#define DCMF_BUFF_ALIGNED                                                 14
#define DCMF_THREADED_MODE                                                15
#define DCMF_USE_SMP_TREE_SHORTCUT                                        16

/*******************/
/* Collective bits */
/*******************/
#define DCMF_USE_NOTREE_OPT_COLLECTIVES                                   17
#define DCMF_USE_MPICH                                                    18

/* Collectives with complicated cutoffs from multiple algorithms have this
 * set if the user does a DCMF_{}={} selection to override the cutoffs */
#define DCMF_ALLGATHER_ENVVAR                                             19
#define DCMF_ALLGATHERV_ENVVAR                                            20
#define DCMF_ALLREDUCE_ENVVAR                                             21
#define DCMF_BCAST_ENVVAR                                                 22
#define DCMF_REDUCE_ENVVAR                                                23
/* In case we have complicated cutoffs in future algorithms */
#define DCMF_ENVVAR_RESERVED1                                             24
#define DCMF_ENVVAR_RESERVED2                                             25

/* this indicates the bit index that represent properties for collectives */
#define DCMF_COLL_PROP                                                    26

/* Allgather protocols */
#define DCMF_ALLGATHER_F_INDEX                                            26
#define DCMF_ALLGATHER_L_INDEX                                            35
/*---------------------------------------------------------------------------*/
#define DCMF_USE_ABCAST_ALLGATHER                                         26
#define DCMF_USE_ABINOM_BCAST_ALLGATHER                                   27
#define DCMF_USE_ALLTOALL_ALLGATHER                                       28
#define DCMF_USE_ARECT_BCAST_ALLGATHER                                    29
#define DCMF_USE_ALLREDUCE_ALLGATHER                                      30
#define DCMF_USE_BCAST_ALLGATHER                                          31
#define DCMF_USE_BINOM_BCAST_ALLGATHER                                    32
#define DCMF_USE_RECT_BCAST_ALLGATHER                                     33
#define DCMF_USE_RECT_DPUT_ALLGATHER                                      34
#define DCMF_USE_MPICH_ALLGATHER                                          35  
#define DCMF_ALLGATHER_RESERVED1                                          36
/* Allgather preallreduce flag. "Nice" datatypes can use this to bypass a 
 * sanity-checking allreduce inside allgather */
#define DCMF_USE_PREALLREDUCE_ALLGATHER                                   37


/* Allgatherv protocols */
#define DCMF_ALLGATHERV_F_INDEX                                           38
#define DCMF_ALLGATHERV_L_INDEX                                           47
/*---------------------------------------------------------------------------*/
#define DCMF_USE_ALLREDUCE_ALLGATHERV                                     38
#define DCMF_USE_BCAST_ALLGATHERV                                         39
#define DCMF_USE_ALLTOALL_ALLGATHERV                                      40
#define DCMF_USE_ABCAST_ALLGATHERV                                        41
#define DCMF_USE_ARECT_BCAST_ALLGATHERV                                   42
#define DCMF_USE_ABINOM_BCAST_ALLGATHERV                                  43
#define DCMF_USE_RECT_BCAST_ALLGATHERV                                    44
#define DCMF_USE_BINOM_BCAST_ALLGATHERV                                   45
#define DCMF_USE_RECT_DPUT_ALLGATHERV                                     46
#define DCMF_USE_MPICH_ALLGATHERV                                         47   
#define DCMF_ALLGATHERV_RESERVED1                                         48
#define DCMF_USE_PREALLREDUCE_ALLGATHERV                                  49

/* Allreduce protocols */
#define DCMF_ALLREDUCE_F_INDEX                                            50
#define DCMF_ALLREDUCE_L_INDEX                                            62
/*---------------------------------------------------------------------------*/
#define DCMF_USE_TREE_ALLREDUCE                                           50
#define DCMF_USE_CCMI_TREE_ALLREDUCE                                      51
#define DCMF_USE_PIPELINED_TREE_ALLREDUCE                                 52
#define DCMF_USE_RECT_ALLREDUCE                                           53
#define DCMF_USE_RECTRING_ALLREDUCE                                       54
#define DCMF_USE_BINOM_ALLREDUCE                                          55
#define DCMF_USE_ARECT_ALLREDUCE                                          56
#define DCMF_USE_ARECTRING_ALLREDUCE                                      57
#define DCMF_USE_ABINOM_ALLREDUCE                                         58
#define DCMF_USE_SHORT_ASYNC_RECT_ALLREDUCE                               59
#define DCMF_USE_SHORT_ASYNC_BINOM_ALLREDUCE                              60
#define DCMF_USE_RRING_DPUT_SINGLETH_ALLREDUCE                            61
#define DCMF_USE_MPICH_ALLREDUCE                                          62
#define DCMF_ALLREDUCE_RESERVED1                                          63
/* Controls whether or not we reuse storage in allreduce */
#define DCMF_USE_STORAGE_ALLREDUCE                                        64



/* Alltoall(vw) protocols */
#define DCMF_ALLTOALL_F_INDEX                                             65
#define DCMF_ALLTOALL_L_INDEX                                             66
/*---------------------------------------------------------------------------*/
#define DCMF_USE_TORUS_ALLTOALL                                           65
#define DCMF_USE_MPICH_ALLTOALL                                           66

#define DCMF_ALLTOALLV_F_INDEX                                            67
#define DCMF_ALLTOALLV_L_INDEX                                            68
/*---------------------------------------------------------------------------*/
#define DCMF_USE_TORUS_ALLTOALLV                                          67
#define DCMF_USE_MPICH_ALLTOALLV                                          68  

#define DCMF_ALLTOALLW_F_INDEX                                            69
#define DCMF_ALLTOALLW_L_INDEX                                            70
/*---------------------------------------------------------------------------*/
#define DCMF_USE_TORUS_ALLTOALLW                                          69
#define DCMF_USE_MPICH_ALLTOALLW                                          70  

#define DCMF_ALLTOALL_RESERVED1                                           71

/* Alltoall(vw) use a large number of arrays. This lazy allocs them at comm
 * create time */
#define DCMF_USE_PREMALLOC_ALLTOALL                                       72


/* Barrier protocols */
#define DCMF_BARRIER_F_INDEX                                              73
#define DCMF_BARRIER_L_INDEX                                              76
/*---------------------------------------------------------------------------*/
#define DCMF_USE_BINOM_BARRIER                                            73
#define DCMF_USE_GI_BARRIER                                               74
#define DCMF_USE_RECT_BARRIER                                             75
#define DCMF_USE_MPICH_BARRIER                                            76

/* Local barriers */
#define DCMF_LBARRIER_F_INDEX                                             77
#define DCMF_LBARRIER_L_INDEX                                             79
/*---------------------------------------------------------------------------*/
#define DCMF_USE_BINOM_LBARRIER                                           77
#define DCMF_USE_LOCKBOX_LBARRIER                                         78
#define DCMF_USE_RECT_LOCKBOX_LBARRIER                                    79
#define DCMF_BARRIER_RESERVED1                                            80

/* Bcast protocols */
#define DCMF_BCAST_F_INDEX                                                81
#define DCMF_BCAST_L_INDEX                                                90
/*---------------------------------------------------------------------------*/
#define DCMF_USE_ABINOM_BCAST                                             81
#define DCMF_USE_ARECT_BCAST                                              82
#define DCMF_USE_BINOM_BCAST                                              83
#define DCMF_USE_BINOM_SINGLETH_BCAST                                     84
#define DCMF_USE_RECT_BCAST                                               85
#define DCMF_USE_RECT_DPUT_BCAST                                          86
#define DCMF_USE_RECT_SINGLETH_BCAST                                      87
#define DCMF_USE_SCATTER_GATHER_BCAST                                     88
#define DCMF_USE_TREE_BCAST                                               89
#define DCMF_USE_MPICH_BCAST                                              90
#define DCMF_BCAST_RESERVED1                                              91
#define DCMF_BCAST_RESERVED2                                              92

/* Exscan in case someone implements something */
#define DCMF_EXSCAN_F_INDEX                                               93
#define DCMF_EXSCAN_L_INDEX                                               93
/*---------------------------------------------------------------------------*/
#define DCMF_USE_MPICH_EXSCAN                                             93
#define DCMF_EXSCAN_RESERVED1                                             94

/* Gather protocols */
#define DCMF_GATHER_F_INDEX                                               95
#define DCMF_GATHER_L_INDEX                                               96
/*---------------------------------------------------------------------------*/
#define DCMF_USE_REDUCE_GATHER                                            95
#define DCMF_USE_MPICH_GATHER                                             96
#define DCMF_GATHER_RESERVED1                                             97

/* Gatherv protocols in case someone implements something */
#define DCMF_GATHERV_F_INDEX                                              98
#define DCMF_GATHERV_L_INDEX                                              98
/*---------------------------------------------------------------------------*/
#define DCMF_USE_MPICH_GATHERV                                            98
#define DCMF_GATHERV_RESERVED1                                            99

/* Reduce protocols */
#define DCMF_REDUCE_F_INDEX                                               100
#define DCMF_REDUCE_L_INDEX                                               105
/*---------------------------------------------------------------------------*/
#define DCMF_USE_BINOM_REDUCE                                             100
#define DCMF_USE_CCMI_TREE_REDUCE                                         101
#define DCMF_USE_RECT_REDUCE                                              102
#define DCMF_USE_RECTRING_REDUCE                                          103
#define DCMF_USE_TREE_REDUCE                                              104
#define DCMF_USE_MPICH_REDUCE                                             105
/* Controls whether or not we reuse storage in reduce */
#define DCMF_REDUCE_RESERVED1                                             106
#define DCMF_REDUCE_RESERVED2                                             107
#define DCMF_USE_STORAGE_REDUCE                                           108

/* Reduce_scatter protocols */
#define DCMF_REDUCESCATTER_F_INDEX                                        109
#define DCMF_REDUCESCATTER_L_INDEX                                        110
/*---------------------------------------------------------------------------*/
#define DCMF_USE_REDUCESCATTER                                            109
#define DCMF_USE_MPICH_REDUCESCATTER                                      110
#define DCMF_REDUCESCATTER_RESERVED1                                      111

/* Scan protocols in case someone implements something */
#define DCMF_SCAN_F_INDEX                                                 112
#define DCMF_SCAN_INDEX                                                   112
/*---------------------------------------------------------------------------*/
#define DCMF_USE_MPICH_SCAN                                               112
#define DCMF_SCAN_RESERVED1                                               113
                                              
/* Scatter protocols */                                              
#define DCMF_SCATTER_F_INDEX                                              114
#define DCMF_SCATTER_L_INDEX                                              115
/*---------------------------------------------------------------------------*/
#define DCMF_USE_BCAST_SCATTER                                            114  
#define DCMF_USE_MPICH_SCATTER                                            115
#define DCMF_SCATTER_RESERVED1                                            116

/* Scatterv protocols */
#define DCMF_SCATTERV_F_INDEX                                             117
#define DCMF_SCATTERV_L_INDEX                                             120
/*---------------------------------------------------------------------------*/
#define DCMF_USE_BCAST_SCATTERV                                           117
#define DCMF_USE_ALLTOALL_SCATTERV                                        118
#define DCMF_USE_ALLREDUCE_SCATTERV                                       119
#define DCMF_USE_MPICH_SCATTERV                                           120
#define DCMF_SCATTERV_RESERVED1                                           121
#define DCMF_USE_PREALLREDUCE_SCATTERV                                    122

#endif
