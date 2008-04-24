/* $Header$ */

/*
  This checks the functioning of the include file farg.h
*/

#include "farg.h"

void parg()
{
  int i;

  for (i=0; i<ARGC_; i++)
    (void) printf("argv(%d)=%s\n", i, ARGV_[i]);
}
