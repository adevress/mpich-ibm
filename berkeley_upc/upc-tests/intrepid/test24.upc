/*
UPC tests

Copyright (C) 2004 
Written by Gary Funck <gary@intrepid.com>
and Nenad Vukicevic <nenad@intrepid.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include <upc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef min
#define min(x,y) (((x) < (y)) ? (x) : (y))
#undef max
#define max(x,y) (((x) > (y)) ? (x) : (y))
#undef abs
#define abs(x) (((x) > 0) ? (x) : -(x))
#undef ceil
#define ceil(x, r) (((x) + (r) - 1)/(r))
#undef round_up
#define round_up(x, r) (ceil(x,r)*(r))

/* The following sequence of numbers, when interpreted as
   a plus number meaning allocation and a minus meaning
   de-allocation will eventually allocate roughly 54 megabytes,
   where approximately 1/3 of the sequence will free memory
   and 2/3's will allocate memory. */
const int alloc_seq[] = {
    39603, 76, 3571, 103682, 123, 199, 4870847, -39603, 29, -4870847,
    -3571, 7, -103682, -7, 15127, 1149851, 11, 1364, 3010349, -199,
    4870847, 20633239, 9349, -3010349, 3, 24476, 199, 4, -76, -15127,
    -199, 76, -9349, -20633239, 3010349, 47, 64079, 843, 15127, 7,
    -47, -3010349, 47, -47, 199, 103682, 9349, 710647, 3010349, 521,
    39603, -15127, -843, 2207, 271443, 20633239, 18, -4, 3571, 15127,
    -3571, -64079, 167761, 64079, 12752043, 1860498, -271443, 843,
    7881196, 3571, 4, -24476, 24476, 5778, 271443, 47, 439204, 322};
#define n_alloc_seq (sizeof (alloc_seq)/sizeof(int))

typedef struct alloc_node_s {
  struct alloc_node_s *next;
  struct alloc_node_s *prev;
  shared void *ptr;
  size_t size;
} alloc_t;
typedef alloc_t *alloc_p;

alloc_t alloc_list;

shared [] char * shared local_alloc[THREADS];

/* Return a character code that hashes thread number 't'
   and size 'size'.  */
int
char_code (int t, size_t size)
{
  int c = 'A' + (size * 8 + t % 8) % 26;
  return c;
}

/* Add entry for (p, size) into the allocated list.  */
void
add_to_alloc_list (shared void *p, size_t size)
{
  alloc_p last = alloc_list.prev;
  alloc_p a = malloc (sizeof (alloc_t));
  if (!a)
    { perror ("malloc"); abort (); }
  a->prev = last;
  a->next = &alloc_list;
  a->ptr = p;
  a->size = size;
  last->next = a;
  alloc_list.prev = a;
}

/* Return the first entry in the allocated list with
   a size matching the 'size' parameter, and delete
   it from the allocated list.  */
alloc_p
remove_from_alloc_list (size_t size)
{
  alloc_p last = alloc_list.prev;
  alloc_p a;
  for (a = alloc_list.next; (a != last) && (a->size != size);
       a = a->next) /* loop */;
  if (a->size != size)
    return NULL;
  a->prev->next = a->next;
  a->next->prev = a->prev;
  return a;
}

void
test24()
{
  const int nxt_thread = (MYTHREAD + 1) % THREADS;
  int i, pass;
  size_t max_alloc;
  char *buf; 
  alloc_t alloc_list_init = {&alloc_list, &alloc_list, 0, 0};
  alloc_list = alloc_list_init;
  for (i = 0, max_alloc = 0; i < n_alloc_seq; ++i)
    max_alloc = max (max_alloc, abs(alloc_seq[i]));
  max_alloc += 1;  /* add 1 for null terminator */
  buf = (char *) malloc (max_alloc);
  if (!buf)
    { perror ("malloc"); abort (); }
  /* Global allocation test */
#ifdef DEBUG
  if (MYTHREAD == 0)
    printf ("--- Global Allocation Test ---\n");
#endif /* DEBUG */
  upc_barrier 1;
  for (pass = 1; pass <= 2; ++pass)
    {
      int de_alloc_pass = (pass == 2);
      for (i = 0; i < n_alloc_seq; ++i)
	{
	  size_t size = abs (alloc_seq[i]) + 1;  /* add 1 for null char. */
	  int is_alloc = (alloc_seq[i] >= 0) ^ de_alloc_pass;
	  if (is_alloc)
	    {
	      int c = char_code (nxt_thread, size);
	      shared char *ptr;
	      shared [] char *s;
	      ptr = upc_all_alloc (THREADS, size);
	      if (!ptr)
	        {
		  fprintf (stderr, "%d: Error: upc_all_alloc() failed\n",
		           MYTHREAD);
		  abort ();
		}
	      s = (shared [] char *)&ptr[nxt_thread];
#ifdef DEBUG
	      printf ("%d: allocate %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	      /* set the data for the next thread */
	      upc_memset (s, c, size-1);
	      s[size-1] = '\0';
	      add_to_alloc_list (ptr, size);
	    }
          else
	    {
	      int c = char_code (MYTHREAD, size);
	      alloc_p a = remove_from_alloc_list (size);
	      shared char *ptr;
	      char *s;
	      if (!a)
	        { fprintf (stderr,
		        "%d: Error: can't find alloc list entry for size %ld\n",
		        MYTHREAD, (long int)size);
		  abort ();
	        }
	      memset (buf, c, size - 1);
	      buf[size-1] = '\0';
	      ptr = a->ptr;
#ifdef DEBUG
	      printf ("%d: check %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	      /* shared -> local */
	      s = (char *)&ptr[MYTHREAD];
	      if (strcmp (s, buf))
	        {
		  fprintf (stderr,
		    "%d: Error: global alloc pass %d:"
		    " data comparison failed for size %ld\n",
                    MYTHREAD, pass, (long int)size);
		  abort ();
		}
	      free (a);
	      upc_barrier 2;
	      /* Only one thread can free the data.  Choose a
	         a unique thread based upon the size of the data.  */
	      if (MYTHREAD == (size % THREADS))
	        {
#ifdef DEBUG
	          printf ("%d: free %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	          upc_free (ptr);
	        }
	    }
	  upc_barrier 3;
	}
    }
  /* Local allocation test */
#ifdef DEBUG
  if (MYTHREAD == 0)
    printf ("--- Local Allocation Test ---\n");
#endif /* DEBUG */
  upc_barrier 4;
  for (pass = 1; pass <= 2; ++pass)
    {
      int de_alloc_pass = (pass == 2);
      for (i = 0; i < n_alloc_seq; ++i)
	{
	  size_t size = abs (alloc_seq[i]) + 1;  /* add 1 for null char. */
	  int is_alloc = (alloc_seq[i] >= 0) ^ de_alloc_pass;
	  if (is_alloc)
	    {
	      int c = char_code (nxt_thread, size);
	      shared [] char *s;
	      s = upc_alloc (size);
	      if (!s)
	        {
		  fprintf (stderr, "%d: Error: upc_all_alloc() failed\n",
		           MYTHREAD);
		  abort ();
		}
#ifdef DEBUG
	      printf ("%d: allocate %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	      add_to_alloc_list (s, size);
              local_alloc[MYTHREAD] = s;
	      upc_barrier 5;
	      /* set the data for the next thread */
              s = local_alloc[nxt_thread];
	      upc_memset (s, c, size-1);
	      s[size-1] = '\0';
	    }
          else
	    {
	      int c = char_code (MYTHREAD, size);
	      alloc_p a = remove_from_alloc_list (size);
	      shared [] char *ptr;
	      char *s;
	      int t_free;
	      if (!a)
	        { fprintf (stderr,
		        "%d: Error: can't find alloc list entry for size %ld\n",
		        MYTHREAD, (long int)size);
		  abort ();
	        }
	      ptr = a->ptr;
	      memset (buf, c, size - 1);
	      buf[size-1] = '\0';
#ifdef DEBUG
	      printf ("%d: check %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	      /* shared -> local */
	      s = (char *)ptr;
	      if (strcmp (s, buf))
	        {
		  fprintf (stderr,
		    "%d: Error: local alloc pass %d:"
		    " data comparison failed for size %ld\n",
                    MYTHREAD, pass, (long int)size);
		  abort ();
		}
	      free (a);
	      local_alloc[MYTHREAD] = ptr;
	      upc_barrier 6;
	      /* differing threads, based upon the size of the data,
	         will free the locally allocated data of other
		 threads (possibly their own data).  */
	      t_free = (MYTHREAD + size) % THREADS;
	      ptr = local_alloc[t_free];
#ifdef DEBUG
              printf ("%d: free %ld\n", MYTHREAD, (long int)size);
#endif /* DEBUG */
	      upc_free (ptr);
	    }
	  upc_barrier 7;
	}
    }
  upc_barrier 8;
  if (MYTHREAD == 0)
    printf ("test24: global/local allocation test - passed\n");
}

int
main()
{
  test24 ();
  return 0;
}
