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

shared unsigned char x1;
shared short x2;
shared int x3;
shared long long x4;
shared float x5;
shared double x6;

void
test00()
{
  
  if (MYTHREAD == 0)
    {
      x1 = 255;
      x2 = -2;
      x3 = -3;
      x4 = -4;
      x5 = -5.0;
      x6 = -6.0;
    }
 
  upc_barrier;
  if (x1 != 255) {
    printf("%d: Error %s : %d = 255\n", MYTHREAD, "char", x1);
    upc_global_exit(1);
  }
  if (x2 != -2) {
    printf("%d: Error %s : %d = -2\n", MYTHREAD, "short", x2);
    upc_global_exit(1);
  }
  if (x3 != -3) {
    printf("%d: Error %s : %d = -3\n", MYTHREAD, "int", x3);
    upc_global_exit(1);
  }
  if (x4 != -4) {
    printf("%d: Error %s : %lld = -4\n", MYTHREAD, "long long", x4);
    upc_global_exit(1);
  }

  if (x5 != -5.0) {
    printf("%d: Error %s : %f = -5.0\n", MYTHREAD, "float", x5);
    upc_global_exit(1);
  }
  
  
  if (x6 != -6.0) {
    printf("%d: Error %s : %f = -6.0\n", MYTHREAD, "double", x6);
    upc_global_exit(1);
  }
  upc_barrier;
  if (MYTHREAD == 0)
    {
      printf ("test00: access shared scalars - passed.\n");
    }
}

int
main()
{
  test00 ();
  return 0;
}
