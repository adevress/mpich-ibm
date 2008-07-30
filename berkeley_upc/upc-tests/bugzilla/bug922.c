#ifdef __BERKELEY_UPC__
#pragma upc c_code
#endif

#include <stdio.h>
#include "bug899.h"

float MYTHREAD;

int foo(int shared) { return shared+1; }

int main(void)
{
  int shared;
  shared = foo(5);
  printf("SUCCESS\n");
  return 0;
}
