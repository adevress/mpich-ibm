#include <stdlib.h> /* For NULL */
#include <fenv.h>
int foo (void) { return feholdexcept(NULL); }


