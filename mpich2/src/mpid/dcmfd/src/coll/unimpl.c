/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/unimpl.c
 * \brief Function prototypes for the optimized collective routines
 */

#include "mpido_coll.h"

/* These are collectives we don't optimize. But if we add these as weak
 * aliases people could implement device-level versions and override 
 * these functions 
 */

#pragma weak PMPIDO_Gather = MPIDO_Gather
int MPIDO_Gather(void *sendbuf,
                 int sendcount,
                 MPI_Datatype sendtype,
                 void *recvbuf,
                 int recvcount,
                 MPI_Datatype recvtype,
                 int root,
                 MPID_Comm * comm_ptr)
{
   return MPIR_Gather(sendbuf, sendcount, sendtype,
                      recvbuf, recvcount, recvtype,
                      root, comm_ptr);
}

#pragma weak PMPIDO_Gatherv = MPIDO_Gatherv
int MPIDO_Gatherv(void *sendbuf,
                  int sendcount,
                  MPI_Datatype sendtype,
                  void *recvbuf,
                  int *recvcounts,
                  int *displs,
                  MPI_Datatype recvtype,
                  int root,
                  MPID_Comm * comm_ptr)
{
   return MPIR_Gatherv(sendbuf, sendcount, sendtype,
                       recvbuf, recvcounts, displs, recvtype,
                       root, comm_ptr);
}

#pragma weak PMPIDO_Scatter = MPIDO_Scatter
int MPIDO_Scatter(void *sendbuf,
                  int sendcount,
                  MPI_Datatype sendtype,
                  void *recvbuf,
                  int recvcount,
                  MPI_Datatype recvtype,
                  int root,
                  MPID_Comm * comm_ptr)
{
   return MPIR_Scatter(sendbuf, sendcount, sendtype,
                       recvbuf, recvcount, recvtype,
                       root, comm_ptr);
}

#pragma weak PMPIDO_Scatterv = MPIDO_Scatterv
int MPIDO_Scatterv(void *sendbuf,
                   int *sendcounts,
                   int *displs,
                   MPI_Datatype sendtype,
                   void *recvbuf,
                   int recvcount,
                   MPI_Datatype recvtype,
                   int root,
                   MPID_Comm * comm_ptr)
{
   return MPIR_Scatterv(sendbuf, sendcounts, displs, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm_ptr);
}

#pragma weak PMPIDO_Reduce_scatter = MPIDO_Reduce_scatter
int MPIDO_Reduce_scatter(void *sendbuf,
                         void *recvbuf,
                         int *recvcounts,
                         MPI_Datatype datatype,
                         MPI_Op op,
                         MPID_Comm * comm_ptr)
{
   return MPIR_Reduce_scatter(sendbuf, recvbuf, recvcounts, datatype,
                              op, comm_ptr);
}

#pragma weak PMPIDO_Scan = MPIDO_Scan
int MPIDO_Scan(void *sendbuf,
               void *recvbuf,
               int count,
               MPI_Datatype datatype,
               MPI_Op op,
               MPID_Comm * comm_ptr)
{
   return MPIR_Scan(sendbuf, recvbuf, count, datatype, op, comm_ptr);
}

#pragma weak PMPIDO_Exscan = MPIDO_Exscan
int MPIDO_Exscan(void *sendbuf,
                 void *recvbuf,
                 int count,
                 MPI_Datatype datatype,
                 MPI_Op op,
                 MPID_Comm * comm_ptr)
{
   return MPIR_Exscan(sendbuf, recvbuf, count, datatype, op, comm_ptr);
}
