/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/coll/coll_utils.c
 * \brief ???
 */

//#define TRACE_ON

#include <mpidimpl.h>

pami_result_t MPIDI_Pami_post_wrapper(pami_context_t context, void *cookie)
{
   TRACE_ERR("In post wrapper\n");
   MPIDI_Post_coll_t *coll = (MPIDI_Post_coll_t*)cookie;
   TRACE_ERR("About to call collecive\n");
   PAMI_Collective(context, (pami_xfer_t *)coll->coll_struct);
   TRACE_ERR("Done calling collective\n");
   return PAMI_SUCCESS;
}

/* some useful macros to make the comparisons less icky, esp given the */
/* explosion of datatypes in MPI2.2                                    */
#define isS_INT(x) ( (x)==MPI_INTEGER || \
                     (x) == MPI_INT32_T || (x) == MPI_INTEGER4 || \
                     (x) == MPI_INT)

#define isUS_INT(x) ( (x) == MPI_UINT32_T || (x) == MPI_UNSIGNED)

#define isS_SHORT(x) ( (x) == MPI_SHORT || (x) == MPI_INT16_T || \
                       (x) == MPI_INTEGER2)

#define isUS_SHORT(x) ( (x) == MPI_UNSIGNED_SHORT || (x) == MPI_UINT16_T)

#define isS_CHAR(x) ( (x) == MPI_SIGNED_CHAR || (x) == MPI_INT8_T || \
                      (x) == MPI_INTEGER1 || (x) == MPI_CHAR)

#define isUS_CHAR(x) ( (x) == MPI_BYTE || \
		       (x) == MPI_UNSIGNED_CHAR || (x) == MPI_UINT8_T )

/* sizeof(longlong) == sizeof(long) == sizeof(uint64) on bgq */
#define isS_LONG(x) ( (x) == MPI_INT64_T || (x) == MPI_LONG || \
                      (x) == MPI_INTEGER8 || (x) == MPI_LONG_LONG || \
                      (x) == MPI_LONG_LONG_INT || (x) == MPI_AINT || \
                      (x) == MPI_OFFSET)

#define isUS_LONG(x) ( (x) == MPI_UINT64_T || (x) == MPI_UNSIGNED_LONG || \
                       (x) == MPI_UNSIGNED_LONG_LONG)

#define isFLOAT(x) ( (x) == MPI_FLOAT || (x) == MPI_REAL)

#define isDOUBLE(x) ( (x) == MPI_DOUBLE || (x) == MPI_DOUBLE_PRECISION)

#define isLOC_TYPE(x) ( (x) == MPI_2REAL || (x) == MPI_2DOUBLE_PRECISION || \
                        (x) == MPI_2INTEGER || (x) == MPI_FLOAT_INT || \
                        (x) == MPI_DOUBLE_INT || (x) == MPI_LONG_INT || \
                        (x) == MPI_2INT || (x) == MPI_SHORT_INT || \
                        (x) == MPI_LONG_DOUBLE_INT )

#define isLOGICAL(x) ( (x) == MPI_C_BOOL || (x) == MPI_LOGICAL)

#define isSINGLE_COMPLEX(x) ( (x) == MPI_COMPLEX || (x) == MPI_C_FLOAT_COMPLEX)

#define isDOUBLE_COMPLEX(x) ( (x) == MPI_DOUBLE_COMPLEX || (x) == MPI_COMPLEX8 ||\
                              (x) == MPI_C_DOUBLE_COMPLEX)

/* known missing types: MPI_C_LONG_DOUBLE_COMPLEX */


#define MUSUPPORTED 1
#define MUUNSUPPORTED 0

/* for easier debug */
void MPIDI_Op_to_string(MPI_Op op, char *string)
{
   switch(op)
   {
      case MPI_SUM: strcpy(string, "MPI_SUM"); break;
      case MPI_PROD: strcpy(string, "MPI_PROD"); break;
      case MPI_LAND: strcpy(string, "MPI_LAND"); break;
      case MPI_LOR: strcpy(string, "MPI_LOR"); break;
      case MPI_LXOR: strcpy(string, "MPI_LXOR"); break;
      case MPI_BAND: strcpy(string, "MPI_BAND"); break;
      case MPI_BOR: strcpy(string, "MPI_BOR"); break;
      case MPI_BXOR: strcpy(string, "MPI_BXOR"); break;
      case MPI_MAX: strcpy(string, "MPI_MAX"); break;
      case MPI_MIN: strcpy(string, "MPI_MIN"); break;
      case MPI_MINLOC: strcpy(string, "MPI_MINLOC"); break;
      case MPI_MAXLOC: strcpy(string, "MPI_MAXLOC"); break;
      default: strcpy(string, "Other"); break;
   }
}


int MPIDI_Datatype_to_pami(MPI_Datatype        dt,
                           pami_type_t        *pdt,
                           MPI_Op              op,
                           pami_data_function *pop,
                           int                *musupport)
{
   *musupport = MUSUPPORTED;
   *pdt = PAMI_TYPE_NULL;
   if(op != -1)
      *pop = PAMI_DATA_NOOP;
   if(isS_INT(dt))
   {
      *pdt = PAMI_TYPE_SIGNED_INT;
      if(op == -1) return MPI_SUCCESS;
      /* #warning FIXME : signed int + Band/bor/bxor doesn't work at the lower level */
      /* For some reason, signed int+B* ops doesn't work */
      if(op == MPI_BOR || op == MPI_BAND || op == MPI_BXOR)
         return -1;
   }
   else if(isUS_INT(dt)) *pdt = PAMI_TYPE_UNSIGNED_INT;
   else if(isFLOAT(dt))  *pdt = PAMI_TYPE_FLOAT;
   else if(isDOUBLE(dt)) *pdt = PAMI_TYPE_DOUBLE;
   else
   {
      *musupport = MUUNSUPPORTED;
      if(isS_CHAR(dt))              *pdt = PAMI_TYPE_SIGNED_CHAR;
      else if(isUS_CHAR(dt))        *pdt = PAMI_TYPE_UNSIGNED_CHAR;
      else if(isS_SHORT(dt))        *pdt = PAMI_TYPE_SIGNED_SHORT;
      else if(isUS_SHORT(dt))       *pdt = PAMI_TYPE_UNSIGNED_SHORT;
      else if(isS_LONG(dt))         *pdt = PAMI_TYPE_SIGNED_LONG_LONG;
      else if(isUS_LONG(dt))        *pdt = PAMI_TYPE_UNSIGNED_LONG_LONG;
      else if(isSINGLE_COMPLEX(dt)) *pdt = PAMI_TYPE_SINGLE_COMPLEX;
      else if(isDOUBLE_COMPLEX(dt)) *pdt = PAMI_TYPE_DOUBLE_COMPLEX;
      else if(isLOC_TYPE(dt))
      {
         switch(dt)
         {
            case MPI_2REAL:             *pdt = PAMI_TYPE_LOC_2FLOAT;     break;
            case MPI_2DOUBLE_PRECISION: *pdt = PAMI_TYPE_LOC_2DOUBLE;    break;
            case MPI_2INTEGER:
            case MPI_2INT:              *pdt = PAMI_TYPE_LOC_2INT;       break;
            case MPI_FLOAT_INT:         *pdt = PAMI_TYPE_LOC_FLOAT_INT;  break;
            case MPI_DOUBLE_INT:        *pdt = PAMI_TYPE_LOC_DOUBLE_INT; break;
            case MPI_SHORT_INT:         *pdt = PAMI_TYPE_LOC_SHORT_INT;  break;
         }
         if(op == -1) return MPI_SUCCESS;
         if(op == MPI_MINLOC) *pop = PAMI_DATA_MINLOC;
         if(op == MPI_MAXLOC) *pop = PAMI_DATA_MAXLOC;
         return MPI_SUCCESS;
      }
      else if(isLOGICAL(dt))
      {
         *pdt = PAMI_TYPE_LOGICAL;
         if(op == -1) return MPI_SUCCESS;
         if(op == MPI_LOR) *pop = PAMI_DATA_LOR;
         if(op == MPI_LAND) *pop = PAMI_DATA_LAND;
         if(op == MPI_LXOR) *pop = PAMI_DATA_LXOR;
         return MPI_SUCCESS;
      }
   }

   if(*pdt == PAMI_TYPE_NULL) return -1;

   if(op == -1) return MPI_SUCCESS; /* just doing DT conversion */

   *pop = PAMI_DATA_NOOP;
   switch(op)
   {
      case MPI_SUM: *pop = PAMI_DATA_SUM; return MPI_SUCCESS; break;
      case MPI_PROD: *pop = PAMI_DATA_PROD; return MPI_SUCCESS; break;
      case MPI_MAX: *pop = PAMI_DATA_MAX; return MPI_SUCCESS; break;
      case MPI_MIN: *pop = PAMI_DATA_MIN; return MPI_SUCCESS; break;
      case MPI_BAND: *pop = PAMI_DATA_BAND; return MPI_SUCCESS; break;
      case MPI_BOR: *pop = PAMI_DATA_BOR; return MPI_SUCCESS; break;
      case MPI_BXOR: *pop = PAMI_DATA_BXOR; return MPI_SUCCESS; break;
   }
   if(*pop == PAMI_DATA_NOOP) return -1;

   return MPI_SUCCESS;
}
