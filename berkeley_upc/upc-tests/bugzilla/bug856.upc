#include <stdio.h>
#include <assert.h>
typedef int I;
typedef I J[10];
typedef shared [*] J K[10];
typedef K L[10];
K k;
L l;
shared [*] int k2[10][10];
shared [*] int l2[10][10][10];

int main() {
printf("upc_blocksizeof(K)=%i\n",(int)upc_blocksizeof(K));
printf("upc_blocksizeof(L)=%i\n",(int)upc_blocksizeof(L));
printf("upc_blocksizeof(k)=%i\n",(int)upc_blocksizeof(k));
printf("upc_blocksizeof(l)=%i\n",(int)upc_blocksizeof(l));
printf("upc_blocksizeof(k2)=%i\n",(int)upc_blocksizeof(k2));
printf("upc_blocksizeof(l2)=%i\n",(int)upc_blocksizeof(l2));
assert(upc_blocksizeof(k2) == 100/THREADS);
assert(upc_blocksizeof(l2) == 1000/THREADS);
assert(upc_blocksizeof(K) == 100/THREADS);
assert(upc_blocksizeof(L) == 1000/THREADS);
assert(upc_blocksizeof(k) == 100/THREADS);
assert(upc_blocksizeof(l) == 1000/THREADS);
printf("done.\n");
return 0;
}
