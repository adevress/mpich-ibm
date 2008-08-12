/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid/dcmfd/src/coll/bcast/mpido_bcast.c
 * \brief ???
 */

#include "mpido_coll.h"
#include "mpidi_coll_prototypes.h"
#include "mpix.h"

#pragma weak PMPIDO_Bcast = MPIDO_Bcast

#ifdef USE_CCMI_COLL

int
MPIDO_Bcast(void *buffer,
            int count, MPI_Datatype datatype, int root, MPID_Comm * comm)
{
  bcast_fptr func = NULL;

  DCMF_Embedded_Info_Set *properties = &(comm->dcmf.properties);

  int data_size, data_contig, rc = MPI_ERR_INTERN;
  char *data_buffer = NULL, *noncontig_buff = NULL;
  MPI_Aint data_true_lb = 0;
  MPID_Datatype *data_ptr;
  MPID_Segment segment;
  int userenvset = DCMF_INFO_ISSET(properties, DCMF_BCAST_ENVVAR);
  int rank = comm->rank;

  if (!count)
    return MPI_SUCCESS;

  if (DCMF_INFO_ISSET(properties, DCMF_USE_MPICH_BCAST))
    return MPIR_Bcast(buffer, count, datatype, root, comm);

  MPIDI_Datatype_get_info(count,
                          datatype,
                          data_contig, data_size, data_ptr, data_true_lb);

  MPID_Ensure_Aint_fits_in_pointer(MPIR_VOID_PTR_CAST_TO_MPI_AINT buffer +
                                   data_true_lb);

  data_buffer = (char *) buffer + data_true_lb;

  if (!data_contig)
    {
      noncontig_buff = MPIU_Malloc(data_size);
      data_buffer = noncontig_buff;
      if (noncontig_buff == NULL)
        {
          fprintf(stderr,
                  "Pack: Tree Bcast cannot allocate local non-contig pack"
                  " buffer\n");
          MPIX_Dump_stacks();
          MPID_Abort(NULL, MPI_ERR_NO_SPACE, 1,
                     "Fatal:  Cannot allocate pack buffer");
        }

      if (comm->rank == root)
        {
          DLOOP_Offset last = data_size;
          MPID_Segment_init(buffer, count, datatype, &segment, 0);
          MPID_Segment_pack(&segment, 0, &last, noncontig_buff);
        }
    }

  /*
   *      The following cut offs were based on benchmark data
   *
   *      A. if (tree is on && 0 < msize < 512KB)
   *         - Tree
   *         - This condition can fail if user does not want tree or comm is not
   *         comm_world or msize >= 512KB
   *
   *      B. if A fails, then we attempt
   *         - Arect if msize <= 8KB
   *         - Rect if msize > 8KB
   *         - This condition can fail if user does not want rect protocols or msize
   *         is not in range.
   *
   *      C. If B fails, we attempt
   *         - Abinom if msize <= 16KB
   *         - Binom if 16KB < msize <= 65KB
   *         - This condition can fail if user does not want binom protocols or msize
   *         is not in range.
   *
   *      D. If C fails, attempt
   *         - Scatter-Gather
   *         - This scenario happens when the communicator is irregular and binom
   *         protocols cannot handle the message size range.
   */


   if(DCMF_INFO_ISSET(properties, DCMF_USE_TREE_BCAST) && 
     (userenvset ||
      (data_size <=65536 || (unsigned)data_buffer & 0x0F)))
   {
//      if(!rank)
//      fprintf(stderr,"size: %d, aligned: %d calling tree tree: %d rect: %d dput: %d\n", data_size, (unsigned)data_buffer & 0x0F, 
//      DCMF_INFO_ISSET(properties, DCMF_USE_TREE_BCAST),
//      DCMF_INFO_ISSET(properties, DCMF_USE_RECT_BCAST), DCMF_INFO_ISSET(properties, DCMF_USE_RECT_DPUT_BCAST));
      func = MPIDO_Bcast_tree;
   }

   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_RECT_BCAST) &&
      (userenvset || data_size <= 4096))
   {
//      if(!rank)
//      fprintf(stderr,"size: %d, aligned: %d calling rect bcast tree: %d rect: %d dput: %d\n", data_size, (unsigned)data_buffer & 0x0F, 
//      DCMF_INFO_ISSET(properties, DCMF_USE_TREE_BCAST),
//      DCMF_INFO_ISSET(properties, DCMF_USE_RECT_BCAST), DCMF_INFO_ISSET(properties, DCMF_USE_RECT_DPUT_BCAST));
      func = MPIDO_Bcast_rect_sync;
   }


   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_RECT_DPUT_BCAST) &&
       !((unsigned) data_buffer & 0x0F))
   {
//      if(!rank)
//      fprintf(stderr,"size: %d, aligned: %d calling dput bcast tree: %d rect: %d dput: %d\n", data_size, (unsigned)data_buffer & 0x0F,
//      DCMF_INFO_ISSET(properties, DCMF_USE_TREE_BCAST),
//      DCMF_INFO_ISSET(properties, DCMF_USE_RECT_BCAST), DCMF_INFO_ISSET(properties, DCMF_USE_RECT_DPUT_BCAST));
      func = MPIDO_Bcast_rect_dput;
   }

//   if(!func && DCMF_INFO_ISSET(properties, DCMF_USE_ARECT_BCAST))
   if(!func && userenvset && DCMF_INFO_ISSET(properties, DCMF_USE_ARECT_BCAST))
   {
//      if (userenvset || 
//          data_size <= 4096)
//      {
         if (comm->dcmf.bcast_iter < 32)
         {
            comm->dcmf.bcast_iter++;
            func = MPIDO_Bcast_rect_async;
         }
         else
         {
            comm->dcmf.bcast_iter = 0;
         }
//      }
   }



   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_ABINOM_BCAST))
   {
      if (userenvset || 
          data_size <= 16384)
      {
         if (comm->dcmf.bcast_iter < 32)
         {
            comm->dcmf.bcast_iter++;
            func = MPIDO_Bcast_binom_async;
         }
         else
         {
            comm->dcmf.bcast_iter = 0;
         }
      }
   }
   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_BINOM_BCAST))
   {
      func = MPIDO_Bcast_binom_sync;
   }


   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_RECT_SINGLETH_BCAST))
   {
      func = MPIDO_Bcast_rect_singleth;
   }

   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_BINOM_SINGLETH_BCAST))
   {
      func = MPIDO_Bcast_binom_singleth;
   }

   if (!func && DCMF_INFO_ISSET(properties, DCMF_USE_SCATTER_GATHER_BCAST))
   {
      func = MPIDO_Bcast_scatter_gather;
   }

   if(!func)
   {
//      if(!rank)
//      fprintf(stderr,"calling mpich\n");
      return MPIR_Bcast(buffer, count, datatype, root, comm);
   }

   rc = (func) (data_buffer, data_size, root, comm);

  if (!data_contig)
    {
      if (comm->rank != root)
        {
          int smpi_errno, rmpi_errno;
          MPIDI_msg_sz_t rcount;
          MPIDI_DCMF_Buffer_copy(noncontig_buff, data_size, MPI_CHAR,
                                 &smpi_errno, buffer, count, datatype,
                                 &rcount, &rmpi_errno);
        }
      MPIU_Free(noncontig_buff);
    }

  return rc;
}

#else /* !USE_CCMI_COLL */

int
MPIDO_Bcast(void *buffer,
            int count, MPI_Datatype datatype, int root, MPID_Comm * comm_ptr)
{
  MPID_abort();
}
#endif /* !USE_CCMI_COLL */
