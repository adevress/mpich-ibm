#include <string.h>
#include <alloca.h>

int main() {
  char *x = alloca(1);
  char *y;
  int z;
  memset(x,0,1);
  z = strlen(x);
  y = x;
  memcpy(x, y, 1); 
  z = memcmp(x,y,1);
  return 0;
}
