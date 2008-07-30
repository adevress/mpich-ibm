#ifndef upc_vectorize_INCLUDED
#define upc_vectorize_INCLUDED

#include <lnopt_main.h>
#include <ara_loop.h>
#include "ara.h"
#include "fiz_fuse.h"
#include "snl.h"
#include "snl_deps.h"
#include "snl_nest.h"
#include "snl_trans.h"
#include "snl_test.h"


extern int LNO_Upc_Vectorize;
extern ARA_LOOP_INFO *Get_Loop_Info(WN* func_nd);
extern void Upc_Vectorize(WN*);
extern FIZ_FUSE_INFO  *loops;




class REMOTE_REF_DESCR {
  
 public:
  ARA_LOOP_INFO *ali;
  int depth;
  ARA_REF * use;
  ARA_REF *def;
  BOOL deps;
  BOOL check_deps;
  REMOTE_REF_DESCR(ARA_REF *_use, ARA_REF *_def, ARA_LOOP_INFO *li, BOOL check, int level, MEM_POOL *mpool);
  REMOTE_REF_DESCR(): use(0), def(0), ali(0), deps(FALSE), depth(0) {};
  BOOL Analyze(DYN_ARRAY<REGION*> &processed_regs);
  void Do_Code_Gen(ST *ldesc, WN* prefix, WN* laddr);
  BOOL Check_Deps(ARRAY_DIRECTED_GRAPH16 *adg);
  ST  *Generate_New_Coeff(WN *bblock, WN *cinit, ST *ldesc, ST *rdesc, ST* lmad, 
			  int cur_pos, int num_dims, BOOL is_use);
  WN *Try_Static_Coeff(int cur_pos, int num_dim, BOOL is_use);

  DYN_ARRAY<WN*>  stride_use;
  DYN_ARRAY<WN*>  span_use;
  DYN_ARRAY<WN*>  base_use;
  

  DYN_ARRAY<WN*>  stride_def;
  DYN_ARRAY<WN*>  span_def;
  DYN_ARRAY<WN*>  base_def;
 
  STACK<WN*> *ind_vars;

  WN* Region_Span(REGION*, AXLE_NODE*, WN*, WN*);
  WN* Region_Base(REGION*, AXLE_NODE*, STACK<WN*> *);
  
};


class REMOTE_REF{

 public:
  ARA_LOOP_INFO *ali;
  int depth;
  DYN_ARRAY<REMOTE_REF_DESCR*> level;
  SYMBOL base;
  REMOTE_REF(ARA_LOOP_INFO *li, int _depth, SYMBOL &_base, MEM_POOL *mpool) :
    ali(li), depth(_depth), base(_base) { level.Set_Mem_Pool(mpool);};
  REMOTE_REF() {};
  BOOL Analyze();
  void Do_Code_Gen(ST *ldesc, WN *prefix, WN *laddr);
};


typedef DYN_ARRAY<REMOTE_REF*> REMOTE_REF_AR;

class VECT_INFO {
 public:
  REMOTE_REF_AR _refs; 
  ST *ldesc;
  int refs_vect;
  
  VECT_INFO(MEM_POOL *mpool) : ldesc(0), refs_vect(0) { _refs.Set_Mem_Pool(mpool);}
  //need a field to describe the conflicts
  REMOTE_REF_AR &Refs() { return _refs;};
  REMOTE_REF *Find_Ref(const SYMBOL&);
  
};

BOOL Can_Vectorize_on_Axle(ARA_LOOP_INFO *ai, REGION *reg, AXLE_NODE *a, 
			   BOOL &loop_is_decreasing, BOOL& reg_is_decreasing, 
			   WN**reg_step, WN **loop_step, int depth, int nest_depth, STACK<WN*> *ind_vars);
void Replace_Shared_Access(WN*, ST*, ST*, ST*, ST*);

#define REF_READ 1
#define REF_WRITE 2
extern WN * SNL_Vectorize_Loops(WN *wn, INT nloops, SNL_NEST_INFO *ni, 
				     SNL_ANAL_INFO *ai, 
				     BOOL *changed, BOOL nice = TRUE);
#endif /* upc_vectorize */
