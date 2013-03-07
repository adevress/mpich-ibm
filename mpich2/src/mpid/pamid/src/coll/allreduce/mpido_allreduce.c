/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/allreduce/mpido_allreduce.c
 * \brief ???
 */

/* #define TRACE_ON */

#include <mpidimpl.h>
/* 
#undef TRACE_ERR
#define TRACE_ERR(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
*/
static void cb_allreduce(void *ctxt, void *clientdata, pami_result_t err)
{
  int *active = (int *) clientdata;
  TRACE_ERR("callback enter, active: %d\n", (*active));
  MPIDI_Progress_signal();
  (*active)--;
}

int MPIDO_Allreduce(const void *sendbuf,
                    void *recvbuf,
                    int count,
                    MPI_Datatype dt,
                    MPI_Op op,
                    MPID_Comm *comm_ptr,
                    int *mpierrno)
{
  void *sbuf;
  TRACE_ERR("Entering mpido_allreduce\n");
  pami_type_t pdt;
  pami_data_function pop = (pami_data_function)NULL;
  int mu;
  int rc;
#ifdef TRACE_ON
  int len; 
  char op_str[255]; 
  char dt_str[255]; 
  MPIDI_Op_to_string(op, op_str); 
  PMPI_Type_get_name(dt, dt_str, &len); 
#endif
  volatile unsigned active = 1;
  pami_xfer_t allred;
  pami_algorithm_t my_allred = 0;
  const pami_metadata_t *my_md = (pami_metadata_t *)NULL;
  int alg_selected = 0;
  const int rank = comm_ptr->rank;
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
#if ASSERT_LEVEL==0
  /* We can't afford the tracing in ndebug/performance libraries */
  const unsigned verbose = 0;
#else
  const unsigned verbose = (MPIDI_Process.verbose >= MPIDI_VERBOSE_DETAILS_ALL) && (rank == 0);
#endif
  int queryreq = 0;
  if(likely((mpid->cached_allreduce_type == MPID_COLL_QUERY)&&(dt == MPI_DOUBLE || dt == MPI_DOUBLE_PRECISION) && (count != 0)))
  {
    /* double protocol works on all message sizes */
    if(likely(op == MPI_SUM))
      pop = PAMI_DATA_SUM;
    else if(likely(op == MPI_MAX))
      pop = PAMI_DATA_MAX;
    else if(likely(op == MPI_MIN))
      pop = PAMI_DATA_MIN;
    if(likely(pop != (pami_data_function)NULL))
    {
      allred.cb_done = cb_allreduce;
      allred.cookie = (void *)&active;
      allred.cmd.xfer_allreduce.sndbuf = (void *)sendbuf;
      if(sendbuf == MPI_IN_PLACE)
        allred.cmd.xfer_allreduce.sndbuf = PAMI_IN_PLACE;
      allred.cmd.xfer_allreduce.stype = PAMI_TYPE_DOUBLE;
      allred.cmd.xfer_allreduce.rcvbuf = recvbuf;
      allred.cmd.xfer_allreduce.rtype = PAMI_TYPE_DOUBLE;
      allred.cmd.xfer_allreduce.stypecount = count;
      allred.cmd.xfer_allreduce.rtypecount = count;
      allred.cmd.xfer_allreduce.op = pop;
      allred.algorithm = mpid->cached_allreduce;

      if(unlikely(verbose))
      {
        unsigned long long int threadID;
        MPIU_Thread_id_t tid;
        MPIU_Thread_self(&tid);
        threadID = (unsigned long long int)tid;
        fprintf(stderr,"<%llx> Using cached protocol %s for allreduce (count = %d) on %u\n", 
                threadID,
                mpid->cached_allreduce_md.name,
                count,
                (unsigned) comm_ptr->context_id);
      }

      MPIDI_Post_coll_t allred_post;
      MPIDI_Context_post(MPIDI_Context[0], &allred_post.state,
                         MPIDI_Pami_post_wrapper, (void *)&allred);

      MPIDI_Update_last_algorithm(comm_ptr,mpid->cached_allreduce_md.name);
      MPID_PROGRESS_WAIT_WHILE(active);
      TRACE_ERR("allreduce done\n");
      return MPI_SUCCESS;
    }
  }

  rc = MPIDI_Datatype_to_pami(dt, &pdt, op, &pop, &mu);

  if(unlikely(verbose))
    fprintf(stderr,"allred rc %u,count %d, Datatype %p, op %p, mu %u, selectedvar %u, sendbuf %p, recvbuf %p\n",
            rc, count, pdt, pop, mu, 
            (unsigned)mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][0], sendbuf, recvbuf);

  /* Punt count 0 allreduce to MPICH. Let them do whatever's 'right' */
  if(unlikely(rc != MPI_SUCCESS || (count==0) || (mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][0] == MPID_COLL_USE_MPICH)))
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH allreduce\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
    return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
  }

  sbuf = (void *)sendbuf;
  if(sendbuf == MPI_IN_PLACE)
  {
    if(unlikely(verbose))
      fprintf(stderr,"allreduce MPI_IN_PLACE buffering\n");
    sbuf = PAMI_IN_PLACE;
  }

  allred.cb_done = cb_allreduce;
  allred.cookie = (void *)&active;
  allred.cmd.xfer_allreduce.sndbuf = sbuf;
  allred.cmd.xfer_allreduce.stype = pdt;
  allred.cmd.xfer_allreduce.rcvbuf = recvbuf;
  allred.cmd.xfer_allreduce.rtype = pdt;
  allred.cmd.xfer_allreduce.stypecount = count;
  allred.cmd.xfer_allreduce.rtypecount = count;
  allred.cmd.xfer_allreduce.op = pop;

  TRACE_ERR("Allreduce - Basic Collective Selection\n");

  MPI_Aint data_true_lb;
  MPID_Datatype *data_ptr;
  int data_size, data_contig;
  MPIDI_Datatype_get_info(count, dt, data_contig, data_size, data_ptr, data_true_lb); 
  if(likely(mpid->cached_allreduce_type < MPID_COLL_USE_MPICH)) /* Optimized cached protocol selected */
  { 
    my_allred = mpid->cached_allreduce;
    my_md = &mpid->cached_allreduce_md;
    alg_selected = 1;
    if(mpid->cached_allreduce_type == MPID_COLL_QUERY)
    {
      if(my_md->check_fn != NULL)/*This should always be the case in FCA.. Otherwise punt to mpich*/
      {
        metadata_result_t result = {0};
        TRACE_ERR("querying allreduce algorithm %s\n",
                  my_md->name);
        result = my_md->check_fn(&allred);
        TRACE_ERR("bitmask: %#X\n", result.bitmask);
        /* \todo Ignore check_correct.values.nonlocal until we implement the
                 'pre-allreduce allreduce' or the 'safe' environment flag.
                 We will basically assume 'safe' -- that all ranks are aligned (or not).
        */
        result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
        if(!result.bitmask)
          ; /* ok, algorithm selected */
        else
        {
          alg_selected = 0;
          if(unlikely(verbose))
            fprintf(stderr,"check_fn failed for %s.\n", my_md->name);
        }
      }
      else /* no check_fn, manually look at the metadata fields */
      {
        TRACE_ERR("Optimzed selection line %d\n",__LINE__);
        /* Check if the message range if restricted */
        if(my_md->check_correct.values.rangeminmax)
        {
          if((my_md->range_lo <= data_size) &&
             (my_md->range_hi >= data_size))
            ; /* ok, algorithm selected */
          else
          {
            if(unlikely(verbose))
              fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                      data_size,
                      my_md->range_lo,
                      my_md->range_hi,
                      my_md->name);
            alg_selected = 0;
          }
        }
        /* \todo check the rest of the metadata */
      }
    }
  }

  /* Didn't select cached algorithm or metadata query failed */
  if(alg_selected ==0)
  { /* Try optimized (and maybe cutoff) algorithm(s) */
    TRACE_ERR("Non-Optimzed selection line %d\n",__LINE__);
    if((mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLREDUCE][0] == 0) || (data_size <= mpid->optimized_algorithm_cutoff_size[PAMI_XFER_ALLREDUCE][0]))
    {
      my_allred = mpid->optimized_algorithm[PAMI_XFER_ALLREDUCE][0];
      my_md = &mpid->optimized_algorithm_metadata[PAMI_XFER_ALLREDUCE][0];
      queryreq = mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][0];
      if((queryreq == MPID_COLL_QUERY) ||
         (queryreq == MPID_COLL_DEFAULT_QUERY))
      {
        TRACE_ERR("Non-Optimzed selection line %d\n",__LINE__);
        if(my_md->check_fn != NULL)
        {
          metadata_result_t result = {0};
          TRACE_ERR("querying allreduce algorithm %s, type was %d\n",
                    my_md->name,
                    mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][0]);
          result = my_md->check_fn(&allred);
          TRACE_ERR("bitmask: %#X\n", result.bitmask);
          /* \todo Ignore check_correct.values.nonlocal until we implement the
             'pre-allreduce allreduce' or the 'safe' environment flag.
             We will basically assume 'safe' -- that all ranks are aligned (or not).
          */
          result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
          if(!result.bitmask)
            alg_selected = 1; /* query algorithm successfully selected */
          else
            if(unlikely(verbose))
            fprintf(stderr,"check_fn failed for %s.\n", my_md->name);
        }
        else /* no check_fn, manually look at the metadata fields */
        {
          TRACE_ERR("Non-Optimzed selection line %d\n",__LINE__);
          /* Check if the message range if restricted */
          if(my_md->check_correct.values.rangeminmax)
          {
            if((my_md->range_lo <= data_size) &&
               (my_md->range_hi >= data_size))
              alg_selected = 1; /* query algorithm successfully selected */
            else
              if(unlikely(verbose))
              fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                      data_size,
                      my_md->range_lo,
                      my_md->range_hi,
                      my_md->name);
          }
          /* \todo check the rest of the metadata */
        }
      }
      else alg_selected = 1; /* non-query algorithm selected */
    }

    /* Didn't select first, small message optimized algorithm (or metadata query failed) */
    if((alg_selected==0) && (mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][1] != MPID_COLL_USE_MPICH))
    { /* Try large message, use [1] */
      my_allred = mpid->optimized_algorithm[PAMI_XFER_ALLREDUCE][1];
      my_md = &mpid->optimized_algorithm_metadata[PAMI_XFER_ALLREDUCE][1];
      queryreq = mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][1];
      if((queryreq  == MPID_COLL_QUERY) ||
         (queryreq == MPID_COLL_DEFAULT_QUERY))
      {
        TRACE_ERR("Non-Optimzed selection line %d\n",__LINE__);
        if(my_md->check_fn != NULL)
        {
          metadata_result_t result = {0};
          TRACE_ERR("querying allreduce algorithm %s, type was %d\n",
                    my_md->name,
                    mpid->optimized_algorithm_type[PAMI_XFER_ALLREDUCE][1]);
          result = my_md->check_fn(&allred);
          TRACE_ERR("bitmask: %#X\n", result.bitmask);
          /* \todo Ignore check_correct.values.nonlocal until we implement the
             'pre-allreduce allreduce' or the 'safe' environment flag.
             We will basically assume 'safe' -- that all ranks are aligned (or not).
          */
          result.check.nonlocal = 0; /* #warning REMOVE THIS WHEN IMPLEMENTED */
          if(!result.bitmask)
            alg_selected = 1; /* query algorithm successfully selected */
          else
            if(unlikely(verbose))
            fprintf(stderr,"check_fn failed for %s.\n", my_md->name);
        }
        else /* no check_fn, manually look at the metadata fields */
        {
          TRACE_ERR("Non-Optimzed selection line %d\n",__LINE__);
          /* Check if the message range if restricted */
          if(my_md->check_correct.values.rangeminmax)
          {
            if((my_md->range_lo <= data_size) &&
               (my_md->range_hi >= data_size))
              alg_selected = 1; /* query algorithm successfully selected */
            else
              if(unlikely(verbose))
              fprintf(stderr,"message size (%u) outside range (%zu<->%zu) for %s.\n",
                      data_size,
                      my_md->range_lo,
                      my_md->range_hi,
                      my_md->name);
          }
          /* \todo check the rest of the metadata */
        }
      }
      else alg_selected = 1; /* non-query algorithm selected */
    }
  }

  /* Didn't select any optimized algorithm (or metadata query failed) */
  if(unlikely(!alg_selected)) /* must be fallback to MPICH */
  {
    if(unlikely(verbose))
      fprintf(stderr,"Using MPICH allreduce\n");
    MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
    return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
  }

  /* Use whatever algorithm was selected */
  allred.algorithm = my_allred;

  if(unlikely(verbose))
  {
    unsigned long long int threadID;
    MPIU_Thread_id_t tid;
    MPIU_Thread_self(&tid);
    threadID = (unsigned long long int)tid;
    fprintf(stderr,"<%llx> Using protocol %s for allreduce on %u\n", 
            threadID,
            my_md->name,
            (unsigned) comm_ptr->context_id);
  }

  MPIDI_Post_coll_t allred_post;
  MPIDI_Context_post(MPIDI_Context[0], &allred_post.state,
                     MPIDI_Pami_post_wrapper, (void *)&allred);

  MPIDI_Update_last_algorithm(comm_ptr,my_md->name);
  MPID_PROGRESS_WAIT_WHILE(active);
  TRACE_ERR("allreduce done\n");
  return MPI_SUCCESS;
}

#ifndef __BGQ__
int MPIDO_Allreduce_simple(const void *sendbuf,
                           void *recvbuf,
                           int count,
                           MPI_Datatype dt,
                           MPI_Op op,
                           MPID_Comm *comm_ptr,
                           int *mpierrno)
{
  void *sbuf;
  TRACE_ERR("Entering MPIDO_Allreduce_optimized\n");
  pami_type_t pdt;
  pami_data_function pop;
  int mu;
  int rc;
#ifdef TRACE_ON
  int len; 
  char op_str[255]; 
  char dt_str[255]; 
  MPIDI_Op_to_string(op, op_str); 
  PMPI_Type_get_name(dt, dt_str, &len); 
#endif
  volatile unsigned active = 1;
  pami_xfer_t allred;
  const pami_metadata_t *my_allred_md = (pami_metadata_t *)NULL;
  const int rank = comm_ptr->rank;
  const struct MPIDI_Comm* const mpid = &(comm_ptr->mpid);
  MPID_Datatype *data_ptr;
  MPI_Aint data_true_lb = 0;
  int data_size, data_contig;

  MPIDI_Datatype_get_info(1, dt,
                          data_contig, data_size, data_ptr, data_true_lb);


  if(MPIDI_Pamix_collsel_advise != NULL)
  {
    advisor_algorithm_t advisor_algorithms[1];
    int num_algorithms = MPIDI_Pamix_collsel_advise(mpid->collsel_fast_query, PAMI_XFER_ALLREDUCE, data_size * count, advisor_algorithms, 1);
    if(num_algorithms)
    {
      if(advisor_algorithms[0].algorithm_type == COLLSEL_EXTERNAL_ALGO)
      {
        return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
      }
    }
  }


  rc = MPIDI_Datatype_to_pami(dt, &pdt, op, &pop, &mu);

  /* convert to metadata query */
  /* Punt count 0 allreduce to MPICH. Let them do whatever's 'right' */
  if(unlikely(rc != MPI_SUCCESS || (count==0)))
  {
    MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
    return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
  }

  if(!data_contig)
  {
    MPIDI_Update_last_algorithm(comm_ptr, "ALLREDUCE_MPICH");
    return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr, mpierrno);
  }

  sbuf = (void *)sendbuf;
  if(unlikely(sendbuf == MPI_IN_PLACE))
  {
    sbuf = PAMI_IN_PLACE;
  }

  allred.cb_done = cb_allreduce;
  allred.cookie = (void *)&active;
  allred.cmd.xfer_allreduce.sndbuf = sbuf;
  allred.cmd.xfer_allreduce.stype = pdt;
  allred.cmd.xfer_allreduce.rcvbuf = recvbuf;
  allred.cmd.xfer_allreduce.rtype = pdt;
  allred.cmd.xfer_allreduce.stypecount = count;
  allred.cmd.xfer_allreduce.rtypecount = count;
  allred.cmd.xfer_allreduce.op = pop;
  allred.algorithm = mpid->algorithm_list[PAMI_XFER_ALLREDUCE][0][0];
  my_allred_md = &mpid->algorithm_metadata_list[PAMI_XFER_ALLREDUCE][0][0];

  MPIDI_Post_coll_t allred_post;
  MPIDI_Context_post(MPIDI_Context[0], &allred_post.state,
                     MPIDI_Pami_post_wrapper, (void *)&allred);

  MPID_assert(rc == PAMI_SUCCESS);
  MPIDI_Update_last_algorithm(comm_ptr,my_allred_md->name);
  MPID_PROGRESS_WAIT_WHILE(active);
  TRACE_ERR("Allreduce done\n");
  return MPI_SUCCESS;
}

int
MPIDO_CSWrapper_allreduce(pami_xfer_t *allreduce,
                          void        *comm)
{
  int mpierrno = 0;
  MPID_Comm   *comm_ptr = (MPID_Comm*)comm;
  MPI_Datatype type;
  MPI_Op op;
  void *sbuf;
  MPIDI_coll_check_in_place(allreduce->cmd.xfer_allreduce.sndbuf, &sbuf);
  int rc = MPIDI_Dtpami_to_dtmpi(  allreduce->cmd.xfer_allreduce.stype,
                                   &type,
                                   allreduce->cmd.xfer_allreduce.op,
                                   &op);
  if(rc == -1) return rc;

  rc  =  MPIR_Allreduce(sbuf,
                        allreduce->cmd.xfer_allreduce.rcvbuf,
                        allreduce->cmd.xfer_allreduce.rtypecount,
                        type, op, comm_ptr, &mpierrno);
  if(allreduce->cb_done && rc == 0)
    allreduce->cb_done(NULL, allreduce->cookie, PAMI_SUCCESS);
  return rc;

}
#endif
