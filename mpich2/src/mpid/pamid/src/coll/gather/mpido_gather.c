/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/gather/mpido_gather.c
 * \brief ???
 */

#include <mpidimpl.h>

int MPIDO_Gather_reduce(void * sendbuf,
			int sendcount,
			MPI_Datatype sendtype,
			void *recvbuf,
			int recvcount,
			MPI_Datatype recvtype,
			int root,
			MPID_Comm * comm_ptr)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb;
  int rank = comm_ptr->rank;
  int size = comm_ptr->local_size;
  int rc, sbytes, rbytes, contig;
  char *tempbuf = NULL;
  char *inplacetemp = NULL;

  MPIDI_Datatype_get_info(sendcount, sendtype, contig,
			  sbytes, data_ptr, true_lb);

  MPIDI_Datatype_get_info(recvcount, recvtype, contig,
			  rbytes, data_ptr, true_lb);

  if(rank == root)
  {
    tempbuf = recvbuf;
    /* zero the buffer if we aren't in place */
    if(sendbuf != MPI_IN_PLACE)
    {
      memset(tempbuf, 0, sbytes * size * sizeof(char));
      memcpy(tempbuf+(rank * sbytes), sendbuf, sbytes);
    }
    /* copy our contribution temporarily, zero the buffer, put it back */
    /* this will likely suck */
    else
    {
      inplacetemp = MPIU_Malloc(rbytes * sizeof(char));
      memcpy(inplacetemp, recvbuf+(rank * rbytes), rbytes);
      memset(tempbuf, 0, sbytes * size * sizeof(char));
      memcpy(tempbuf+(rank * rbytes), inplacetemp, rbytes);
      MPIU_Free(inplacetemp);
    }
  }
  /* everyone might need to speciifcally allocate a tempbuf, or
   * we might need to make sure we don't end up at mpich in the
   * mpido_() functions - seems to be a problem?
   */

  /* If we aren't root, malloc tempbuf and zero it,
   * then copy our contribution to the right spot in the big buffer */
  else
  {
    tempbuf = MPIU_Malloc(sbytes * size * sizeof(char));
    if(!tempbuf)
      return MPIR_Err_create_code(MPI_SUCCESS,
                                  MPIR_ERR_RECOVERABLE,
                                  __FUNCTION__,
                                  __LINE__,
                                  MPI_ERR_OTHER,
                                  "**nomem", 0);

    memset(tempbuf, 0, sbytes * size * sizeof(char));
    memcpy(tempbuf+(rank*sbytes), sendbuf, sbytes);
  }
  /* #warning need an optimal reduce */
  rc = MPIR_Reduce(MPI_IN_PLACE,
                    tempbuf,
                    (sbytes * size)/4,
                    MPI_INT,
                    MPI_BOR,
                    root,
                    comm_ptr);

  if(rank != root)
    MPIU_Free(tempbuf);

  return rc;
}

/* works for simple data types, assumes fast reduce is available */

int MPIDO_Gather(void *sendbuf,
                 int sendcount,
                 MPI_Datatype sendtype,
                 void *recvbuf,
                 int recvcount,
                 MPI_Datatype recvtype,
                 int root,
                 MPID_Comm *comm_ptr)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;
  char *sbuf = sendbuf, *rbuf = recvbuf;
  int success = 1, contig, send_bytes=-1, recv_bytes = 0;
  int rank = comm_ptr->rank;

  if (sendtype != MPI_DATATYPE_NULL && sendcount >= 0)
  {
    MPIDI_Datatype_get_info(sendcount, sendtype, contig,
                            send_bytes, data_ptr, true_lb);
    if (!contig || ((send_bytes * comm_ptr->local_size) % sizeof(int)))
      success = 0;
  }
  else
    success = 0;

  if (success && rank == root)
  {
    if (recvtype != MPI_DATATYPE_NULL && recvcount >= 0)
    {
      MPIDI_Datatype_get_info(recvcount, recvtype, contig,
                              recv_bytes, data_ptr, true_lb);
      if (!contig) success = 0;
    }
    else
      success = 0;
  }

  MPIDI_Update_last_algorithm(comm_ptr, "GATHER_MPICH");
  if(!comm_ptr->mpid.optgather ||
   comm_ptr->mpid.user_selectedvar[PAMI_XFER_GATHER] == 0)
  {
    return MPIR_Gather(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr);
  }


  MPIDO_Allreduce(MPI_IN_PLACE, &success, 1, MPI_INT, MPI_BAND, comm_ptr);

  if (!success)
    return MPIR_Gather(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr);

  sbuf = sendbuf + true_lb;
  rbuf = recvbuf + true_lb;

  MPIDI_Update_last_algorithm(comm_ptr, "GATHER_OPT_REDUCE");
   return MPIDO_Gather_reduce(sbuf, sendcount, sendtype,
                              rbuf, recvcount, recvtype,
                              root, comm_ptr);
}
