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
  Usage: BARNES <options> < inputfile

  Command line options:
  
  -h : Print out input file description
  
  Input parameters should be placed in a file and redirected through
  standard input.  There are a total of twelve parameters, and all of 
  them have default values.
  
  1) infile (char*) : The name of an input file that contains particle
  data.
  
  The format of the file is:
  a) An int representing the number of particles in the distribution
  b) An int representing the dimensionality of the problem (3-D)
  c) A double representing the current time of the simulation
  d) Doubles representing the masses of all the particles
  e) A vector (length equal to the dimensionality) of doubles
  representing the positions of all the particles
  f) A vector (length equal to the dimensionality) of doubles
  representing the velocities of all the particles
  
  Each of these numbers can be separated by any amount of whitespace.
  2) nbody (int) : If no input file is specified (the first line is
  blank), this number specifies the number of particles to generate
  under a plummer model.  Default is 16384.
  3) seed (int) : The seed used by the random number generator.
  Default is 123.
  4) outfile (char*) : The name of the file that snapshots will be
  printed to. This feature has been disabled in the SPLASH release.
  Default is NULL.
  5) dtime (double) : The integration time-step.
  Default is 0.025.
  6) eps (double) : The usual potential softening
  Default is 0.05.
  7) tol (double) : The cell subdivision tolerance.
  Default is 1.0.
  8) fcells (double) : Number of cells created = fcells * number of 
  leaves.
  Default is 2.0.
  9) fleaves (double) : Number of leaves created = fleaves * nbody.
  Default is 0.5.
  10) tstop (double) : The time to stop integration.
  Default is 0.075.
  11) dtout (double) : The data-output interval.
  Default is 0.25.
  12) NPROC (int) : The number of processors.
  Default is 1.
*/


#define global  /* nada */

#include "code.h"
#include "defs.h"
#include <math.h>
#include <time.h>
#include <upc.h>
#include "sys/time.h"

const char *defv[] = {                 /* DEFAULT PARAMETER VALUES              */
  /* file names for input/output                                         */
  "in=",                        /* snapshot of initial conditions        */
  "out=",                       /* stream of output snapshots            */
  
  /* params, used if no input specified, to make a Plummer Model         */
  "nbody=16384",                /* number of particles to generate       */
  "seed=123",                   /* random number generator seed          */
  
  /* params to control N-body integration                                */
  "dtime=0.025",                /* integration time-step                 */
  "eps=0.05",                   /* usual potential softening             */
  "tol=1.0",                    /* cell subdivision tolerence            */
  "fcells=2.0",                 /* cell allocation parameter             */
  "fleaves=0.5",                 /* leaf allocation parameter             */
  
  "tstop=0.075",                 /* time to stop integration              */
  "dtout=0.25",                 /* data-output interval                  */
  
  "NPROC=1",                    /* number of processors                  */
};

void SlaveStart ();
void stepsystem (unsigned int ProcessId);
void ComputeForces ();
void Help();
extern void stop_trace(int );
extern void start_trace(int);
extern void reset_trace();
extern void save_trace(int);
extern void reset_ptr_add_stat(int);
extern void start_ptr_add_stat(int);
extern void stop_ptr_add_stat(int);
extern int count_psharedI_add(int);
extern int count_pshared_add(int);
extern int count_shared_add(int);
FILE *fopen();

int start_procs;
int start_bods;
cellptr shared ctabws;
leafptr shared ltabws;
shared real globtout;
shared real globtnow;
shared int globnstep;

double wctime(){
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec + 1E-6 * tv.tv_usec);
}


/*
 * ANLINIT : initialize ANL macros
 */
void ANLinit()
{
  
  int i;
  if(MYTHREAD == 0) {
    CountLock = upc_global_lock_alloc();
    io_lock = upc_global_lock_alloc();
  }

  upc_forall(i = 0; i < MAXLOCK; i++; &CellLock[i]) {
     CellLock[i] = (upc_lock_t *)upc_global_lock_alloc();
     if(CellLock[i] == 0) {
       printf("%d: Can't allocate lock %d\n", MYTHREAD, i);
       exit(1);
     } 
     
   }
}


/*
 * TESTDATA: generate Plummer model initial conditions for test runs,
 * scaled to units such that M = -4E = G = 1 (Henon, Hegge, etc).
 * See Aarseth, SJ, Henon, M, & Wielen, R (1974) Astr & Ap, 37, 183.
 */

#define MFRAC  0.999                /* mass cut off at MFRAC of total */

void pickshell(vec, rad)
   shared [] real *vec;                     /* coordinate vector chosen */
   real rad;                       /* radius of chosen point */
{
   register int k;
   double rsq, xrand(), sqrt(), rsc;

   do {
      for (k = 0; k < NDIM; k++) {
	 vec[k] = xrand(-1.0, 1.0);
      }
      DOTVP(rsq, vec, vec);
   } while (rsq > 1.0);

   rsc = rad / sqrt(rsq);
   MULVS(vec, vec, rsc);
   
  
}

int intpow(i,j)
  int i,j;
{   
    int k;
    int temp = 1;

    for (k = 0; k < j; k++)
        temp = temp*i;
    return temp;
}

void testdata()
{
   real rsc, vsc, sqrt(), xrand(), pow(), rsq, r, v, x, y;
   vector cmr, cmv;
   bodyptr p;
   int rejects = 0;
   int k;
   int halfnbody, i;
   float offset;
   bodyptr cp;
   double tmp;
   int count;

   headline = (char *) "Hack code: Plummer model";
   Local.tnow = 0.0;
   bodytab = (shared bodyptr) upc_global_alloc( THREADS+1 , (nbody/THREADS) * sizeof(body));
   if (bodytab == NULL) {
      error("testdata: not enuf memory\n");
   }
   rsc = 9 * PI / 16;
   vsc = sqrt(1.0 / rsc);

   CLRV(cmr);
   CLRV(cmv);

   halfnbody = nbody / 2;
   if (nbody % 2 != 0) halfnbody++;
   count = 0;
   for (p = bodytab; p < bodytab+halfnbody; p++, count++) {
      Type(p) = BODY;
      Mass(p) = 1.0 / nbody;
      Cost(p) = 1;
      // printf("%d: Set body %d M = %lf in 0x%llx\n", MYTHREAD, count, Mass(p), p);
      
      r = 1 / sqrt(pow(xrand(0.0, MFRAC), -2.0/3.0) - 1);
      /*   reject radii greater than 10 */
      while (r > 9.0) {
	 rejects++;
	 r = 1 / sqrt(pow(xrand(0.0, MFRAC), -2.0/3.0) - 1);
      }        
      pickshell(Pos(p), rsc * r);
      ADDV(cmr, cmr,  Pos(p));
    
      do {
	 x = xrand(0.0, 1.0);
	 y = xrand(0.0, 0.1);

      } while (y > x*x * pow(1 - x*x, 3.5));

      v = sqrt(2.0) * x / pow(1 + r*r, 0.25);
      pickshell(Vel(p), vsc * v);
      ADDV(cmv, cmv, Vel(p));
      
   }

   offset = 4.0;

   for (p = bodytab + halfnbody; p < bodytab+nbody; p++, count++) {
      Type(p) = BODY;
      Mass(p) = 1.0 / nbody;
      Cost(p) = 1;
      //printf("%d: Set body %d M = %lf in 0x%llx\n", MYTHREAD, count, Mass(p), p);
      cp = p - halfnbody;
      for (i = 0; i < NDIM; i++){
	 Pos(p)[i] = Pos(cp)[i] + offset; 
	 ADDV(cmr, cmr,  Pos(p));
	 Vel(p)[i] = Vel(cp)[i];
	 ADDV(cmv, cmv, Vel(p));
      }
   }

   DIVVS(cmr, cmr, (real) nbody);
   DIVVS(cmv, cmv, (real) nbody);
   

   for (p = bodytab; p < bodytab+nbody; p++) {
    
     SUBV(Pos(p), Pos(p), cmr);
     SUBV(Vel(p), Vel(p), cmv);
     
     
   }
}

/*
 * PICKSHELL: pick a random point on a sphere of specified radius.
 */



/*
 * SETBOUND: Compute the initial size of the root of the tree; only done
 * before first time step, and only processor 0 does it
 */
void setbound()
{
   int i;
   real side ;
   bodyptr p;

   SETVS(Local.min,1E99);
   SETVS(Local.max,-1E99);
   side=0;

   for (p = bodytab; p < bodytab+nbody; p++) {
      for (i=0; i<NDIM;i++) {
	 if (Pos(p)[i]<Local.min[i]) Local.min[i]=Pos(p)[i] ;
	 if (Pos(p)[i]>Local.max[i])  Local.max[i]=Pos(p)[i] ;
      }
   }
    
   SUBV(Local.max,Local.max,Local.min);
   for (i=0; i<NDIM;i++) if (side<Local.max[i]) side=Local.max[i];
   ADDVS(rmin,Local.min,-side/100000.0);
   rsize = 1.00002*side;
   SETVS(max,-1E99);
   SETVS(min,1E99);
}



void startrun()
{
   string getparam();
   int getiparam();
   bool getbparam();
   double getdparam();
   int seed;
   seed = 123;
   nbody = start_bods;
   outfile = NULL; 
   dtime =  0.025; 
   dthf = 0.5 * dtime;
   eps = 0.05; 
   epssq = eps*eps;
   tol = 1.0;
   tolsq = tol*tol;
   fcells = 2.00;
   fleaves = 2.0;
   tstop = 0.075;
   dtout = 0.25; 
   NPROC = start_procs; 
   Local.nstep = 0;
   globnstep = 0;
   pranset(seed);
   testdata();
   setbound();
   Local.tout = Local.tnow + dtout;
   globtout = Local.tout;
   globtnow = Local.tnow;
}

void tab_init()
{
   cellptr pc;
   int i;
   char *starting_address, *ending_address;

   /*allocate leaf/cell space */
   if(MYTHREAD == 0) {
     maxleaf = (int) ((double) fleaves * nbody);
     maxcell = fcells * maxleaf;
     ctabws = (cellptr) upc_global_alloc(NPROC, (maxcell / NPROC) * sizeof(cell));
     ltabws = (leafptr) upc_global_alloc(NPROC, (maxleaf / NPROC) * sizeof(leaf));
     
   }
   upc_barrier;
   
   Local.ctab = (shared [] cell *) (cellptr)(ctabws+MYTHREAD);
   Local.ltab = (shared [] leaf *) (leafptr)(ltabws+MYTHREAD);
   
   /*allocate space for personal lists of body pointers */
   maxmybody = (nbody+maxleaf*MAX_BODIES_PER_LEAF)/NPROC; 
   Local.mybodytab = malloc(maxmybody*sizeof(bodyptr));
     /* (bodyptr*) upc_global_alloc(NPROC,maxmybody*sizeof(bodyptr));  */
   /* space is allocated so that every */
   /* process can have a maximum of maxmybody pointers to bodies */ 
   /* then there is an array of bodies called bodytab which is  */
   /* allocated in the distribution generation or when the distr. */
   /* file is read */
   maxmycell = maxcell / NPROC;
   maxmyleaf = maxleaf / NPROC;
   /* celltabws = (cellptr*) upc_global_alloc(NPROC,maxmycell*sizeof(cellptr)); */
   Local.mycelltab = (cellptr*) malloc(maxmycell*sizeof(cellptr));
   Local.myleaftab = (leafptr*) malloc(maxmyleaf*sizeof(leafptr));
  
}


int main(int argc, char *argv[])
{
   unsigned ProcessId = 0;
   int c;

#ifdef DO_TRACE   
   save_trace(MYTHREAD);
   upc_barrier;
   reset_trace();
   reset_ptr_add_stat(MYTHREAD);
#endif

   

   if(MYTHREAD == 0) {
     while ((c = getopt(argc, argv, "h")) != -1) {
       switch(c) {
       case 'h': 
	 Help(); 
	 exit(-1); 
	 break;
       default:
	 fprintf(stderr, "Only valid option is \"-h\".\n");
	 exit(-1);
	 break;
       }
     }
   }
   
   //start_procs = atoi(argv[2]);
   start_procs = THREADS;
   start_bods = atoi(argv[1]);
   //NPROC = start_bods;
   ANLinit();
   if(MYTHREAD == 0) {
     initparam(argv, (string *) defv);
     startrun();
     //initoutput();
   }
   
   tab_init();

   if(MYTHREAD == 0) {  
     tracktime = 0;
     partitiontime = 0;
     treebuildtime = 0;
     forcecalctime = 0;
     
   /* Create the slave processes: number of processors less one,
      since the master will do work as well */
     current_id = 0;
     CLOCK(computestart);
     printf("COMPUTESTART  = %f\n",computestart);
   }
   
   SlaveStart();

   barrier;
   if (MYTHREAD == 0)
     CLOCK(computeend);

   /* WAIT_FOR_END(NPROC-1); */

   if (MYTHREAD == 0) {
     printf("COMPUTEEND    = %f\n",computeend);
     printf("COMPUTETIME   = %f\n",computeend - computestart);
     printf("TRACKTIME     = %f\n",tracktime); 
     printf("PARTITIONTIME = %f\t%5.2f\n",partitiontime,
	    ((float)partitiontime)/tracktime);
     printf("TREEBUILDTIME = %f\t%5.2f\n",treebuildtime, 
	    ((float)treebuildtime)/tracktime);
     printf("FORCECALCTIME = %f\t%5.2f\n",forcecalctime,
	    ((float)forcecalctime)/tracktime);
     printf("RESTTIME      = %f\t%5.2f\n",
	    tracktime - partitiontime - 
	    treebuildtime - forcecalctime, 
	    ((float)(tracktime-partitiontime-
		     treebuildtime-forcecalctime))/
	    tracktime);
   }
#ifdef DO_TRACE
   start_trace(MYTHREAD);
   start_ptr_add_stat(MYTHREAD);
   printf("THREAD %d : API = %d, AP1 = %d, APS = %d\n", MYTHREAD, count_psharedI_add(MYTHREAD),
	  count_pshared1_add(MYTHREAD), count_shared_add(MYTHREAD));
#endif
   
}

/*
 * INIT_ROOT: Processor 0 reinitialize the global root at each time step
 */
void init_root (ProcessId)
   unsigned int ProcessId;
{
   int i;

  
   G_root=(shared cell *) Local.ctab;
   Type(G_root) = CELL;
   Done(G_root) = FALSE;
   Level(G_root) = IMAX >> 1;
   for (i = 0; i < NSUB; i++) {
     Subp(G_root)[i] = NULL;
   }
   
   Local.mynumcell=2;
  
}

int Log_base_2(number)
int number;
{
   int cumulative;
   int out;

   cumulative = 1;
   for (out = 0; out < 20; out++) {
      if (cumulative == number) {
         return(out);
      }
      else {
         cumulative = cumulative * 2;
      }
   }

   fprintf(stderr,"Log_base_2: couldn't find log2 of %d\n", number);
   exit(-1);
   //make the compiler happy
   return 0;
}


void find_my_initial_bodies(btab, nbody, ProcessId)
bodyptr  btab;
int nbody;
unsigned int ProcessId;
{
  int Myindex;
  int intpow();
  int equalbodies;
  int extra,offset,i;

  Local.mynbody = nbody / NPROC;
  extra = nbody % NPROC;
  if (ProcessId < extra) {
    Local.mynbody++;    
    offset = Local.mynbody * ProcessId;
  }
  if (ProcessId >= extra) {
    offset = (Local.mynbody+1) * extra + (ProcessId - extra) 
       * Local.mynbody; 
  }
  for (i=0; i < Local.mynbody; i++) {
    
     Local.mybodytab[i] = &(btab[offset+i]);
     /* printf("Process %d initializing %d with 0x%llx, mass = %lf\n", MYTHREAD, i, Local.mybodytab[i], Mass(Local.mybodytab[i])); */
  }
  upc_barrier;
}

/*
 * SLAVESTART: main task for each processor
 */
void SlaveStart()
{
   unsigned int ProcessId;

   /* Get unique ProcessId */
   upc_lock(CountLock);
   ProcessId = current_id++;
   ProcessId = MYTHREAD;
   upc_unlock(CountLock);

/* POSSIBLE ENHANCEMENT:  Here is where one might pin processes to
   processors to avoid migration */

   /* initialize mybodytabs */
  /*  Local[ProcessId].mybodytab = Local[0].mybodytab + ProcessId; */
   /* note that every process has its own copy   */
   /* of mybodytab, which was initialized to the */
   /* beginning of the whole array by proc. 0    */
   /* before create                              */
  /*  Local[ProcessId].mycelltab = Local[0].mycelltab +  ProcessId; */
/*    Local[ProcessId].myleaftab = Local[0].myleaftab +  ProcessId; */
/* POSSIBLE ENHANCEMENT:  Here is where one might distribute the
   data across physically distributed memories as desired. 

   One way to do this is as follows:

   int i;

   if (ProcessId == 0) {
     for (i=0;i<NPROC;i++) {
       Place all addresses x such that 
         &(Local[i]) <= x < &(Local[i])+
           sizeof(struct local_memory) on node i
       Place all addresses x such that 
         &(Local[i].mybodytab) <= x < &(Local[i].mybodytab)+
           maxmybody * sizeof(bodyptr) - 1 on node i
       Place all addresses x such that 
         &(Local[i].mycelltab) <= x < &(Local[i].mycelltab)+
           maxmycell * sizeof(cellptr) - 1 on node i
       Place all addresses x such that 
         &(Local[i].myleaftab) <= x < &(Local[i].myleaftab)+
           maxmyleaf * sizeof(leafptr) - 1 on node i
     }
   }

   barrier(Barstart,NPROC);

*/

   Local.tout = globtout;
   Local.tnow = globtnow;
   Local.nstep = globnstep;

   find_my_initial_bodies(bodytab, nbody, ProcessId);

   /* main loop */
   while (Local.tnow < tstop + 0.1 * dtime) {
      stepsystem(ProcessId);
   }
}

/*
 * STARTRUN: startup hierarchical N-body code.
 */

void find_my_bodies(mycell, work, direction, ProcessId)
  nodeptr mycell;
  int work;
  int direction;
  unsigned ProcessId;
{
   int i;
   leafptr l;
   nodeptr qptr;

   if (Type(mycell) == LEAF) {
      l = (leafptr) mycell;
      for (i = 0; i < l->num_bodies; i++) {
	 if (work >= Local.workMin - .1) {
	    if((Local.mynbody+2) > maxmybody) {
	       error("find_my_bodies: Processor %d needs more than %d bodies; increase fleaves\n",ProcessId, maxmybody); 
	    }
	    Local.mybodytab[Local.mynbody++] = 
	       Bodyp(l)[i];
	 }
	 work += Cost(Bodyp(l)[i]);
	 if (work >= Local.workMax-.1) {
	    break;
	 }
      }
   }
   else {
      for(i = 0; (i < NSUB) && (work < (Local.workMax - .1)); i++){
	 qptr = Subp(mycell)[Child_Sequence[direction][i]];
	 if (qptr!=NULL) {
	    if ((work+Cost(qptr)) >= (Local.workMin -.1)) {
	       find_my_bodies(qptr,work, Direction_Sequence[direction][i],
			      ProcessId);
	    }
	    work += Cost(qptr);
	 }
      }
   }
   
//   printf("%d: FMB has %d \n", ProcessId, Local.mynbody);
}


/*
 * HOUSEKEEP: reinitialize the different variables (in particular global
 * variables) between each time step.
 */

void Housekeep(ProcessId)
unsigned ProcessId;
{
   Local.myn2bcalc = Local.mynbccalc 
      = Local.myselfint = 0;
   SETVS(Local.min,1E99);
   SETVS(Local.max,-1E99);
}



/*
 * STEPSYSTEM: advance N-body system one time-step.
 */

void
stepsystem (ProcessId)
   unsigned int ProcessId;
{
    int i;
    real Cavg;
    bodyptr p,*pp;
    vector acc1, dacc, dvel, vel1, dpos;
    int intpow();
    double time;
    double trackstart, trackend;
    double partitionstart, partitionend;
    double treebuildstart, treebuildend;
    double forcecalcstart, forcecalcend;

    if (Local.nstep == 2) {
/* POSSIBLE ENHANCEMENT:  Here is where one might reset the
   statistics that one is measuring about the parallel execution */
    }

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
         CLOCK(trackstart);
    }

    if (ProcessId == 0) {
       init_root(ProcessId);
    }
    else {
       Local.mynumcell = 0;
       Local.mynumleaf = 0;
    }
    
   /*  for (i=0; i < Local.mynbody; i++) { */
      
/*      printf("%d: Process  sees %d with 0x%llx, mass = %lf\n", ProcessId, i, Local.mybodytab[i], Mass(Local.mybodytab[i])); */
/*     } */


    /* start at same time */
    upc_barrier;

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
        CLOCK(treebuildstart);
    }

    /* load bodies into tree   */
    maketree(ProcessId);
    if ((ProcessId == 0) && (Local.nstep >= 2)) {
      CLOCK(treebuildend); 	
      treebuildtime += treebuildend - treebuildstart;
    }

    Housekeep(ProcessId);
    Cavg = (real) Cost(G_root) / (real)NPROC ;
    Local.workMin = (int) (Cavg * ProcessId);
    Local.workMax = (int) (Cavg * (ProcessId + 1)
				      + (ProcessId == (NPROC - 1)));

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
        CLOCK(partitionstart); 
    }
   
    Local.mynbody = 0;
    find_my_bodies(G_root, 0, BRC_FUC, ProcessId );
    if ((ProcessId == 0) && (Local.nstep >= 2)) {
      CLOCK(partitionend);
      partitiontime += partitionend - partitionstart;
    }

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
      CLOCK(forcecalcstart);
    }

#ifdef DO_TRACE
    start_trace(MYTHREAD);
    start_ptr_add_stat(MYTHREAD);
#endif
    ComputeForces(ProcessId);
#ifdef DO_TRACE    
    stop_trace(MYTHREAD);
    stop_ptr_add_stat(MYTHREAD);
#endif

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
      CLOCK(forcecalcend); 
        forcecalctime += forcecalcend - forcecalcstart;
    }

    /* advance my bodies */
    for (pp = Local.mybodytab;
	 pp < Local.mybodytab+Local.mynbody; pp++) {
       p = *pp;
       MULVS(dvel, Acc(p), dthf);              
       ADDV(vel1, Vel(p), dvel);               
       MULVS(dpos, vel1, dtime);               
       ADDV(Pos(p), Pos(p), dpos);             
       ADDV(Vel(p), vel1, dvel);
       
       for (i = 0; i < NDIM; i++) {
          if (Pos(p)[i]<Local.min[i]) {
	     Local.min[i]=Pos(p)[i];
	  }
          if (Pos(p)[i]>Local.max[i]) {
	     Local.max[i]=Pos(p)[i] ;
	  }
       }
    }
    upc_lock(CountLock);
    for (i = 0; i < NDIM; i++) {
       if (min[i] > Local.min[i]) {
	  min[i] = Local.min[i];
       }
       if (max[i] < Local.max[i]) {
	  max[i] = Local.max[i];
       }
    }
    upc_unlock(CountLock);

    /* bar needed to make sure that every process has computed its min */
    /* and max coordinates, and has accumulated them into the global   */
    /* min and max, before the new dimensions are computed	       */
    upc_barrier;

    if ((ProcessId == 0) && (Local.nstep >= 2)) {
        CLOCK(trackend); 
        tracktime += trackend - trackstart;
    }
    if (ProcessId==0) {
      rsize=0;
      SUBV(max,max,min);
      for (i = 0; i < NDIM; i++) {
	if (rsize < max[i]) {
	   rsize = max[i];
	}
      }
      ADDVS(rmin,min,-rsize/100000.0);
      rsize = 1.00002*rsize;
      SETVS(min,1E99);
      SETVS(max,-1E99);
    }
    Local.nstep++;
    Local.tnow = Local.tnow + dtime;
}

void
ComputeForces (ProcessId)
   unsigned int ProcessId;
{
   bodyptr p,*pp;
   vector acc1, dacc, dvel, vel1, dpos;
   vector tmpv;	
   for (pp = Local.mybodytab;
	pp < Local.mybodytab+Local.mynbody;pp++) {  
      p = *pp;	
      COPYV(acc1, Acc(p));
      Cost(p)=0;
      hackgrav(p,ProcessId);
      Local.myn2bcalc += Local.myn2bterm; 
      Local.mynbccalc += Local.mynbcterm;
      if (!Local.skipself) {       /*   did we miss self-int?  */
	 Local.myselfint++;        /*   count another goofup   */
      }
      if (Local.nstep > 0) {
	 /*   use change in accel to make 2nd order correction to vel      */
	 SUBV(dacc, Acc(p), acc1);
	 MULVS(dvel, dacc, dthf);
	 ADDV(tmpv, Vel(p), dvel);
	 COPYV(Vel(p), tmpv);
	 
      }
   }
}

/* 
 * FIND_MY_INITIAL_BODIES: puts into mybodytab the initial list of bodies 
 * assigned to the processor.  
 */


void
Help () 
{
   printf("There are a total of twelve parameters, and all of them have default values.\n");
   printf("\n");
   printf("1) infile (char*) : The name of an input file that contains particle data.  \n");
   printf("    The format of the file is:\n");
   printf("\ta) An int representing the number of particles in the distribution\n");
   printf("\tb) An int representing the dimensionality of the problem (3-D)\n");
   printf("\tc) A double representing the current time of the simulation\n");
   printf("\td) Doubles representing the masses of all the particles\n");
   printf("\te) A vector (length equal to the dimensionality) of doubles\n");
   printf("\t   representing the positions of all the particles\n");
   printf("\tf) A vector (length equal to the dimensionality) of doubles\n");
   printf("\t   representing the velocities of all the particles\n");
   printf("\n");
   printf("    Each of these numbers can be separated by any amount of whitespace.\n");
   printf("\n");
   printf("2) nbody (int) : If no input file is specified (the first line is blank), this\n");
   printf("    number specifies the number of particles to generate under a plummer model.\n");
   printf("    Default is 16384.\n");
   printf("\n");
   printf("3) seed (int) : The seed used by the random number generator.\n");
   printf("    Default is 123.\n");
   printf("\n");
   printf("4) outfile (char*) : The name of the file that snapshots will be printed to. \n");
   printf("    This feature has been disabled in the SPLASH release.\n");
   printf("    Default is NULL.\n");
   printf("\n");
   printf("5) dtime (double) : The integration time-step.\n");
   printf("    Default is 0.025.\n");
   printf("\n");
   printf("6) eps (double) : The usual potential softening\n");
   printf("    Default is 0.05.\n");
   printf("\n");
   printf("7) tol (double) : The cell subdivision tolerance.\n");
   printf("    Default is 1.0.\n");
   printf("\n");
   printf("8) fcells (double) : The total number of cells created is equal to \n");
   printf("    fcells * number of leaves.\n");
   printf("    Default is 2.0.\n");
   printf("\n");
   printf("9) fleaves (double) : The total number of leaves created is equal to  \n");
   printf("    fleaves * nbody.\n");
   printf("    Default is 0.5.\n");
   printf("\n");
   printf("10) tstop (double) : The time to stop integration.\n");
   printf("    Default is 0.075.\n");
   printf("\n");
   printf("11) dtout (double) : The data-output interval.\n");
   printf("    Default is 0.25.\n");
   printf("\n");
   printf("12) NPROC (int) : The number of processors.\n");
   printf("    Default is 1.\n");
}
