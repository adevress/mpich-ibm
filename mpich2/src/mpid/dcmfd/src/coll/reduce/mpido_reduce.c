/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/reduce/mpido_reduce.c
 * \brief ???
 */

#include "mpidi_star.h"
#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"


#ifdef USE_CCMI_COLL

#pragma weak PMPIDO_Reduce = MPIDO_Reduce

int MPIDO_Reduce(void * sendbuf,
                 void * recvbuf,
                 int count,
                 MPI_Datatype datatype,
                 MPI_Op op,
                 int root,
                 MPID_Comm * comm)
{
  reduce_fptr func = NULL;
  DCMF_Embedded_Info_Set * properties = &(comm->dcmf.properties);
  int success = 1, rc = 0, op_type_support;
  int data_contig, data_size = 0;
  int userenvset = DCMF_INFO_ISSET(properties, DCMF_REDUCE_ENVVAR);
  MPID_Datatype * data_ptr;
  MPI_Aint data_true_lb = 0;
  char *sbuf = sendbuf, *rbuf = recvbuf;

  DCMF_Dt dcmf_data = DCMF_UNDEFINED_DT;
  DCMF_Op dcmf_op = DCMF_UNDEFINED_OP;

  if (datatype != MPI_DATATYPE_NULL && count > 0)
  {
    MPIDI_Datatype_get_info(count,
                            datatype,
                            data_contig,
                            data_size,
                            data_ptr,
                            data_true_lb);
    if (!data_contig) success = 0;
  }
  else
    success = 0;

  /* quick exit conditions */
  if (DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_REDUCE) ||
      HANDLE_GET_KIND(op) != HANDLE_KIND_BUILTIN || !success)
    return MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);

  op_type_support = MPIDI_ConvertMPItoDCMF(op, &dcmf_op, datatype, &dcmf_data);

  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   data_true_lb);
  rbuf = (char *) recvbuf + data_true_lb;

  if (sendbuf == MPI_IN_PLACE)
  {
    sbuf = rbuf;
  }
  else
  {
    MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
                                     data_true_lb);
    sbuf = (char *) sendbuf + data_true_lb;
  }
   
  if (!STAR_info.enabled || STAR_info.internal_control_flow)// ||
    //      ((op_type_support == DCMF_TREE_SUPPORT &&
    //  DCMF_INFO_ISSET(properties, DCMF_TREE_COMM)) ||
    // data_size <= STAR_info.threshold))
  {
    if(!userenvset)
    {
       
      if (op_type_support == DCMF_TREE_SUPPORT &&
          DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE))
      {
         if(DCMF_INFO_ISSET(properties, DCMF_USE_SMP_TREE_SHORTCUT))
          func = MPIDO_Reduce_global_tree;
        else
          func = MPIDO_Reduce_tree;
      }
       
      if (!func && op_type_support != DCMF_NOT_SUPPORTED)
      {
        if (DCMF_INFO_ISSET(properties, DCMF_IRREG_COMM))
          func = MPIDO_Reduce_binom;
         
        if (!func && (data_size <= 32768))
        {
          if (DCMF_INFO_ISSET(properties, DCMF_USE_BINOM_REDUCE))
            func = MPIDO_Reduce_binom;
        }
         
        if (!func && (data_size > 32768))
        {
          if (DCMF_INFO_ISSET(properties, DCMF_USE_RECTRING_REDUCE))
            func = MPIDO_Reduce_rectring;
        }
      }
    }
    else
    {
      if(DCMF_INFO_ISSET(properties, DCMF_USE_BINOM_REDUCE))
        func = MPIDO_Reduce_binom;
      if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE) && 
         DCMF_INFO_ISSET(properties, DCMF_USE_SMP_TREE_SHORTCUT) &&
         op_type_support == DCMF_TREE_SUPPORT)
      {
        func = MPIDO_Reduce_global_tree;
      }
      if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE) && 
         !DCMF_INFO_ISSET(properties, DCMF_USE_SMP_TREE_SHORTCUT) &&
         op_type_support == DCMF_TREE_SUPPORT)
        func = MPIDO_Reduce_tree;
      if(DCMF_INFO_ISSET(properties, DCMF_USE_RECTRING_REDUCE))
        func = MPIDO_Reduce_rectring;
    }
     
    if (func && !DCMF_INFO_ISSET(properties, DCMF_IRREG_COMM))
      rc = (func)(sbuf, rbuf, count, dcmf_data,
                  dcmf_op, datatype, root, comm);      
      
    else
    {
      rc = MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    }
  }
  else
  {
    int id;
    unsigned char same_callsite = 1;

    void ** tb_ptr = (void **) MPIU_Malloc(sizeof(void *) *
                                           STAR_info.traceback_levels);

    /* set the internal control flow to disable internal star tuning */
    STAR_info.internal_control_flow = 1;

    /* get backtrace info for caller to this func, use that as callsite_id */
    backtrace(tb_ptr, STAR_info.traceback_levels);
    id = (int) tb_ptr[STAR_info.traceback_levels - 1];

    /* find out if all participants agree on the callsite id */
    if (STAR_info.agree_on_callsite)
    {
      int tmp[2], result[2];
      tmp[0] = id;
      tmp[1] = ~id;
      MPIDO_Allreduce(tmp, result, 2, MPI_UNSIGNED_LONG, MPI_MAX, comm);
      if (result[0] != (~result[1]))
        same_callsite = 0;
    }

    if (same_callsite)
    {
      STAR_Callsite collective_site;

      /* create a signature callsite info for this particular call site */
      collective_site.call_type = REDUCE_CALL;
      collective_site.comm = comm;
      collective_site.bytes = data_size;
      collective_site.id = id;
      collective_site.op_type_support = op_type_support;


      rc = STAR_Reduce(sbuf, rbuf, count, dcmf_data, dcmf_op,
                       datatype, root, &collective_site,
                       STAR_reduce_repository,
                       STAR_info.reduce_algorithms);
    }

    if (rc == STAR_FAILURE || !same_callsite)
    {
      rc = MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    }
    /* unset the internal control flow */
    STAR_info.internal_control_flow = 0;

    MPIU_Free(tb_ptr);
  }  

  
  return rc;
}

#else /* !USE_CCMI_COLL */

int MPIDO_Reduce(void * sendbuf,
                 void * recvbuf,
                 int count,
                 MPI_Datatype datatype,
                 MPI_Op op,
                 int root,
                 MPID_Comm * comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
