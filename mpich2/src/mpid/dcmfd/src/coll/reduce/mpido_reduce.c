/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/reduce/mpido_reduce.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Reduce = MPIDO_Reduce

#ifdef USE_CCMI_COLL

int MPIDO_Reduce(void * sendbuf,
                 void * recvbuf,
                 int count,
                 MPI_Datatype datatype,
                 MPI_Op op,
                 int root,
                 MPID_Comm * comm)
{
  reduce_fptr func = NULL;
  DCMF_Embedded_Info_Set * properties = &(comm -> dcmf.properties);
  int success = 1, rc, op_type_support;
  int data_contig, data_size;
  unsigned char reset_sendbuff = 0;
  MPID_Datatype * data_ptr;
  MPI_Aint data_true_lb = 0;

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
  recvbuf = (char *) recvbuf + data_true_lb;

  if (sendbuf == MPI_IN_PLACE)
    {
      sendbuf = recvbuf;
      reset_sendbuff = 1;
    }
  else
    {
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				       data_true_lb);
      sendbuf = (char *) sendbuf + data_true_lb;
    }

  extern int DCMF_TREE_SMP_SHORTCUT;

  
  if (op_type_support == DCMF_TREE_SUPPORT &&
      DCMF_INFO_ISSET(properties, DCMF_USE_TREE_REDUCE))
    {
      if (DCMF_TREE_SMP_SHORTCUT)
	func = MPIDO_Reduce_global_tree;
      else
	func = MPIDO_Reduce_tree;
    }
  
  else if (op_type_support == DCMF_TORUS_SUPPORT ||
	   op_type_support == DCMF_TREE_SUPPORT)
    {
      if (DCMF_INFO_ISSET(properties, DCMF_USE_RECT_REDUCE))
	func = MPIDO_Reduce_rect;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_USE_RECTRING_REDUCE) &&
	       count > 16384)
	func = MPIDO_Reduce_rectring;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_USE_BINOM_REDUCE))
	func = MPIDO_Reduce_binom;
    }
  
  if (func)
    rc = (func)(sendbuf, recvbuf, count, dcmf_data,
		dcmf_op, datatype, root, comm);      
  
  /* 
     if datatype or op are not device specific, or no func was found,
     default to mpich 
  */
  else
    {
      if (reset_sendbuff) sendbuf = MPI_IN_PLACE;
      rc = MPIR_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    }
  

  if (reset_sendbuff) sendbuf = MPI_IN_PLACE;
  
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
