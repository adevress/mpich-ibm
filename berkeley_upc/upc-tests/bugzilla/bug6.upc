#include <upc.h>

shared [UPC_MAX_BLOCK_SIZE] int x[THREADS*(UPC_MAX_BLOCK_SIZE)]; /* ok */
shared [UPC_MAX_BLOCK_SIZE+1] int y[THREADS*(UPC_MAX_BLOCK_SIZE+1)]; /* error */
shared [*] int z[THREADS*(UPC_MAX_BLOCK_SIZE+1)]; /* error */
shared [1000000] int a[THREADS*1000000];
shared [2] int b[THREADS * 1000000];

int main() {}
