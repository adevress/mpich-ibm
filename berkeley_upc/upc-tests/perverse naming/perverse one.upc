#include <upc.h>
#include <num one.h>
#include <num two.h>
#include <num three.h>
#include <stdio.h>

int main() {
x = 1;
y = 2;
z = 3;
sx = 10;
sy = 10;
sz = 10;
upc_barrier 5;
printf("Hello from %i/%i\n",MYTHREAD,THREADS);
printf("%i",x+y+z+sx+sy+sz);
upc_barrier;
return 0;
}
