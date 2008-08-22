/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/gather/gather_algorithms.c
 * \brief ???
 */

#include "mpido_coll.h"

int MPIDO_Gather_reduce(void * sendbuf,
			int sendcount,
			MPI_Datatype sendtype,
			void *recvbuf,
			int recvcount,
			MPI_Datatype recvtype,
			int root,
			MPID_Comm * comm)
{
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb;

  int rank = comm->rank;
  int size = comm->local_size;
  int rc, remain, count, buff_size, cbytes, sbytes, rbytes, contig;
  char * tempbuf = NULL;
  char * inplacetemp = NULL;

  MPIDI_Datatype_get_info(sendcount, sendtype, contig,
			  sbytes, data_ptr, true_lb);

  MPIDI_Datatype_get_info(recvcount, recvtype, contig,
			  rbytes, data_ptr, true_lb);

  count = sendcount;
  remain = sendcount % sizeof(int);

  if (remain)
  {
    if (sendcount <= sizeof(int))
      count = remain * sizeof(int);
    else
      count = remain * sizeof(int) + sizeof(int);
  }

  if (count != sendcount)
  {
    MPIDI_Datatype_get_info(count, sendtype, contig,
                            cbytes, data_ptr, true_lb);

    buff_size = cbytes * size * sizeof(char);

    tempbuf = MPIU_Malloc(buff_size);
    if(!tempbuf)
      return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                  "MPI_Gather", __LINE__, MPI_ERR_OTHER,
                                  "**nomem", 0);

    memset(tempbuf, 0, sbytes * size * sizeof(char));
    memcpy(tempbuf + rank * sbytes, sendbuf, sbytes);
  }

  else
  {
    buff_size = sbytes * size * sizeof(char);
    if(rank == root)
    {
      tempbuf = recvbuf;

      /* zero the buffer if we aren't in place */
      if(sendbuf != MPI_IN_PLACE)
      {
        memset(tempbuf, 0, buff_size);
        memcpy(tempbuf+(rank * sbytes), sendbuf, sbytes);
      }
      /* copy our contribution temporarily, zero the buffer, put it back */
      /* this will likely suck */
      else
      {
        inplacetemp = MPIU_Malloc(rbytes * sizeof(char));
        memcpy(inplacetemp, recvbuf+(rank * rbytes), rbytes);
        memset(tempbuf, 0, buff_size);
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
      tempbuf = MPIU_Malloc(buff_size);
      if(!tempbuf)
        return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                    "MPI_Gather", __LINE__, MPI_ERR_OTHER,
                                    "**nomem", 0);

      memset(tempbuf, 0, buff_size);
      memcpy(tempbuf+(rank*sbytes), sendbuf, sbytes);
    }
  }

#if 0
  tempbuf = MPIU_Malloc(sbytes * size * sizeof(char));
  memset(tempbuf, 0, sbytes * size * sizeof(char));
  memcpy(tempbuf+(rank * sbytes), sendbuf, sbytes);
#if 0
  if(rank == root)
  {
    tempbuf = MPIU_Malloc(sbytes * size * sizeof(char));
    //tempbuf = recvbuf;
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
      return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                  "MPI_Gather", __LINE__, MPI_ERR_OTHER,
                                  "**nomem", 0);

    memset(tempbuf, 0, sbytes * size * sizeof(char));
    memcpy(tempbuf+(rank*sbytes), sendbuf, sbytes);
  }
#endif
#endif

  rc = MPIDO_Reduce(MPI_IN_PLACE,
		    tempbuf,
		    buff_size / sizeof(int),
		    MPI_INT,
		    MPI_BOR,
		    root,
		    comm);


  if (count != sendcount)
  {
    if (rank == root)
    {
      memcpy(recvbuf, tempbuf, sbytes * size);
      MPIU_Free(tempbuf);
    }
  }

  if (rank != root)
    MPIU_Free(tempbuf);

  return rc;
}
