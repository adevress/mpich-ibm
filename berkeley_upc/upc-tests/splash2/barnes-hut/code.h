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
 * CODE.H: define various global things for CODE.C.
 */

#ifndef _CODE_H_
#define _CODE_H_

#include "defs.h"
#include <upc.h>

#define PAD_SIZE (PAGE_SIZE / (sizeof(int)))

/* Defined by the input file */
string headline; 	/* message describing calculation */
string infile; 		/* file name for snapshot input */
shared string outfile; 		/* file name for snapshot output */
shared real dtime; 		/* timestep for leapfrog integrator */
shared real dtout; 		/* time between data outputs */
shared real tstop; 		/* time to stop calculation */
shared int nbody; 		/* number of bodies in system */
shared real fcells; 		/* ratio of cells/leaves allocated */
shared real fleaves; 		/* ratio of leaves/bodies allocated */
shared real tol; 		/* accuracy parameter: 0.0 => exact */
shared real tolsq; 		/* square of previous */
shared real eps; 		/* potential softening parameter */
shared real epssq; 		/* square of previous */
shared real dthf; 		/* half time step */
shared int NPROC; 		/* Number of Processors */

shared int maxcell;		/* max number of cells allocated */
shared int maxleaf;		/* max number of leaves allocated */
shared int maxmybody;		/* max no. of bodies allocated per processor */
shared int maxmycell;		/* max num. of cells to be allocated */
shared int maxmyleaf;		/* max num. of leaves to be allocated */
bodyptr shared bodytab; 	/* array size is exactly nbody bodies */

upc_lock_t* shared CellLock[MAXLOCK];
    
//struct GlobalMemory  {	/* all this info is for the whole system */
shared int n2bcalc;       /* total number of body/cell interactions  */
shared int nbccalc;       /* total number of body/body interactions  */
shared int selfint;       /* number of self interactions             */
shared real mtot;         /* total mass of N-body system             */
shared real etot[3];      /* binding, kinetic, potential energy      */
shared matrix keten;      /* kinetic energy tensor                   */
shared matrix peten;      /* potential energy tensor                 */
shared vector cmphase[2]; /* center of mass coordinates and velocity */
shared vector amvec;      /* angular momentum vector                 */
shared cellptr G_root;    /* root of the whole tree                  */
shared vector rmin;       /* lower-left corner of coordinate box     */
shared vector min;        /* temporary lower-left corner of the box  */
shared vector max;        /* temporary upper right corner of the box */
shared real rsize;        /* side-length of integer coordinate box   */
   
upc_lock_t *CountLock; /* Lock on the shared variables            */
upc_lock_t *NcellLock; /* Lock on the counter of array of cells for loadtree */
upc_lock_t *NleafLock;/* Lock on the counter of array of leaves for loadtree */
upc_lock_t *io_lock;
shared double createstart,createend,computestart,computeend;
shared double trackstart, trackend, tracktime;
shared double partitionstart, partitionend, partitiontime;
shared double treebuildstart, treebuildend, treebuildtime;
shared double forcecalcstart, forcecalcend, forcecalctime;
shared unsigned int current_id;
shared int k; /*for memory allocation in code.C */
//};
//shared struct GlobalMemory * shared Global;

/* This structure is needed because under the sproc model there is no
 * per processor private address space. 
 */
struct local_memory {
   /* Use padding so that each processor's variables are on their own page */
   //int pad_begin[PAD_SIZE];

   real tnow;        	/* current value of simulation time */
   real tout;         	/* time next output is due */
   int nstep;      	/* number of integration steps so far */

   int workMin, workMax;/* interval of cost to be treated by a proc */

   vector min, max; 	/* min and max of coordinates for each Proc. */

   int mynumcell; 	/* num. of cells used for this proc in ctab */
   int mynumleaf; 	/* num. of leaves used for this proc in ctab */
   int mynbody;   	/* num bodies allocated to the processor */
   bodyptr * mybodytab;	/* array of bodies allocated / processor */
   int myncell; 	/* num cells allocated to the processor */
   cellptr * mycelltab;	/* array of cellptrs allocated to the processor */
   int mynleaf; 	/* number of leaves allocated to the processor */
   leafptr * myleaftab; 	/* array of leafptrs allocated to the processor */
   shared [] cell *ctab;	/* array of cells used for the tree. */
   shared [] leaf * ltab;	/* array of cells used for the tree. */

   int myn2bcalc; 	/* body-body force calculations for each processor */
   int mynbccalc; 	/* body-cell force calculations for each processor */
   int myselfint; 	/* count self-interactions for each processor */
   int myn2bterm; 	/* count body-body terms for a body */
   int mynbcterm; 	/* count body-cell terms for a body */
   bool skipself; 	/* true if self-interaction skipped OK */
   bodyptr pskip;       /* body to skip in force evaluation */
   vector pos0;         /* point at which to evaluate field */
   real phi0;           /* computed potential at pos0 */
   vector acc0;         /* computed acceleration at pos0 */
   vector dr;  		/* data to be shared */
   real drsq;      	/* between gravsub and subdivp */
   nodeptr pmem;	/* remember particle data */

   nodeptr Current_Root;
   int Root_Coords[NDIM];

   real mymtot;      	/* total mass of N-body system */
   real myetot[3];   	/* binding, kinetic, potential energy */
   matrix myketen;   	/* kinetic energy tensor */
   matrix mypeten;   	/* potential energy tensor */
   vector mycmphase[2];	/* center of mass coordinates */
   vector myamvec;   	/* angular momentum vector */

   int pad_end[PAD_SIZE];
};
struct local_memory Local;


/* Prototype all  functions otherwise we get a slew of warnings*/
void hackgrav(bodyptr, unsigned);
/* void gravsub(nodeptr, unsigned, int); */
void hackwalk(proced, unsigned);
void walksub(nodeptr, real, unsigned);
bool subdiv(nodeptr, real, unsigned);
void init_root(unsigned int);

void error(const char* msg, ...);
void pranset(int seed);
void initparam (string * s1, string *s2);
void maketree(unsigned int);

#endif
