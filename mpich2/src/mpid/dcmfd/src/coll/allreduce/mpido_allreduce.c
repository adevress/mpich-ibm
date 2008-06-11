/*  (C)Copyright IBM Corp.  2007, 2008  */

/**
 * \file src/coll/allreduce/mpido_allreduce.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Allreduce = MPIDO_Allreduce

int
MPIDO_Allreduce(void * sendbuf,
		void * recvbuf,
		int count,
		MPI_Datatype datatype,
		MPI_Op op,
		MPID_Comm * comm)
{
  int rank;
  MPI_Comm_rank(comm->handle, &rank);
  allreduce_fptr func = NULL;
  DCMF_Embedded_Info_Set * properties = &(comm -> dcmf.properties);
  DCMF_Dt dcmf_data = DCMF_UNDEFINED_DT;
  DCMF_Op dcmf_op = DCMF_UNDEFINED_OP;
  MPID_Datatype * data_ptr;
  MPI_Aint data_true_lb = 0;
  int rc, op_type_support, data_contig, data_size;  
  unsigned char reset_sendbuff = 0;

  if (count == 0) return MPI_SUCCESS;

  /* quick exit conditions */
  if (comm -> comm_kind != MPID_INTRACOMM || comm->local_size < 3 ||
      HANDLE_GET_KIND(op) != HANDLE_KIND_BUILTIN)
    return MPIR_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);

  
  MPIDI_Datatype_get_info(count, datatype, data_contig, data_size,
			  data_ptr, data_true_lb);
  
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   data_true_lb);
  if (sendbuf == MPI_IN_PLACE)
    {
      sendbuf = recvbuf;
      reset_sendbuff = 1;
    }
  else
    {
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				       data_true_lb);
      sendbuf = ((char *) sendbuf + data_true_lb);
    }

  op_type_support = MPIDI_ConvertMPItoDCMF(op, &dcmf_op, datatype, &dcmf_data);

  extern int DCMF_TREE_SMP_SHORTCUT;
  
  recvbuf = ((char *) recvbuf + data_true_lb);
  
  if (op_type_support == DCMF_TREE_SUPPORT &&
      DCMF_INFO_ISSET(properties, DCMF_TREE_ALLREDUCE))
    {
      if (DCMF_TREE_SMP_SHORTCUT)
	func = MPIDO_Allreduce_global_tree;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_TREE_CCMI_ALLREDUCE))
	func = MPIDO_Allreduce_tree;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_TREE_PIPE_ALLREDUCE))
	func = MPIDO_Allreduce_pipelined_tree;
    }
  
  else if (op_type_support == DCMF_TORUS_SUPPORT)
    {
      if (DCMF_INFO_ISSET(properties, DCMF_RECT_ALLREDUCE))
	func = MPIDO_Allreduce_rect;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_BINOM_ALLREDUCE))
	func = MPIDO_Allreduce_binom;
      
      else if (DCMF_INFO_ISSET(properties, DCMF_RECTRING_ALLREDUCE) &&
	       count > 16384)
	func = MPIDO_Allreduce_rectring;
    }
  
  if (func)
    rc = (func)(sendbuf, 
		recvbuf, 
		count, 
		dcmf_data, 
		dcmf_op, 
		datatype, 
		comm);
  
  /* 
     punt to MPICH in the case no optimized func is found or in the case 
     generic op/type is used
  */
  else
    {
      if (reset_sendbuff) sendbuf = MPI_IN_PLACE;
      rc = MPIR_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    }
  
  if (reset_sendbuff) sendbuf = MPI_IN_PLACE;
  
  return rc;
}
