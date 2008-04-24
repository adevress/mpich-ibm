/*$Id: adjust.c,v 1.10 2003/06/22 07:19:31 d3h325 Exp $*/
/***********************************************************************\
* Purpose: adjusts timers in tracefiles generated by ga_trace,          *
*          sorts events, combines tracefiles, and reformats the data    *
* Jarek Nieplocha, 10.15.1993                                           *
* 11.09.99 - updated for GA_OFFSET + error diagnostics                  *
\***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#define MAX_EVENTS 1000000
#define MAX_ARRAYS 8
#define MAX_EVENT_TYPES 32
#define MAX_PROC 512 


/* this value must be exactly like in the GA package source */
#define GA_OFFSET 1000

#define ABS(x) ((x)>0 ? (x) :(-x))
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))

/* if enabled, program prints GA usage statistics & requires EXTRA MEMORY 
 * NOT recommended for large number of arrays/processes and/or event types
 */
#define PRINT_STATS 1
#ifdef PRINT_STATS
   double stat[MAX_PROC][MAX_ARRAYS][MAX_EVENT_TYPES];
   unsigned int acc[MAX_PROC][MAX_ARRAYS][MAX_EVENT_TYPES];
   void printstat(), update();
#endif

int proc=0, arrays=0, event_types=0; 

main(argc,argv)
int argc;
char **argv;
{
long int p,i,j,k,MR,events=0;
unsigned long int  *clock_base, *times, base=0, *tbase, maxtime=0;
int *record;
int ga=-GA_OFFSET;

static int tcomp();
int flag;
          
FILE *fin,*fout, *fdistr;
char *foutname="adjust.ed", fdstrname[15], finname[8];
 int distrdata;
 

   if(argc < 2){
      printf("Usage: adjust <array handle> [<max number of events>] \n");
      exit(1);
   } 

   sscanf(argv[1],"%d",&ga);
   ga += GA_OFFSET;

   /* find the distribution file name and open it */
   sprintf(fdstrname, "distrib.%d", ga-GA_OFFSET);
   if((fdistr = fopen(fdstrname,"r")) == NULL) {
       fprintf(stderr, "This array handle %d is invalid!\n", ga-GA_OFFSET);
       exit(3);
   }

   fscanf(fdistr, "%d", &p);
   
   if(argc>2) sscanf(argv[3],"%ld",&MR);
   else MR =  MAX_EVENTS;
   
   if(p>MAX_PROC){
     fprintf(stderr,"Only %d procs allowed - you must modify this program\n",MAX_PROC); 
     exit(1);
   }

   if(p<1){
     fprintf(stderr,"number of processes must be > 0: %d\n",p);
     exit(2);
   }


   printf("Processing tracefiles for %d processes\n",p);

#ifdef PRINT_STATS
   for(i=0;i<p;i++)for(j=0;j<MAX_ARRAYS;j++) for(k=0;k<MAX_EVENT_TYPES;k++){
     stat[i][j][k]=0.; 
     acc[i][j][k]=0;
   }
#endif
 
   if(!(clock_base = (unsigned long int *)malloc(p*sizeof(unsigned long int)))){
                     printf("couldn't allocate memory 1\n");
                     exit(2);
   }
   if(!(record = (int*)malloc(7*MR*sizeof(int)))){
                     printf("couldn't allocate memory 2\n");
                     exit(2);
   }
   if(!(times = (unsigned long int *)malloc(4*MR*sizeof(unsigned long int)))){
                     printf("couldn't allocate memory 3\n");
                     exit(2);
   }
   
   base = 0; tbase = times; events=0;
   for(i=0;i<p;i++){
      sprintf(finname,"%03d",i);
      fin = fopen(finname,"r");
      if(!fin){
          fprintf(stderr,"%s: File Not Found, Exiting ...\n",finname);
          exit(3);
      }

      for(k=0;k<7;k++) fscanf(fin,"%d",&flag);
      fscanf(fin,"%lu", clock_base+i);
      fscanf(fin,"%lu", clock_base+i);

      for(j=0;;j++){
          for(k=base;k<7+base;k++)fscanf(fin,"%d",(record+k)); 
          fscanf(fin,"%lu",times);
          if(feof(fin))break;

          /* transform GA handle to 0 .. MAX_ARRAYS-1 range */
          record[base+1] += GA_OFFSET;
          if(record[base+1] >= MAX_ARRAYS){
             fprintf(stderr, 
                    "array handle beyond range we can deal with %d > %d\n",
                     record[base+1], MAX_ARRAYS-1);
             exit(4);
          }

          times[0] -= clock_base[i]; 
          times[1] = base+1;  
          fscanf(fin,"%lu",times+2);

          times[2] -= clock_base[i];
#ifdef PRINT_STATS
          update(i,record+base,times[0],times[2]);
#endif
          times[3] = -base-1; 

          if(maxtime<times[2])maxtime=times[2]; /*find the end time */

          /* advance pointers only for the required array */
          if(record[base+1]==ga){
             times+=4; 
             base += 7;
             events++;
          }
      }
      fclose(fin);
   }
   
#ifdef PRINT_STATS
   printstat(p, maxtime);
#endif

   /* sorting events */
   
   times = tbase; 
/*   printf("\nsorting %d\n",2*events);*/
   qsort(tbase, 2*events, 2*sizeof(unsigned long int), tcomp);
   printf("%d events matching array handle %d processed\n",events,ga-GA_OFFSET);
   
   fout = fopen(foutname,"w");

   /* output processed events to the output file */
   fscanf(fdistr, "%d", &distrdata); fprintf(fout, "%d ", distrdata);
   fscanf(fdistr, "%d", &distrdata); fprintf(fout, "%d ", distrdata);
   fprintf(fout, "%d ", events*2);
   
   while((distrdata = getc(fdistr)) != EOF) putc(distrdata, fout);

   fclose(fdistr);
   
   /* print out the distribution first */
   
   for(i=0;i<events*2;i++){
      base = ABS((long int)times[1]); base -=1;
      flag = ((long int)times[1]) < 0 ? -1 : 1; 
      for(k=base;k<7+base;k++)fprintf(fout,"%d ",*(record+k)); 
      fprintf(fout,"%d ",flag);
      fprintf(fout,"%lu\n",times[0]);
      times += 2;
   }     
   
   fclose(fout);
}
   
      
static int tcomp(t1, t2)
unsigned long int *t1, *t2;
{
int flag;
    flag = (*t1 == *t2) ? 0 :(*t1 > *t2 ? 1 : -1);
    return (flag);
}


#ifdef PRINT_STATS
void update(proc,record, t0, t1)
long int proc;
int *record;
unsigned long t0,t1;
{
int ar=record[1], et=record[6];

  if(arrays < ar){
     arrays = ar;
     if(arrays>= MAX_ARRAYS){
       fprintf(stderr,"The program can handle only %d arrays\n",MAX_ARRAYS);
       exit(1);
     }
  }
  if(event_types < et){
     event_types = et;
     if (event_types >= MAX_EVENT_TYPES){
       fprintf(stderr,"The program can handle only %d event types%d\n",MAX_EVENT_TYPES,et);
       exit(1);
     }
  }
  stat[proc][ar][et] += 1e-6 * (double)(t1-t0);
  acc[proc][ar][et] ++;
}



void printstat(proc,tlast)
int proc;
unsigned tlast;
{
int p,e,a;
double t,ta,te, tl= 1e-6 * tlast;
double total_cm=0., total_cp=0.;
long int ia,  ie;

  printf("\t\t\tActivity Statistics\n");
  for(p=0;p<proc;p++){
     t=0.;  
     printf("Process %3d:\n~~~~~~~~~~~\n",p);
     /* event codes are in src/globalp.h */
     printf(" EventCode   NumberOfEvents   TotalEventTime   AverageTime\n");
     for(e=0; e<=event_types; e++){
        te =0.; ie=0;
        for(a=0;a<=arrays;a++){
           te += stat[p][a][e];
           ie += acc[p][a][e];
        }
        if(ie)printf(" %6d  %12d \t     %14g %14g\n",e,ie, te, te/ie);
        t += te;
     }
     printf("   Array     NumberOfEvents   TotalEventTime   AverageTime\n");
     for(a=0; a<=arrays; a++){
        ta =0.; ia=0;
        for(e=0; e<=event_types; e++){
           ta += stat[p][a][e];
           ia += acc[p][a][e];
        }
        if(ia)printf(" %6d  %12d \t     %14g %14g\n",a-GA_OFFSET,ia, ta, ta/ia);
     }
     total_cm += t; total_cp += MAX(tl-t,0.);
     printf(" Time in GAs:      %14g\n Time outside GAs: %14g\n\n",t,MAX(tl-t,0.));
  }

  printf("\n Total CPU time used [node-seconds]: \n");
  printf(" Time in GAs:      %14g\n Time outside GAs: %14g\n\n",total_cm,total_cp);
}

#endif
