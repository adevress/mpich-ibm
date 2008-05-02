/* $Header$ */

#include <stdlib.h>
extern char *strcpy();
extern size_t strlen();

char *strdup(s)
    char *s;
{
  char *new;

  if ((new = malloc((size_t) (strlen(s)+1))))
     (void) strcpy(new,s);

  return new;
}
