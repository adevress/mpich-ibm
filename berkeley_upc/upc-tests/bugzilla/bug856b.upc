#include <stdio.h>
#include <assert.h>

#define BLK 3

typedef int I;
typedef I J[10];
typedef shared [BLK] J K[10];
typedef K L[10];
K k;
L l;
shared [BLK] int k2[10][10];
shared [BLK] int l2[10][10][10];

int main() {
printf("upc_blocksizeof(K)=%i\n",(int)upc_blocksizeof(K));
printf("upc_blocksizeof(L)=%i\n",(int)upc_blocksizeof(L));
printf("upc_blocksizeof(k)=%i\n",(int)upc_blocksizeof(k));
printf("upc_blocksizeof(l)=%i\n",(int)upc_blocksizeof(l));
printf("upc_blocksizeof(k2)=%i\n",(int)upc_blocksizeof(k2));
printf("upc_blocksizeof(l2)=%i\n",(int)upc_blocksizeof(l2));
assert(upc_blocksizeof(K) == 3);
assert(upc_blocksizeof(L) == 3);
assert(upc_blocksizeof(k) == 3);
assert(upc_blocksizeof(l) == 3);
assert(upc_blocksizeof(k2) == 3);
assert(upc_blocksizeof(l2) == 3);
printf("done.\n");
return 0;
}
