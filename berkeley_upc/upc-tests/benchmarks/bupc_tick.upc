#include <upc.h>

double x = 1.0001;
void compute_foo() {
 for (int i=0; i < 1000; i++) {
   x *= x;
 }
}

int main() {
  bupc_tick_t start = bupc_ticks_now();
  compute_foo(); /* do something that needs to be timed */
  bupc_tick_t end = bupc_ticks_now();
  
  printf("Time was: %d microseconds\n", (int)bupc_ticks_to_us(end-start));
  printf("Time was: %d nanoseconds\n",  (int)bupc_ticks_to_ns(end-start));
  printf("Timer granularity: <= %.3f us, overhead: ~ %.3f us\n",
       bupc_tick_granularityus(), bupc_tick_overheadus());
  printf("Estimated error: +- %.3f %%\n",
      100.0*(bupc_tick_granularityus()+bupc_tick_overheadus()) / 
            bupc_ticks_to_us(end-start));
  printf("done.\n");
}
