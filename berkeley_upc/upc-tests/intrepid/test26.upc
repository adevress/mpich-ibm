/*
UPC tests

Copyright (C) 2008
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

int shared a[100 * THREADS];

#define BLKSIZE 10
int shared[BLKSIZE] ablocked[100 * THREADS];

struct row
{
  int x, y, z;
};

struct row data[] = {
  {0, 0, 0}, {0, 99, 99}, {0, 10, 10}, {0, 11, 11},
  {23, 29, 84}, {15, 58, 19}, {81, 17, 48}, {15, 36, 49},
  {10, 63, 1}, {72, 10, 48}, {25, 67, 89}, {75, 72, 90},
  {92, 37, 89}, {77, 32, 19}, {99, 16, 70}, {50, 93, 71},
  {10, 20, 55}, {70, 7, 51}, {19, 27, 63}, {44, 3, 46},
  {91, 26, 89}, {22, 63, 57}, {33, 10, 50}, {56, 85, 4}
};

#define NDATA (sizeof(data)/sizeof(data[0]))

enum cmp_op
{ EQ_OP, FIRST_OP = EQ_OP,
  NE_OP, GT_OP, GE_OP, LT_OP,
  LE_OP, LAST_OP = LE_OP
};

void
test_phaseless ()
{
  int i;
  int shared *p0, *p1;
  for (i = 0; i < NDATA; ++i)
    {
      enum cmp_op op;
      int t0 = MYTHREAD;
      int t1 = (MYTHREAD + data[i].x) % THREADS;
      int j = data[i].y;
      int k = data[i].z;
      int ediff = (k - j);
      int tdiff = (t1 - t0);
      int diff = ediff * THREADS + tdiff;
      p0 = &a[(j * THREADS + t0) + j];
      p1 = &a[(k * THREADS + t1) + k];
      for (op = FIRST_OP; op <= LAST_OP; ++op)
	{
	  int expected, got;
	  const char *op_name;
	  switch (op)
	    {
	    case EQ_OP:
	      op_name = "=";
	      expected = (diff == 0);
	      got = (p0 == p1);
	      break;
	    case NE_OP:
	      op_name = "!=";
	      expected = (diff != 0);
	      got = (p0 != p1);
	      break;
	    case GT_OP:
	      op_name = ">";
	      expected = (diff > 0);
	      got = (p1 > p0);
	      break;
	    case GE_OP:
	      op_name = ">=";
	      expected = (diff >= 0);
	      got = (p1 >= p0);
	      break;
	    case LT_OP:
	      op_name = "<";
	      expected = (diff < 0);
	      got = (p1 < p0);
	      break;
	    case LE_OP:
	      op_name = "<=";
	      expected = (diff <= 0);
	      got = (p1 <= p0);
	      break;
	    default:
	      fprintf (stderr, "Error: unknown op: %d\n", (int) op);
	      abort ();
	    }
	  if (got != expected)
	    {
	      fprintf (stderr, "test26: Error: thread:%d phaseless PTS comparison %s failed.\n"
		       "        t0=%d t1=%d k=%d j=%d expected=%d got=%d\n",
		       MYTHREAD, op_name, t0, t1, k, j, expected, got);
	      abort ();
	    }
	}
    }
}

void
test_phased ()
{
  int i;
  int shared[BLKSIZE] *p0, *p1;
  for (i = 0; i < NDATA; ++i)
    {
      enum cmp_op op;
      int t0 = MYTHREAD;
      int t1 = (MYTHREAD + data[i].x) % THREADS;
      int j = data[i].y;
      int k = data[i].z;
      int ediff = (k - j);
      int pdiff = k % BLKSIZE - j % BLKSIZE;
      int tdiff = (t1 - t0);
      int diff = (ediff - pdiff) * THREADS + tdiff * BLKSIZE + pdiff;
      p0 = &ablocked[((j / BLKSIZE) * THREADS + t0) * BLKSIZE + (j % BLKSIZE)];
      p1 = &ablocked[((k / BLKSIZE) * THREADS + t1) * BLKSIZE + (k % BLKSIZE)];
      for (op = FIRST_OP; op <= LAST_OP; ++op)
	{
	  int expected, got;
	  const char *op_name;
	  switch (op)
	    {
	    case EQ_OP:
	      op_name = "=";
	      expected = (diff == 0);
	      got = (p0 == p1);
	      break;
	    case NE_OP:
	      op_name = "!=";
	      expected = (diff != 0);
	      got = (p0 != p1);
	      break;
	    case GT_OP:
	      op_name = ">";
	      expected = (diff > 0);
	      got = (p1 > p0);
	      break;
	    case GE_OP:
	      op_name = ">=";
	      expected = (diff >= 0);
	      got = (p1 >= p0);
	      break;
	    case LT_OP:
	      op_name = "<";
	      expected = (diff < 0);
	      got = (p1 < p0);
	      break;
	    case LE_OP:
	      op_name = "<=";
	      expected = (diff <= 0);
	      got = (p1 <= p0);
	      break;
	    default:
	      fprintf (stderr, "Error: unknown op: %d\n", (int) op);
	      abort ();
	    }
	  if (got != expected)
	    {
	      fprintf (stderr, "test26: Error: thread:%d phased PTS comparison %s failed.\n"
		       "        t0=%d t1=%d k=%d j=%d expected=%d got=%d\n",
		       MYTHREAD, op_name, t0, t1, k, j, expected, got);
	      abort ();
	    }
	}
    }
}

void
test26 ()
{
  test_phaseless ();
  upc_barrier;
  test_phased ();
}

int
main ()
{
  test26 ();
  upc_barrier;
  if (!MYTHREAD)
    {
      printf ("test26: test pointer-to-shared comparison - passed.\n");
    }
  return 0;
}
