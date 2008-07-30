/*
UPC tests

Copyright (C) 2001 
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
#include <stdio.h>

#define FACTOR 100
shared int array[FACTOR*THREADS];

void
test01()
{
  int i;
  for (i = MYTHREAD; i < FACTOR*THREADS; i += THREADS)
    {
      array[i] = i+1;
    }
  upc_barrier;
  if (MYTHREAD == 0)
    {
      for (i = 0; i < FACTOR*THREADS; ++i)
	{
	  int got = array[i];
	  int expected = i+1;
	  if (got != expected)
	    {
	      printf("test01: error at element %d. Expected %d, got %d\n",
		i, expected, got);
	    
	      upc_global_exit(1);
	    }
	}
      printf ("test01: simple array test - passed.\n");
    }
  
}

int
main()
{
  test01 ();
  return 0;
}
