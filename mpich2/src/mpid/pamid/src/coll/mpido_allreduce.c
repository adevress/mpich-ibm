#include "mpidimpl.h"

#ifdef TRACE_ERR
#undef TRACE_ERR
#endif
#define TRACE_ERR(x) //fprintf x

int MPItoPAMI(MPI_Datatype dt, pami_dt *pdt, MPI_Op op, pami_op *pop, int *musupport);
void MPIopString(MPI_Op op, char *string);

static void cb_allreduce(void *ctxt, void *clientdata, pami_result_t err)
{
   int *active = (int *) clientdata;
   TRACE_ERR((stderr,"callback enter, active: %d\n", (*active)));
   (*active)--;
}

/* some useful macros to make the comparisons less icky, esp given the */
/* explosion of datatypes in MPI2.2                                    */
#define isS_INT(x) ( (x)==MPI_INTEGER || (x) == MPI_BYTE || \
                     (x) == MPI_INT32_T || (x) == MPI_INTEGER4 || \
                     (x) == MPI_INT)

#define isUS_INT(x) ( (x) == MPI_UINT32_T || (x) == MPI_UNSIGNED)

#define isS_SHORT(x) ( (x) == MPI_SHORT || (x) == MPI_INT16_T || \
                       (x) == MPI_INTEGER2)

#define isUS_SHORT(x) ( (x) == MPI_UNSIGNED_SHORT || (x) == MPI_UINT16_T)

#define isS_CHAR(x) ( (x) == MPI_SIGNED_CHAR || (x) == MPI_INT8_T || \
                      (x) == MPI_INTEGER1)

#define isUS_CHAR(x) ( (x) == MPI_UNSIGNED_CHAR || (x) == MPI_UINT8_T)

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

#define YES 1
#define NO 0

void MPIopString(MPI_Op op, char *string)
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


int MPItoPAMI(MPI_Datatype dt, pami_dt *pdt, MPI_Op op, pami_op *pop, int *musupport)
{
   *musupport = YES;
   *pdt = PAMI_UNDEFINED_DT;
   *pop = PAMI_UNDEFINED_OP;
   if(isS_INT(dt))
   {
      *pdt = PAMI_SIGNED_INT;
      /* For some reason, signed int+B* ops doesn't work */
      if(op == MPI_BOR || op == MPI_BAND || op == MPI_BXOR)
         return -1;
   }
   else if(isUS_INT(dt)) *pdt = PAMI_UNSIGNED_INT;
   else if(isFLOAT(dt)) *pdt = PAMI_FLOAT;
   else if(isDOUBLE(dt)) *pdt = PAMI_DOUBLE;
   else
   {
      *musupport = NO;
      if(isS_CHAR(dt)) *pdt = PAMI_SIGNED_CHAR;
      else if(isUS_CHAR(dt)) *pdt = PAMI_UNSIGNED_CHAR;
      else if(isS_SHORT(dt)) *pdt = PAMI_SIGNED_SHORT;
      else if(isUS_SHORT(dt)) *pdt = PAMI_UNSIGNED_SHORT;
      else if(isS_LONG(dt)) *pdt = PAMI_SIGNED_LONG_LONG;
      else if(isUS_LONG(dt)) *pdt = PAMI_UNSIGNED_LONG_LONG;
      else if(isSINGLE_COMPLEX(dt)) *pdt = PAMI_SINGLE_COMPLEX;
      else if(isDOUBLE_COMPLEX(dt)) *pdt = PAMI_DOUBLE_COMPLEX;
      else if(isLOC_TYPE(dt))
      {
         switch(dt)
         {
            case MPI_2REAL: *pdt = PAMI_LOC_2FLOAT; break;
            case MPI_2DOUBLE_PRECISION: *pdt = PAMI_LOC_2DOUBLE; break;
            case MPI_2INTEGER:
            case MPI_2INT: *pdt = PAMI_LOC_2INT; break;
            case MPI_FLOAT_INT: *pdt = PAMI_LOC_FLOAT_INT; break;
            case MPI_DOUBLE_INT: *pdt = PAMI_LOC_DOUBLE_INT; break;
            case MPI_SHORT_INT: *pdt = PAMI_LOC_SHORT_INT; break;
         }
         if(op == MPI_MINLOC) *pop = PAMI_MINLOC;
         if(op == MPI_MAXLOC) *pop = PAMI_MAXLOC;
         return MPI_SUCCESS;
      }
      else if(isLOGICAL(dt))
      {
         *pdt = PAMI_LOGICAL;
         if(op == MPI_LOR) *pop = PAMI_LOR;
         if(op == MPI_LAND) *pop = PAMI_LAND;
         if(op == MPI_LXOR) *pop = PAMI_LXOR;
         return MPI_SUCCESS;
      }
   }

   if(*pdt == PAMI_UNDEFINED_DT) return -1;

   *pop = PAMI_UNDEFINED_OP;
   switch(op)
   {
      case MPI_SUM: *pop = PAMI_SUM; return MPI_SUCCESS; break;
      case MPI_PROD: *pop = PAMI_PROD; return MPI_SUCCESS; break;
      case MPI_MAX: *pop = PAMI_MAX; return MPI_SUCCESS; break;
      case MPI_MIN: *pop = PAMI_MIN; return MPI_SUCCESS; break;
      case MPI_BAND: *pop = PAMI_BAND; return MPI_SUCCESS; break;
      case MPI_BOR: *pop = PAMI_BOR; return MPI_SUCCESS; break;
      case MPI_BXOR: *pop = PAMI_BXOR; return MPI_SUCCESS; break;
   }
   if(*pop == PAMI_UNDEFINED_OP) return -1;

   return MPI_SUCCESS;
}





int MPIDO_Allreduce(void *sendbuf,
                    void *recvbuf,
                    int count,
                    MPI_Datatype dt,
                    MPI_Op op,
                    MPID_Comm *comm_ptr)
{
   TRACE_ERR((stderr,"in mpido_allreduce\n"));
   pami_dt pdt;
   pami_op pop;
   int mu;
   int rc;
   int len;
   char op_str[255];
   char dt_str[255];
   volatile unsigned active = 1;
   pami_xfer_t allred;
   MPIopString(op, op_str);
   PMPI_Type_get_name(dt, dt_str, &len);
   rc = MPItoPAMI(dt, &pdt, op, &pop, &mu);
   if(rc == MPI_SUCCESS && mu == 1)
   {
      MPI_Aint data_true_lb;
      MPID_Datatype *data_ptr;
      int data_size, data_contig;
      MPIDI_Datatype_get_info(count, dt, data_contig, data_size, data_ptr, data_true_lb);
      allred.cb_done = cb_allreduce;
      allred.cookie = (void *)&active;
      allred.algorithm = comm_ptr->mpid.allreduces[0];
      allred.cmd.xfer_allreduce.sndbuf = sendbuf;
      allred.cmd.xfer_allreduce.stype = PAMI_BYTE;
      allred.cmd.xfer_allreduce.rcvbuf = recvbuf;
      allred.cmd.xfer_allreduce.rtype = PAMI_BYTE;
      allred.cmd.xfer_allreduce.stypecount = data_size; // datasize is sizeof()*count
      allred.cmd.xfer_allreduce.rtypecount = data_size;
      allred.cmd.xfer_allreduce.dt = pdt;
      allred.cmd.xfer_allreduce.op = pop;
      TRACE_ERR((stderr,"posting allreduce, context: %d, algoname: %s, dt: %s, op: %s, count: %d\n", 0,
               comm_ptr->mpid.allreduce_metas[0].name, dt_str, op_str, count));
      rc = PAMI_Collective(MPIDI_Context[0], (pami_xfer_t *)&allred);
      TRACE_ERR((stderr,"allreduce posted, rc: %d\n", rc));

      assert(rc == PAMI_SUCCESS);
      while(active)
      {
         static int spin = 0;
         if(spin %1000 == 0)
            TRACE_ERR((stderr,"spinning: %d\n", spin));
         rc = PAMI_Context_advance(MPIDI_Context[0], 1);
         spin ++;
      }
      TRACE_ERR((stderr,"allreduce done\n"));
      return MPI_SUCCESS;
   }
   else
      return MPIR_Allreduce(sendbuf, recvbuf, count, dt, op, comm_ptr);
}
