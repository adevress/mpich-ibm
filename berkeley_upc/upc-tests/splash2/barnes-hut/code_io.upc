/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/*
 * CODE_IO.C: 
 */


    
#include "code.h"
#include <upc.h>

void in_int (), in_real (), in_vector ();
void out_int (), out_real (), out_vector ();
void diagnostics (unsigned int ProcessId);
    
/*
 * INPUTDATA: read initial conditions from input file.
 */
    
void inputdata ()
{
   stream instr;
   permanent char headbuf[128];
   int ndim,counter=0;
   real tnow;
   bodyptr p;
   int i;
    
   fprintf(stderr,"reading input file : %s\n",infile);
   fflush(stderr);
   instr = fopen(infile, "r");
   if (instr == NULL)
     error("inputdata: cannot find file %s\n", infile);
   sprintf(headbuf, "Hack code: input file %s\n", infile);
   headline = headbuf;
   in_int(instr, &nbody);
   if (nbody < 1)
      error("inputdata: nbody = %d is absurd\n", nbody);
   in_int(instr, &ndim);
   if (ndim != NDIM)
      error("inputdata: NDIM = %d ndim = %d is absurd\n", NDIM,ndim);
   in_real(instr, &tnow);
   for (i = 0; i < MAX_PROC; i++) {
      Local.tnow = tnow;
   }
   bodytab = (bodyptr) upc_global_alloc(THREADS+1, (nbody/THREADS) * sizeof(body));
   if (bodytab == NULL)
      error("inputdata: not enuf memory\n");
   for (p = bodytab; p < bodytab+nbody; p++) {
      Type(p) = BODY;
      Cost(p) = 1;
      Phi(p) = 0.0;
      CLRV(Acc(p));
   } 
   for (p = bodytab; p < bodytab+nbody; p++)
      in_real(instr, &Mass(p));
   for (p = bodytab; p < bodytab+nbody; p++)
      in_vector(instr, Pos(p));
   for (p = bodytab; p < bodytab+nbody; p++)
      in_vector(instr, Vel(p));
   fclose(instr);
}

/*
 * INITOUTPUT: initialize output routines.
 */


void initoutput()
{
   printf("\n\t\t%s\n\n", headline);
   printf("%10s%10s%10s%10s%10s%10s%10s%10s\n",
	  "nbody", "dtime", "eps", "tol", "dtout", "tstop","fcells","NPROC");
   printf("%10d%10.5f%10.4f%10.2f%10.3f%10.3f%10.2f%10d\n\n",
	  nbody, dtime, eps, tol, dtout, tstop, fcells, NPROC);
}

/*
 * STOPOUTPUT: finish up after a run.
 */

/*
 * OUTPUT: compute diagnostics and output data.
 */

void
output (ProcessId)
   unsigned int ProcessId;
{
   int nttot, nbavg, ncavg,k;
   double cputime();
   bodyptr p, *pp;
   vector tempv1,tempv2;

   if ((Local.tout - 0.01 * dtime) <= Local.tnow) {
      Local.tout += dtout;
   }

   diagnostics(ProcessId);

   if (Local.mymtot!=0) {
      upc_lock(CountLock);
      n2bcalc += Local.myn2bcalc;
      nbccalc += Local.mynbccalc;
      selfint += Local.myselfint;
      ADDM(keten,  keten, Local.myketen);
      ADDM(peten,  peten, Local.mypeten);
      for (k=0;k<3;k++) etot[k] +=  Local.myetot[k];
      ADDV(amvec,  amvec, Local.myamvec);
        
      MULVS(tempv1, cmphase[0],mtot);
      MULVS(tempv2, Local.mycmphase[0], Local.mymtot);
      ADDV(tempv1, tempv1, tempv2);
      DIVVS(cmphase[0], tempv1, mtot+Local.mymtot); 
        
      MULVS(tempv1, cmphase[1],mtot);
      MULVS(tempv2, Local.mycmphase[1], Local.mymtot);
      ADDV(tempv1, tempv1, tempv2);
      DIVVS(cmphase[1], tempv1, mtot+Local.mymtot);
      mtot +=Local.mymtot;
      upc_unlock(CountLock);
   }
    
   upc_barrier;
    
   if (ProcessId==0) {
      nttot = n2bcalc + nbccalc;
      nbavg = (int) ((real) n2bcalc / (real) nbody);
      ncavg = (int) ((real) nbccalc / (real) nbody);
   }
}

/*
 * DIAGNOSTICS: compute set of dynamical diagnostics.
 */

void
diagnostics (ProcessId)
   unsigned int ProcessId;
{
   bodyptr p;
   register bodyptr *pp;
   real velsq;
   vector tmpv;
   matrix tmpt;

   Local.mymtot = 0.0;
   Local.myetot[1] = Local.myetot[2] = 0.0;
   CLRM(Local.myketen);
   CLRM(Local.mypeten);
   CLRV(Local.mycmphase[0]);
   CLRV(Local.mycmphase[1]);
   CLRV(Local.myamvec);
   for (pp = Local.mybodytab+Local.mynbody -1; 
	pp >= Local.mybodytab; pp--) { 
      p= *pp;
      Local.mymtot += Mass(p);
      DOTVP(velsq, Vel(p), Vel(p));
      Local.myetot[1] += 0.5 * Mass(p) * velsq;
      Local.myetot[2] += 0.5 * Mass(p) * Phi(p);
      MULVS(tmpv, Vel(p), 0.5 * Mass(p));
      OUTVP(tmpt, tmpv, Vel(p));
      ADDM(Local.myketen, Local.myketen, tmpt);
      MULVS(tmpv, Pos(p), Mass(p));
      OUTVP(tmpt, tmpv, Acc(p));
      ADDM(Local.mypeten, Local.mypeten, tmpt);
      MULVS(tmpv, Pos(p), Mass(p));
      ADDV(Local.mycmphase[0], Local.mycmphase[0], tmpv);
      MULVS(tmpv, Vel(p), Mass(p));
      ADDV(Local.mycmphase[1], Local.mycmphase[1], tmpv);
      CROSSVP(tmpv, Pos(p), Vel(p));
      MULVS(tmpv, tmpv, Mass(p));
      ADDV(Local.myamvec, Local.myamvec, tmpv);
   }
   Local.myetot[0] = Local.myetot[1] 
      + Local.myetot[2];
   if (Local.mymtot!=0){
      DIVVS(Local.mycmphase[0], Local.mycmphase[0], 
	    Local.mymtot);
      DIVVS(Local.mycmphase[1], Local.mycmphase[1], 
	    Local.mymtot);
   }
}


/*
 * Low-level input and output operations.
 */

void in_int(str, iptr)
  stream str;
  int *iptr;
{
   if (fscanf(str, "%d", iptr) != 1)
      error("in_int: input conversion error\n");
}

void in_real(str, rptr)
  stream str;
  real *rptr;
{
   double tmp;

   if (fscanf(str, "%lf", &tmp) != 1)
      error("in_real: input conversion error\n");
   *rptr = tmp;
}

void in_vector(str, vec)
  stream str;
  vector vec;
{
   double tmpx, tmpy, tmpz;

   if (fscanf(str, "%lf%lf%lf", &tmpx, &tmpy, &tmpz) != 3)
      error("in_vector: input conversion error\n");
   vec[0] = tmpx;    vec[1] = tmpy;    vec[2] = tmpz;
}

void out_int(str, ival)
  stream str;
  int ival;
{
   fprintf(str, "  %d\n", ival);
}

void out_real(str, rval)
  stream str;
  real rval;
{
   fprintf(str, " %21.14E\n", rval);
}

void out_vector(str, vec)
  stream str;
  vector vec;
{
   fprintf(str, " %21.14E %21.14E", vec[0], vec[1]);
   fprintf(str, " %21.14E\n",vec[2]);
}
