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

void
test14 ()
{
  int shared * res; 
  int *p;
  res = 0; 
  upc_barrier;
  if (MYTHREAD == 0)
    {
      if (res)
	abort ();
      if (res != 0)
	abort ();
      p = (int *)res;
      if (p)
	abort ();
      if ((int *) res != 0)
	abort ();
      if (0 != (int *) res)
	abort ();
      printf ("test14: test assignment, comparison, and conversion of a null shared pointer - passed.\n");
    }
}

int main (int argc, char ** argv) 
{ 
   test14 ();
   return 0;
}
