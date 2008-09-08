#include <upc_strict.h>

struct G {
int curid;
  int last;
};

shared struct G* shared GS;
shared struct G* G;

int pid;
int main() {
  G = upc_alloc(sizeof(struct G));
  if (MYTHREAD == 0) GS = G;
  upc_barrier;

  G->last = G->curid = 0;
  G->last += pid;
  pid = GS->curid++; //broken
  pid = G->curid++;
  return 0;
}
