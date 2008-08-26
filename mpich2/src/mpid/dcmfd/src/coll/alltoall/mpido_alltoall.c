/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/coll/alltoall/mpido_alltoall.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_star.h"
#include "mpidi_coll_prototypes.h"

#pragma weak PMPIDO_Alltoall = MPIDO_Alltoall

int
MPIDO_Alltoall(void *sendbuf,
               int sendcount,
               MPI_Datatype sendtype,
               void *recvbuf,
               int recvcount,
               MPI_Datatype recvtype,
               MPID_Comm * comm)
{
  int i, numprocs = comm->local_size;
  int tsndlen, trcvlen, snd_contig, rcv_contig, rc;
  DCMF_Embedded_Info_Set * properties = &(comm -> dcmf.properties);

  MPI_Aint sdt_true_lb, rdt_true_lb;
  MPID_Datatype *dt_null = NULL;

  if (sendcount == 0 || recvcount == 0)
    return MPI_SUCCESS;

  MPIDI_Datatype_get_info(sendcount, sendtype, snd_contig,
			  tsndlen, dt_null, sdt_true_lb);
  MPIDI_Datatype_get_info(recvcount, recvtype, rcv_contig,
			  trcvlen, dt_null, rdt_true_lb);

  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT sendbuf +
				   sdt_true_lb);
  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT recvbuf +
				   rdt_true_lb);
  
  if (!snd_contig ||
      !rcv_contig ||
      tsndlen != trcvlen ||
      DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_ALLTOALL))
    return MPIR_Alltoall(sendbuf, sendcount, sendtype,
			 recvbuf, recvcount, recvtype,
			 comm);

  if (!DCMF_AllocateAlltoallBuffers(comm))
    return MPIR_Err_create_code(MPI_SUCCESS,
				MPIR_ERR_RECOVERABLE,
				"MPI_Alltoall",
				__LINE__, MPI_ERR_OTHER, "**nomem", 0);
      
  /* ---------------------------------------------- */
  /* Initialize the send buffers and lengths        */
  /* pktInject is the number of packets to inject   */
  /* per advance loop.  The best performance is 2   */
  /* ---------------------------------------------- */
  for (i = 0; i < numprocs; i++)
  {
    comm->dcmf.sndlen [i] =     tsndlen;
    comm->dcmf.sdispls[i] = i * tsndlen;
    comm->dcmf.rcvlen [i] =     trcvlen;
    comm->dcmf.rdispls[i] = i * trcvlen;
  }
      

  if (!STAR_info.enabled || STAR_info.internal_control_flow)
  {

    if (!DCMF_INFO_ISSET(properties, DCMF_USE_TORUS_ALLTOALL))
      return MPIR_Alltoall(sendbuf, sendcount, sendtype,
                           recvbuf, recvcount, recvtype,
                           comm);

  rc = MPIDO_Alltoall_torus((char *) sendbuf + sdt_true_lb,
                              sendcount,
                              sendtype,
                              (char *) recvbuf + rdt_true_lb,
                              recvcount,
                              recvtype,
                              comm);

  }
  else
  {
    STAR_Callsite collective_site;
    void ** tb_ptr = (void **) MPIU_Malloc(sizeof(void *) *
                                           STAR_info.traceback_levels);

    /* get backtrace info for caller to this func, use that as callsite_id */
    backtrace(tb_ptr, STAR_info.traceback_levels);

    /* create a signature callsite info for this particular call site */
    collective_site.call_type = ALLTOALL_CALL;
    collective_site.comm = comm;
    collective_site.bytes = tsndlen;
    collective_site.op_type_support = DCMF_SUPPORT_NOT_NEEDED;
    collective_site.id = (int) tb_ptr[STAR_info.traceback_levels - 1];

    /* set the internal control flow to disable internal star tuning */
    STAR_info.internal_control_flow = 1;

    rc = STAR_Alltoall((char *) sendbuf + sdt_true_lb,
                       sendcount,
                       sendtype,
                       (char *) recvbuf + rdt_true_lb,
                       recvcount,
                       recvtype,
                       &collective_site,
                       STAR_alltoall_repository,
                       STAR_info.alltoall_algorithms);

    /* unset the internal control flow */
    STAR_info.internal_control_flow = 0;
      
    if (rc == STAR_FAILURE)
      rc = MPIR_Alltoall(sendbuf, sendcount, sendtype,
                         recvbuf, recvcount, recvtype,
                         comm);
    MPIU_Free(tb_ptr);
  }

  return rc;
}
