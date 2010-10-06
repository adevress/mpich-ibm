/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_macros.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_macros_h__
#define __include_mpidi_macros_h__


#ifdef TRACE_ON
#ifdef __GNUC__
#define TRACE_ALL(fd, format, ...) fprintf(fd, "%s:%u (%d) " format, __FILE__, __LINE__, MPIR_Process.comm_world->rank, ##__VA_ARGS__)
#define TRACE_OUT(format, ...) TRACE_ALL(stdout, format, ##__VA_ARGS__)
#define TRACE_ERR(format, ...) TRACE_ALL(stderr, format, ##__VA_ARGS__)
#else
#define TRACE_OUT(format...) fprintf(stdout, format)
#define TRACE_ERR(format...) fprintf(stderr, format)
#endif
#else
#define TRACE_OUT(format...)
#define TRACE_ERR(format...)
#endif


/**
 * \brief Gets significant info regarding the datatype
 * Used in mpid_send, mpidi_send.
 */
#define MPIDI_Datatype_get_info(_count, _datatype,              \
_dt_contig_out, _data_sz_out, _dt_ptr, _dt_true_lb)             \
({                                                              \
  if (HANDLE_GET_KIND(_datatype) == HANDLE_KIND_BUILTIN)        \
    {                                                           \
        (_dt_ptr)        = NULL;                                \
        (_dt_contig_out) = TRUE;                                \
        (_dt_true_lb)    = 0;                                   \
        (_data_sz_out)   = (_count) *                           \
        MPID_Datatype_get_basic_size(_datatype);                \
    }                                                           \
  else                                                          \
    {                                                           \
        MPID_Datatype_get_ptr((_datatype), (_dt_ptr));          \
        (_dt_contig_out) = (_dt_ptr)->is_contig;                \
        (_dt_true_lb)    = (_dt_ptr)->true_lb;                  \
        (_data_sz_out)   = (_count) * (_dt_ptr)->size;          \
    }                                                           \
})

/* Add some error checking for size eventually */
#define MPIDI_Update_last_algorithm(_comm, _name) \
{ strncpy( (_comm)->mpid.last_algorithm[0], (_name), strlen((_name))+1); }


/**
 * \brief Macro for allocating memory
 *
 * \param[in] count Number of elements to allocate
 * \param[in] type  The type of the memory, excluding "*"
 * \return Address or NULL
 */
#define MPIU_Calloc0(count, type)               \
({                                              \
  size_t __size = (count) * sizeof(type);       \
  type* __p = MPIU_Malloc(__size);              \
  MPID_assert(__p != NULL);                     \
  if (__p != NULL)                              \
    memset(__p, 0, __size);                     \
  __p;                                          \
})

#define MPIU_TestFree(p)                        \
({                                              \
  if (*(p) != NULL)                             \
    {                                           \
      MPIU_Free(*(p));                          \
      *(p) = NULL;                              \
    }                                           \
})


#define MPID_VCR_GET_LPID(vcr, index)           \
({                                              \
  vcr[index];                                   \
})
#define MPID_GPID_Get(comm_ptr, rank, gpid)             \
{                                                       \
  gpid[0] = 0;                                          \
  gpid[1] = MPID_VCR_GET_LPID(comm_ptr->vcr, rank);     \
}


#endif
