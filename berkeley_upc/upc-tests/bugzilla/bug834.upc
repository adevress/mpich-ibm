#include <stdio.h>
//#include <stdint.h>
#include <inttypes.h>
int main() {
    uint64_t tm = 0x010408104080;
    uint32_t seed = 0xFFFF0000;
    int i;
    for (i=0; i < 4; i++) {
      seed = (uint16_t)seed ^ ((uint16_t*)&tm)[i];
    }
    if (seed == 0x00004994) printf("done.\n");
    else printf("ERROR\n");
}

