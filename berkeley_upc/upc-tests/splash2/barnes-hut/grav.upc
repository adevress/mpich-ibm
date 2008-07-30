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
 * GRAV.C: 
 */


#include "code.h"

/*
 * HACKGRAV: evaluate grav field at a given particle.
 */
  


void hackgrav(p,ProcessId)
  bodyptr p;
  unsigned ProcessId;

{
   extern void gravsub();
   vector tmpv;
   Local.pskip = p;
   COPYV(tmpv, Pos(p));
   COPYV(Local.pos0, tmpv);
   Local.phi0 = 0.0;
   CLRV(Local.acc0);
   Local.myn2bterm = 0;
   Local.mynbcterm = 0;
   Local.skipself = FALSE;
   hackwalk(gravsub, ProcessId);
   Phi(p) = Local.phi0;
   SETV(Acc(p), Local.acc0);
#ifdef QUADPOLE
   Cost(p) = Local.myn2bterm + NDIM * Local.mynbcterm;
#else
   Cost(p) = Local.myn2bterm + Local.mynbcterm;
#endif
}



/*
 * GRAVSUB: compute a single body-body or body-cell interaction.
 */

void gravsub(p, ProcessId, level)
  nodeptr p;               /* body or cell to interact with     */
  unsigned ProcessId;
  int level;
{
    double sqrt();
    real drabs, phii, mor3;
    vector ai, quaddr, tmpv;
    real dr5inv, phiquad, drquaddr;
    vector tmpv1;
    if (p != Local.pmem) {
        
      //SUBV(Local.dr, Pos(p), Local.pos0);
      SUBV(tmpv, Pos(p), Local.pos0);
      COPYV(Local.dr, tmpv);
      DOTVP(Local.drsq, Local.dr, Local.dr);
    }

    Local.drsq += epssq;
    drabs = sqrt((double) Local.drsq);
    phii = Mass(p) / drabs;
    Local.phi0 -= phii;
    mor3 = phii / Local.drsq;
    MULVS(ai, Local.dr, mor3);
    ADDV(tmpv1, Local.acc0, ai); 
    COPYV(Local.acc0, tmpv1);
    if(Type(p) != BODY) {                  /* a body-cell/leaf interaction? */
       Local.mynbcterm++;
#ifdef QUADPOLE
       dr5inv = 1.0/(Local.drsq * Local.drsq * drabs);
       MULMV(quaddr, Quad(p), Local.dr);
       DOTVP(drquaddr, Local.dr, quaddr);
       phiquad = -0.5 * dr5inv * drquaddr;
       Local.phi0 += phiquad;
       phiquad = 5.0 * phiquad / Local.drsq;
       MULVS(ai, Local.dr, phiquad);
       SUBV(Local.acc0, Local.acc0, ai);
       MULVS(quaddr, quaddr, dr5inv);   
       SUBV(Local.acc0, Local.acc0, quaddr);
#endif
    }
    else {                                      /* a body-body interaction  */
       Local.myn2bterm++;
    }
   /*  if(ProcessId == 0) { */
/*       PRTV("GP 2", Local.acc0); */
/*     } */
}


/*
 * HACKWALK: walk the tree opening cells too close to a given point.
 */

proced hacksub; 

void hackwalk(sub, ProcessId)
  proced sub;                                /* routine to do calculation */
  unsigned ProcessId;
{
    walksub((nodeptr) G_root, rsize * rsize, ProcessId);
}

/*
 * WALKSUB: recursive routine to do hackwalk operation.
 */


void walksub(n, dsq, ProcessId)
   nodeptr n;                        /* pointer into body-tree    */
   real dsq;                         /* size of box squared       */
   unsigned ProcessId;
{
   bool subdivp();
   nodeptr nn;
   leafptr l;
   bodyptr p;
   int i;
   int j;	
    
   if (subdivp(n, dsq, ProcessId)) {
      if (Type(n) == CELL) {
	for(j = 0; j < NSUB; j++) {
	//for (nn = Subp(n); nn < Subp(n) + NSUB; nn++) {
	  nn = Subp(n)[j];  
	    if (nn != NULL) {
	       walksub(nn, dsq / 4.0, ProcessId);
 	    } 
	 }
      }
      else {
	 l = (leafptr) n;
	 for (i = 0; i < l->num_bodies; i++) {
	    p = Bodyp(l)[i];
	    if (p != Local.pskip) {
	       gravsub(p, ProcessId);
	    }
	    else {
	       Local.skipself = TRUE;
	    }
	 }
      }
   }
   else {
      gravsub(n, ProcessId);
   }
}



/*
 * SUBDIVP: decide if a node should be opened.
 * Side effects: sets  pmem,dr, and drsq.
 */

bool subdivp(p, dsq, ProcessId)
   nodeptr p;                      /* body/cell to be tested    */
   real dsq;                                /* size of cell squared      */
   unsigned ProcessId;
{
  vector tmpv;
  //SUBV(Local.dr, Pos(p), Local.pos0);                                                                                   
  SUBV(tmpv, Pos(p), Local.pos0);
  DOTVP(Local.drsq, Local.dr, Local.dr);
  Local.pmem = p;
  COPYV(Local.dr, tmpv);
  return (tolsq * Local.drsq < dsq);
}


