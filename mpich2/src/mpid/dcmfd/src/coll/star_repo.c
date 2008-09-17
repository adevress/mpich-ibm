/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/STAR_repo.c
 * \brief ???
 */
/******************************************************************************

Copyright (c) 2006, Ahmad Faraj & Xin Yuan,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    * Neither the name of the Florida State University nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***************************************************************************
*     Any results obtained from executing this software require the       *
*     acknowledgment and citation of the software and its owners.         *
*     The full citation is given below:                                   *
*                                                                         *
*     A. Faraj, X. Yuan, and D. Lowenthal. "STAR-MPI: Self Tuned Adaptive *
*     Routines for MPI Collective Operations." The 20th ACM International *
*     Conference on Supercomputing (ICS), Queensland, Australia           *
*     June 28-July 1, 2006.                                               *
***************************************************************************

******************************************************************************/

  /*
    Author: Ahmad Faraj
    contact: faraja@us.ibm.com
    date: 4/1/08
  */

#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"
#include <stdarg.h>


STAR_Algorithm * STAR_bcast_repository;
STAR_Algorithm * STAR_allreduce_repository;
STAR_Algorithm * STAR_reduce_repository;
STAR_Algorithm * STAR_allgather_repository;
STAR_Algorithm * STAR_allgatherv_repository;
STAR_Algorithm * STAR_alltoall_repository;
STAR_Algorithm * STAR_gather_repository;
STAR_Algorithm * STAR_scatter_repository;
STAR_Algorithm * STAR_barrier_repository;
  
STAR_Info STAR_info =
  {
    enabled: 0,
    internal_control_flow: 0,
    agree_on_callsite: 1,
    traceback_levels: 3,
    debug: 0,
    invocs_per_algorithm: 10,

    bcast_algorithms: 0,
    barrier_algorithms: 0,
    alltoall_algorithms: 0,
    allreduce_algorithms: 0,
    allgather_algorithms: 0,
    allgatherv_algorithms: 0,
    reduce_algorithms: 0,
    scatter_algorithms: 0,
    gather_algorithms: 0,

    mpis: {"NO_OP", "Alltoall", "Allgather", "Allgatherv", "Allreduce",
           "Bcast", "Reduce", "Gather",  "Scatter", "Barrier"}
  };

void STAR_SetRepositoryElement(STAR_Algorithm * element,
			       STAR_Func_Ptr func,
			       unsigned int min_np,
			       unsigned int max_np,
			       unsigned int min_bytes,
			       unsigned int max_bytes,
			       char * name,
			       ...) /* this is for different bit patterns */

{
  va_list arg_ptr;
  int value = 0;

  DCMF_INFO_ZERO(&(element -> properties));

  va_start(arg_ptr, name);

  value = va_arg(arg_ptr, int);
  
  do
  {
    DCMF_INFO_SET(&(element -> properties), value);
    value = va_arg(arg_ptr, int);      
  } while (value > DCMF_END_ARGS);
  
  va_end(arg_ptr);

  element -> func = func;

  element -> nprocs[0] = min_np;
  element -> nprocs[1] = max_np;

  element -> bytes[0] = min_bytes;
  element -> bytes[1] = max_bytes;

  strcpy(element -> name, name);
}

void STAR_InitRepositories()
{
  int curr, num, alg_size;
  
  alg_size = sizeof(STAR_Algorithm);

#ifdef USE_CCMI_COLL
  /* algorithms for Bcast */
  curr = 0;
  num = 9;
  STAR_info.bcast_algorithms = num;
  STAR_bcast_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_tree),
			    0, 0,
			    1, 0,
			    "bcast_tree",
			    DCMF_TREE_COMM,
			    DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_rect_sync),
			    0, 0,
			    0, 0,
			    "bcast_rect",
			    DCMF_RECT_COMM,
			    DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_rect_async),
			    0, 0,
			    0, 0,
			    "bcast_async_rect",
			    DCMF_RECT_COMM,
			    DCMF_END_ARGS);
			    
  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_binom_sync),
			    0, 0,
			    0, 0,
			    "bcast_binom",
			    DCMF_TORUS_COMM,
			    DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_binom_async),
			    0, 0,
			    0, 0,
			    "bcast_async_binom",
			    DCMF_TORUS_COMM,
			    DCMF_END_ARGS);
			    
  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_scatter_gather),
			    0, 0,
			    0, 0,
			    "bcast_scatter_gather",
			    DCMF_IRREG_COMM,
			    DCMF_END_ARGS);
			    

  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_rect_dput),
			    0, 0,
			    0, 0,
			    "bcast_rect_directput",
			    DCMF_RECT_COMM,
			    DCMF_SINGLE_THREAD_MODE,
			    DCMF_BUFF_ALIGNED,
			    DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_binom_singleth),
			    0, 0,
			    0, 0,
			    "bcast_binom_single_thread",
			    DCMF_TORUS_COMM,
			    DCMF_SINGLE_THREAD_MODE,
			    DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Bcast_rect_singleth),
			    0, 0,
			    0, 0,
			    "bcast_rect_single_thread",
			    DCMF_RECT_COMM,
			    DCMF_SINGLE_THREAD_MODE,
			    DCMF_END_ARGS);
 
  /*
    STAR_SetRepositoryElement(&STAR_bcast_repository[curr++],
    (STAR_Func_Ptr) (&MPIDO_Bcast_flattree),
    0, 0,
    0, 0,
    "bcast_flattree",
    DCMF_IRREG_COMM,
    DCMF_END_ARGS);
  */

  /* algorithms for Allreduce */
  curr = 0;
  num = 7; 
  STAR_info.allreduce_algorithms = num;
  STAR_allreduce_repository = malloc(alg_size * num);
  /*
    STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
    (STAR_Func_Ptr)(&MPIDO_Allreduce_pipelined_tree),
    0, 0,
    0, 0,
    "allreduce_pipelined_tree",
    DCMF_TREE_COMM, DCMF_TREE_OP_TYPE, DCMF_END_ARGS);
  
			    
    STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
    (STAR_Func_Ptr) (&MPIDO_Allreduce_tree),
    0, 0,
    0, 0,
    "allreduce_tree",
    DCMF_TREE_COMM, DCMF_TREE_OP_TYPE, DCMF_END_ARGS);
  */
  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_global_tree),
			    0, 0,
			    0, 0,
			    "allreduce_global_tree",
			    DCMF_TREE_COMM, DCMF_TREE_OP_TYPE, DCMF_END_ARGS);
    

  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_binom),
			    0, 0,
			    0, 0,
			    "allreduce_binom",
			    DCMF_TORUS_COMM, DCMF_TORUS_OP_TYPE,DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_rect),
			    0, 0,
			    0, 0,
			    "allreduce_rect",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_rectring),
			    0, 0,
			    0, 0,
			    "allreduce_rectring",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_async_binom),
			    0, 0,
			    0, 0,
			    "allreduce_async_binom",
			    DCMF_TORUS_COMM, DCMF_TORUS_OP_TYPE,DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_async_rect),
			    0, 0,
			    0, 0,
			    "allreduce_async_rect",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_allreduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allreduce_async_rectring),
			    0, 0,
			    0, 0,
			    "allreduce_async_rectring",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);

  /* algorithms for Reduce */
  curr = 0;
  num = 4; /* 5 */
  STAR_info.reduce_algorithms = num;
  STAR_reduce_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_reduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Reduce_global_tree),
			    0, 0,
			    0, 0,
			    "reduce_global_tree",
			    DCMF_TREE_COMM, DCMF_TREE_OP_TYPE, DCMF_END_ARGS);

  /*
    STAR_SetRepositoryElement(&STAR_reduce_repository[curr++],
    (STAR_Func_Ptr) (&MPIDO_Reduce_tree),
    0, 0,
    0, 0,
    "reduce_tree",
    DCMF_TREE_COMM, DCMF_TREE_OP_TYPE, DCMF_END_ARGS);
  */

  STAR_SetRepositoryElement(&STAR_reduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Reduce_binom),
			    0, 0,
			    0, 0,
			    "reduce_binom",
			    DCMF_TORUS_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);
  
  STAR_SetRepositoryElement(&STAR_reduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Reduce_rect),
			    0, 0,
			    0, 0,
			    "reduce_rect",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_reduce_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Reduce_rectring),
			    0, 0,
			    0, 0,
			    "reduce_rectring",
			    DCMF_RECT_COMM, DCMF_TORUS_OP_TYPE, DCMF_END_ARGS);
  
  /* algorithms for alltoall */
  curr = 0;
  num = 2;
  STAR_info.alltoall_algorithms = num;
  STAR_alltoall_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_alltoall_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Alltoall_torus),
			    0, 0,
			    0, 0,
			    "alltoall_torus",
			    DCMF_USE_TORUS_ALLTOALL, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_alltoall_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Alltoall_simple),
			    0, 0,
			    0, 0,
			    "alltoall_simple",
			    DCMF_TORUS_COMM, DCMF_END_ARGS);

  /* algorithms for allgather */
  curr = 0;
  num = 5;
  STAR_info.allgather_algorithms = num;
  STAR_allgather_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgather_bcast),
			    0, 0,
			    0, 0,
			    "allgather_bcast",
			    DCMF_USE_BCAST_ALLGATHER,
                            DCMF_END_ARGS);
  /*
    STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
    (STAR_Func_Ptr) (&MPIDO_Allgather_bcast),
    0, 0,
    0, 0,
    "allgather_bcast",
    DCMF_USE_TREE_BCAST,
    DCMF_END_ARGS);
  */
  STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgather_allreduce),
			    0, 0,
			    0, 0,
			    "allgather_allreduce",
			    DCMF_USE_TREE_ALLREDUCE,
			    DCMF_SBUFF_CONTIG,
			    DCMF_RBUFF_CONTIG,
			    DCMF_RBUFF_CONTIN,
			    DCMF_BUFF_SIZE_MUL4, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgather_alltoall),
			    0, 0,
			    0, 0,
			    "allgather_alltoall",
			    DCMF_USE_TORUS_ALLTOALL,
                            DCMF_END_ARGS);


  STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
                            (STAR_Func_Ptr) (&MPIDO_Allgather_bcast_rect_async),
			    0, 0,
			    0, 0,
			    "allgather_bcast_rect_async",
			    DCMF_USE_ARECT_BCAST, DCMF_END_ARGS);


  STAR_SetRepositoryElement(&STAR_allgather_repository[curr++],
                            (STAR_Func_Ptr)(&MPIDO_Allgather_bcast_binom_async),
			    0, 0,
			    0, 0,
			    "allgather_bcast_binom_async",
			    DCMF_USE_ABINOM_BCAST, DCMF_END_ARGS);
  

  /* algorithms for allgatherv */
  curr = 0;
  num = 5;
  STAR_info.allgatherv_algorithms = num;
  STAR_allgatherv_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_allgatherv_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgatherv_bcast),
			    0, 0,
			    0, 0,
			    "allgatherv_bcast",
			    DCMF_USE_BCAST_ALLGATHERV, DCMF_END_ARGS);


  STAR_SetRepositoryElement(&STAR_allgatherv_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgatherv_allreduce),
			    0, 0,
			    0, 0,
			    "allgatherv_allreduce",
			    DCMF_USE_ALLREDUCE_ALLGATHERV,
			    DCMF_USE_TREE_ALLREDUCE,
			    DCMF_SBUFF_CONTIG,
			    DCMF_RBUFF_CONTIG,
			    DCMF_RBUFF_CONTIN,
			    DCMF_BUFF_SIZE_MUL4, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allgatherv_repository[curr++],
                            (STAR_Func_Ptr)(&MPIDO_Allgatherv_bcast_rect_async),
			    0, 0,
			    0, 0,
			    "allgatherv_bcast_rect_async",
			    DCMF_USE_ARECT_BCAST_ALLGATHERV,
			    DCMF_USE_ARECT_BCAST, DCMF_END_ARGS);


  STAR_SetRepositoryElement(&STAR_allgatherv_repository[curr++],
                            (STAR_Func_Ptr)(&MPIDO_Allgatherv_bcast_binom_async),
			    0, 0,
			    0, 0,
			    "allgatherv_bcast_binom_async",
			    DCMF_USE_ABINOM_BCAST_ALLGATHERV,
			    DCMF_USE_ABINOM_BCAST, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_allgatherv_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Allgatherv_alltoall),
			    0, 0,
			    0, 0,
			    "allgatherv_alltoall",
			    DCMF_USE_TORUS_ALLTOALL,
			    DCMF_USE_ALLTOALL_ALLGATHERV, DCMF_END_ARGS);

  /* algorithms for gather */
  curr = 0;
  num = 1;
  STAR_info.gather_algorithms = num;
  STAR_gather_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_gather_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Gather_reduce),
			    0, 0,
			    0, 0,
			    "gather_reduce",
			    DCMF_USE_REDUCE_GATHER, DCMF_END_ARGS);

  /* algorithms for scatter */
  curr = 0;
  num = 1;
  STAR_info.scatter_algorithms = num;
  STAR_scatter_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_scatter_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Scatter_bcast),
			    0, 0,
			    0, 0,
			    "scatter_bcast",
			    DCMF_USE_BCAST_SCATTER, DCMF_END_ARGS);

  /* algorithms for barrier */
  curr = 0;
  num = 2;
  STAR_info.barrier_algorithms = num;
  STAR_barrier_repository = malloc(alg_size * num);

  STAR_SetRepositoryElement(&STAR_barrier_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Barrier_gi),
			    0, 0,
			    0, 0,
			    "barrier_gi",
			    DCMF_USE_GI_BARRIER, DCMF_END_ARGS);

  STAR_SetRepositoryElement(&STAR_barrier_repository[curr++],
			    (STAR_Func_Ptr) (&MPIDO_Barrier_dcmf),
			    0, 0,
			    0, 0,
			    "barrier_dcmf",
			    DCMF_TORUS_COMM, DCMF_END_ARGS);
#else /* !USE_CCMI_COLL */
  STAR_bcast_repository = NULL;
  STAR_allreduce_repository = NULL;
  STAR_reduce_repository = NULL;
  STAR_alltoall_repository = NULL;
  STAR_allgather_repository = NULL;
  STAR_allgatherv_repository = NULL;
  STAR_gather_repository = NULL;
  STAR_scatter_repository = NULL;
  STAR_barrier_repository = NULL;
#endif /* !USE_CCMI_COLL */
}

