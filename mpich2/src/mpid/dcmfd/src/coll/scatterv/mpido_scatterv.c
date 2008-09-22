/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/scatterv/mpido_scatterv.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"


#ifdef USE_CCMI_COLL

#pragma weak PMPIDO_Scatterv = MPIDO_Scatterv

int MPIDO_Scatterv(void *sendbuf,
                   int *sendcounts,
                   int *displs,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   int root,
                   MPID_Comm *comm_ptr)
{
  DCMF_Embedded_Info_Set * properties = &(comm_ptr->dcmf.properties);
  int rank = comm_ptr->rank, size = comm_ptr->local_size;
  int i, nbytes, sum=0, contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint true_lb=0;

  if (DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_SCATTERV) ||
      comm_ptr->comm_kind != MPID_INTRACOMM)
  {
    return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
                         recvbuf, recvcount, recvtype, 
                         root, comm_ptr);
  }

    
  /* we can't call scatterv-via-bcast unless we know all nodes have
   * valid sendcount arrays. so the user must explicitly ask for it.
   */

   /* optscatterv[0] == optscatterv bcast? 
    * optscatterv[1] == optscatterv alltoall? 
    *  (having both allows cutoff agreement)
    * optscatterv[2] == sum of sendcounts 
    */
   int optscatterv[3];

   optscatterv[0] = !DCMF_INFO_ISSET(properties, DCMF_USE_ALLTOALL_SCATTERV);
   optscatterv[1] = !DCMF_INFO_ISSET(properties, DCMF_USE_BCAST_SCATTERV);
   optscatterv[2] = 1;

   if(rank == root)
   {
      if(!optscatterv[1])
      {
         if(sendcounts)
         {
            for(i=0;i<size;i++)
               sum+=sendcounts[i];
         }
         optscatterv[2] = sum;
      }
  
      MPIDI_Datatype_get_info(1,
                            sendtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);
      if(recvtype == MPI_DATATYPE_NULL || recvcount <= 0 || !contig)
      {
         optscatterv[0] = 1;
         optscatterv[1] = 1;
      }
  }
  else
  {
      MPIDI_Datatype_get_info(1,
                            recvtype,
                            contig,
                            nbytes,
                            dt_ptr,
                            true_lb);
      if(sendtype == MPI_DATATYPE_NULL || !contig)
      {
         optscatterv[0] = 1;
         optscatterv[1] = 1;
      }
   }

  /* Make sure parameters are the same on all the nodes */
  /* specifically, noncontig on the receive */
  /* set the internal control flow to disable internal star tuning */
  STAR_info.internal_control_flow = 1;

   if(DCMF_INFO_ISSET(properties, DCMF_USE_PREALLREDUCE_SCATTERV))
   {
      MPIDO_Allreduce(MPI_IN_PLACE,
		  optscatterv,
		  3,
		  MPI_INT,
		  MPI_BOR,
		  comm_ptr);
   }
  /* reset flag */
  STAR_info.internal_control_flow = 0;  

   if(!optscatterv[0] || (!optscatterv[1]))
   {
      char *newrecvbuf = recvbuf;
      char *newsendbuf = sendbuf;
      if(rank == root)
      {
         MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT
                                          sendbuf + true_lb);
         newsendbuf = (char *) sendbuf + true_lb;
      }
      else
      {
         if(recvbuf != MPI_IN_PLACE)
         {
            MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT
                                             recvbuf + true_lb);
            newrecvbuf = (char *) recvbuf + true_lb;
         }
      }
      if(!optscatterv[0])
      {
         return MPIDO_Scatterv_alltoallv(newsendbuf,
                                         sendcounts,
                                         displs,
                                         sendtype,
                                         newrecvbuf,
                                         recvcount,
                                         recvtype,
                                         root,
                                         comm_ptr);
     
      }
      else
      {
         return MPIDO_Scatterv_bcast(newsendbuf,
                                     sendcounts,
                                     displs,
                                     sendtype,
                                     newrecvbuf,
                                     recvcount,
                                     recvtype,
                                     root,
                                     comm_ptr);
      }
   } /* nothing valid to try, go to mpich */
   else
   {
      return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
                           recvbuf, recvcount, recvtype,
                           root, comm_ptr);
   }
}

#else /* !USE_CCMI_COLL */

int MPIDO_Scatterv(void *sendbuf,
                   int *sendcounts, 
                   int *displs,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   int root,
                   MPID_Comm *comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
