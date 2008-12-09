/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/reduce_scatter/mpido_reduce_scatter.c
 * \brief Function prototypes for the optimized collective routines
 */

#include "mpido_coll.h"

#ifdef USE_CCMI_COLL

#pragma weak PMPIDO_Reduce_scatter = MPIDO_Reduce_scatter

/* Call optimized reduce+scatterv - should be faster than pt2pt on
 * larger partitions 
 */


int MPIDO_Reduce_scatter(void *sendbuf,
                         void *recvbuf,
                         int *recvcounts,
                         MPI_Datatype datatype,
                         MPI_Op op,
                         MPID_Comm * comm_ptr)
{
  DCMF_Embedded_Info_Set * properties = &(comm_ptr->dcmf.properties);
  int tcount=0, i, rc;
  
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_lb=0, extent=0;
  
  char *tempbuf;
  char *sbuf = sendbuf;
  int *displs;
  int size = comm_ptr->local_size;

  if(DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_REDUCESCATTER) ||
     !DCMF_INFO_ISSET(properties, DCMF_USE_REDUCESCATTER) ||
     recvcounts[0] < 256 || 
     !MPIDI_IsTreeOp(op, datatype))
    return MPIR_Reduce_scatter(sendbuf, 
			       recvbuf, 
			       recvcounts, 
			       datatype, 
			       op, 
			       comm_ptr);


   NMPI_Type_get_true_extent(datatype, &dt_lb, &extent);
   MPID_Ensure_Aint_fits_in_int(extent);

   displs = MPIU_Malloc(size * sizeof(int));
   if(!displs)
    return MPIR_Err_create_code(MPI_SUCCESS, 
                                MPIR_ERR_RECOVERABLE,
                                "MPI_Reduce_scatter",
                                __LINE__, MPI_ERR_OTHER, "**nomem", 0);
  
   displs[0] = 0;
   for(i = 0; i < size-1; i++)
   {
      tcount += recvcounts[i];
      displs[i+1]=displs[i]+recvcounts[i];
   }
   tcount+=recvcounts[size-1];

  
  tempbuf = MPIU_Malloc(extent * sizeof(char) * tcount);
  
  if (!tempbuf)
  {
    if(displs)
      MPIU_Free(displs);
    return MPIR_Err_create_code(MPI_SUCCESS, 
                                MPIR_ERR_RECOVERABLE,
                                "MPI_Reduce_scatter",
                                __LINE__, MPI_ERR_OTHER, "**nomem", 0);
  }
  
  MPIDI_VerifyBuffer(sendbuf, sbuf, dt_lb);
  
  rc = MPIDO_Reduce(sbuf,
		    tempbuf, 
		    tcount, 
		    datatype, 
		    op, 
		    0, 
		    comm_ptr);
  
  /* rank 0 has the entire buffer, need to split out our individual 
     piece does recvbuf need a dt_lb added? */
  if (rc == MPI_SUCCESS)
  {
    rc = MPIDO_Scatterv(tempbuf, 
                        recvcounts, 
                        displs, 
                        datatype,
                        recvbuf, 
                        tcount, 
                        datatype, 
                        0, 
                        comm_ptr);
  }
  
  MPIU_Free(tempbuf);
  MPIU_Free(displs);

  return rc;
}
  

#endif
