#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <upc.h>

double x = 1.0001;

int main() {
clock_t init = clock();
bupc_tick_t inittick = bupc_ticks_now();
printf("initial=%llu\n",(unsigned long long)init);
 for (int i=0; i < 5; i++) {
   { /* delay loop of CPU busy time */
     bupc_tick_t start = bupc_ticks_now();
     while (bupc_ticks_to_us(bupc_ticks_now()-start) < 1000000) {
       for (int i=0; i < 1000; i++) {
         x *= 1.0001;
       }
     }
   }
   {
   double tickt = bupc_ticks_to_us(bupc_ticks_now()-inittick)/1000000.0;
   double clockt = (clock()-init)/(double)CLOCKS_PER_SEC;
   printf("tick_t: %.3f  clock: %.3f\n", tickt, clockt);
   if (fabs(tickt - clockt) > 2) 
      printf("ERROR: clock() and bupc_ticks_now() do not agree - (heavily loaded system?)\n");
   fflush(NULL);
   }
 }
 printf("done.\n");
}
