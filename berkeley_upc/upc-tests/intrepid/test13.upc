/*
UPC tests

Copyright (C) 2003 
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
#include <upc_strict.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* This test calculates the difference between two shared pointers.
   Further, it declares shared pointers in a stack local context. */

shared int vec[THREADS];

size_t
ptr_diff(shared int *p1, shared int *p2)
{
  return p1 - p2;
}

void
test13()
{
  size_t s1, s2;
  shared int *p_t, *p_0;
  p_t = &vec[THREADS - 1];
  p_0 = &vec[0];
  s1 = ptr_diff(p_t, p_0);
  s2 = p_t - p_0;
  if (s1 != (THREADS - 1))
    {
      fprintf (stderr, "Error in pointer difference,"
                       " got: %lld, expected: %lld.\n",
		       (long long)s1, (long long)(THREADS - 1));
      abort ();
    }
  if (s1 != s2)
    {
      fprintf (stderr, "Error in pointer difference,"
                       " s1: %lld not equal to s2: %lld.\n",
		       (long long)s1, (long long)s2);
      abort ();
    }
  upc_barrier;
  if (MYTHREAD == 0)
    {
      printf ("test13: test pointer difference and local declaration"
	      " of shared pointers - passed.\n");
    }
}

int
main ()
{
  test13 ();
  return 0;
}
