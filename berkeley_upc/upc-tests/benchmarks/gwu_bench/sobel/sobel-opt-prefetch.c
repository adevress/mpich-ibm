/* sobel-opt-prefetch.c
 *
 * Sobel function with remote data prefetching in local-shared memory (that's
 * why `localrow' is declared as a pointer to shared data).
 * (should be included in sobel.c)
 *
 * $Id: sobel-opt-prefetch.c,v 1.3 2005/04/05 22:14:34 bonachea Exp $
 */

#define STRUCTSIZE 6
typedef struct { BYTET a[STRUCTSIZE]; } cpstruct;

#define LOCALROWSZ (IMGSIZE+STRUCTSIZE)
  /* DOB: extra STRUCTSIZE bytes prevents an overflow 
   *    * when IMGSIZE is not a multiple of STRUCTSIZE */

#ifdef STATIC_ALLOC
shared[LOCALROWSZ] BYTET localrow[THREADS][LOCALROWSZ];
#else
shared[] BYTET* shared localrow[THREADS];	/* zero init (ANSI C) */
#endif
 
int Sobel(void)
{
  int i,j,d1,d2;
  double magnitude;
 
  shared BYTET* p_orig0;
  shared BYTET* p_orig1;
  shared BYTET* p_orig2;
  shared BYTET* p_edge;

#ifndef STATIC_ALLOC
	if(localrow[MYTHREAD]==NULL)
		localrow[MYTHREAD] = (shared[] BYTET*)upc_local_alloc(LOCALROWSZ, sizeof(BYTET));
#endif

  upc_forall(i=1; i<IMGSIZE-1; i++; &edge[i])
  {
    p_edge=(shared BYTET*)edge[i].r;
    p_orig1=(shared BYTET*)orig[i].r;

		/* At the beginning of a block, prefetch the previous line (which has
		 * affinity with MYTHREAD-1) into local-shared memory by copying structures.
		 */
    if (FIRST_ROW(i))
    { shared [] cpstruct *pss = (shared void*) orig[PREV_ROW(i)].r;
      shared [] cpstruct *psp = (shared void*)localrow[MYTHREAD];

      for(j=0; j<IMGSIZE; j+=STRUCTSIZE) *(psp++) = *(pss++);

      p_orig0=(shared BYTET*)localrow[MYTHREAD];
    }
    else
      p_orig0=(shared BYTET*)orig[PREV_ROW(i)].r;

		/* At the end of a block, prefetch the next line (which has
		 * affinity with MYTHREAD+1) into local-shared memory.
		 */
    if (LAST_ROW(i))
    { shared [] cpstruct *pss = (shared void*) orig[NEXT_ROW(i)].r;
      shared [] cpstruct *psp = (shared void*)localrow[MYTHREAD];

      for(j=0; j<IMGSIZE; j+=STRUCTSIZE) *(psp++) = *(pss++);

      p_orig2=(shared BYTET*)localrow[MYTHREAD];
    }
    else
      p_orig2=(shared BYTET*)orig[NEXT_ROW(i)].r;

    for (j=1; j<IMGSIZE-1; j++) {   
      d1=(int)   p_orig0[NEXT_ROW(j)]-p_orig0[PREV_ROW(j)];
      d1+=((int) p_orig1[NEXT_ROW(j)]-p_orig1[PREV_ROW(j)])<<1;
      d1+=(int)  p_orig2[NEXT_ROW(j)]-p_orig2[PREV_ROW(j)];

      d2=(int)   p_orig0[PREV_ROW(j)]-p_orig2[PREV_ROW(j)];
      d2+=((int) p_orig0[j]  -p_orig2[j]  )<<1;
      d2+=(int)  p_orig0[NEXT_ROW(j)]-p_orig2[NEXT_ROW(j)];

      magnitude=sqrt((double)(d1*d1+d2*d2));
      p_edge[j]=magnitude>255? 255:(unsigned char)magnitude;
    }
  }
  return 0;
}

/* vi:ts=2:ai
 */
