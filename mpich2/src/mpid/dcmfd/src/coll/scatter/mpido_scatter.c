/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/scatter/mpido_scatter.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Scatter = MPIDO_Scatter

/* works for simple data types, assumes fast bcast is available */

int MPIDO_Scatter(void *sendbuf,
                  int sendcount,
                  MPI_Datatype sendtype,
                  void *recvbuf,
                  int recvcount,
                  MPI_Datatype recvtype,
                  int root,
                  MPID_Comm * comm)
{
  DCMF_Embedded_Info_Set * properties = &(comm->dcmf.properties);
  MPID_Datatype * data_ptr;
  MPI_Aint true_lb = 0;

  int contig, nbytes;
  int rank = comm->rank;
  int success = 1, rc;

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

  MPIDO_Allreduce(MPI_IN_PLACE, &success, 1, MPI_INT, MPI_BAND, comm);

  if (!success || DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_SCATTER) ||
      !DCMF_INFO_ISSET(properties, DCMF_USE_BCAST_SCATTER))
    return MPIR_Scatter(sendbuf, sendcount, sendtype,
			recvbuf, recvcount, recvtype,
			root, comm);

  MPID_Ensure_Aint_fits_in_pointer (MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				    true_lb);
  sendbuf = (char *) sendbuf + true_lb;

  if (recvbuf != MPI_IN_PLACE)
    {
      MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				       true_lb);
      recvbuf = (char *) recvbuf + true_lb;
    }

    return MPIDO_Scatter_bcast(sendbuf, sendcount, sendtype,
			       recvbuf, recvcount, recvtype,
			       root, comm);
}
