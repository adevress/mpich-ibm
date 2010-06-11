/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/scatter/scatter_algorithms.c
 * \brief ???
 */

#include "mpidimpl.h"

/* works for simple data types, assumes fast bcast is available */
int MPIDO_Scatter_bcast(void * sendbuf,
			int sendcount,
			MPI_Datatype sendtype,
			void *recvbuf,
			int recvcount,
			MPI_Datatype recvtype,
			int root,
			MPID_Comm * comm_ptr)
{
  /* Pretty simple - bcast a temp buffer and copy our little chunk out */
  int contig, nbytes, rc;
  int rank = comm_ptr->rank;
  int size = comm_ptr->local_size;
  char *tempbuf = NULL;

  MPID_Datatype * dt_ptr;
  MPI_Aint true_lb = 0; 
  
  if(rank == root)
  {
    MPIDI_Datatype_get_info(sendcount,
                            sendtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);
    tempbuf = sendbuf;
  }
  else
  {
    MPIDI_Datatype_get_info(recvcount,
                            recvtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);

    tempbuf = MPIU_Malloc(nbytes * size);
    if(!tempbuf)
    {
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  "MPI_Scatter",
                                  __LINE__, MPI_ERR_OTHER, "**nomem", 0);
    }
  }
  
  rc = MPIDO_Bcast(tempbuf, nbytes*size, MPI_CHAR, root, comm_ptr);
  
  if(rank == root && recvbuf == MPI_IN_PLACE)
    return rc;
  else
    memcpy(recvbuf, tempbuf+(rank*nbytes), nbytes);
  
  if (rank!=root)
    MPIU_Free(tempbuf);

  return rc;
}


int MPIDO_Scatter(void *sendbuf,
                  int sendcount,
                  MPI_Datatype sendtype,
                  void *recvbuf,
                  int recvcount,
                  MPI_Datatype recvtype,
                  int root,
                  MPID_Comm * comm_ptr)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;
  char *sbuf = sendbuf, *rbuf = recvbuf;
  int contig, nbytes = 0;
  int rank = comm_ptr->rank;
  int success = 1;

  if (rank == root)
  {
    if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
    {
      MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                              nbytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
    else
      success = 0;

    if (success)
    {
      if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
      {
        MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                                nbytes, data_ptr, true_lb);
        if (!contig) success = 0;
      }
      else success = 0;
    }
  }

  else
  {
    if (sendtype != MPI_DATATYPE_NULL && sendcount >= 0)
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                              nbytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
    else
      success = 0;
  }

  MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_MPICH"); 
  if(!comm_ptr->mpid.optscatter)
  {
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr);
  }

  /* see if we all agree to use bcast scatter */
  MPIDO_Allreduce(MPI_IN_PLACE, &success, 1, MPI_INT, MPI_BAND, comm_ptr);

  if (!success)
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr);

   sbuf = sendbuf+true_lb;
   rbuf = recvbuf+true_lb;
  
   MPIDI_Update_last_algorithm(comm_ptr, "SCATTER_OPT_BCAST");
   return MPIDO_Scatter_bcast(sbuf, sendcount, sendtype,
                                 rbuf, recvcount, recvtype,
                                 root, comm_ptr);
}

