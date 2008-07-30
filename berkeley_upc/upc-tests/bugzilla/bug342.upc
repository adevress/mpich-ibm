#include <upc.h>

/* When bug is "live" this include (linux+pthreads) is sufficient */
#include <stdlib.h>

/* This is the type of code that causes the warning */
typedef union
{
  int *p;
  double d;
} foo_t __attribute__ ((__transparent_union__));
