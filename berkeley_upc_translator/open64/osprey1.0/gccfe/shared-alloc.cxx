#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#if defined(__MACH__) && defined(__APPLE__)
#else
#include <values.h>
#endif
#include "defs.h"
#include "errors.h"
#include "gnu_config.h"

extern "C" {
#include "gnu/flags.h"
#include "gnu/system.h"
#include "gnu/tree.h"
#include "gnu/toplev.h"
#include "gnu/upc-act.h"
}

#include "glob.h"
#include "symtab.h"
#include "strtab.h"
#include "tree_symtab.h"
#include "wn.h"
#include "wfe_expr.h"
#include "wfe_misc.h"
#include "wfe_dst.h"
#include "ir_reader.h"
#include <cmplrs/rcodes.h>
#include "upc_mangle_name.h"

//frorm toplev.c
extern unsigned int pshared_size, shared_size;

#ifdef __alpha
//wei: on Tru64 long is the same as long long,
//and the system only provides atol
#define atoll atol 
#endif

#include "cxx_memory.h"

#define NAME_LEN 256
#define INIT_LEN 1000
#define TYPE_LEN 64
#define LINE_LEN 2048

#ifndef min
#define min(a,b) (a > b) ? b : a
#endif

class Decl {
public:
  bool is_static;
  string name;
  string orig_type; // original type of the decl
  UINT64 size;
  int kind;
  UINT64 esize;
  //a blkSize of 0 means indefinite blk size
  UINT blkSize; 
  string initExp;
  string dimExp;
  int* initDim;

  Decl(string name, UINT64 size, UINT blkSize, UINT64 esize, char kind, string init, string orig_type, string dim);
  void print();
  bool is_pshared();
  bool hasInit();
  UINT64 get_bsize();
  UINT64 get_num_blk();
  unsigned int get_num_dim();
  string get_next_dimlen();

private:
  int curPos; // current position in the dimension expression
  void get_init_dim();
};

vector<Decl*> file_vars;

string file_token = "";
string decls = "";
string static_alloc = "";

//declare the temporary variables used in initialization at the top of the init function,
//to work with non C99-compliant C compilers
string static_init_decls = "";

string static_init = "";
string TLD_init = "";
string shared = "", pshared = "";

//list of upc runtime functions
const char* SHARED = "upcr_shared_ptr_t ";
const char* PSHARED = "upcr_pshared_ptr_t ";
const char* ISNULL = "upcr_isnull_shared";
const char* ISNULL_P = "upcr_isnull_pshared";
const char* S_TO_P = "upcr_shared_to_pshared";
const char* P_TO_S = "upcr_pshared_to_shared";
const char* S_TO_L = "upcr_shared_to_local";
const char* P_TO_L = "upcr_pshared_to_local";
const char* PUT_P = "upcr_put_pshared";
const char* PUT_S = "upcr_put_shared";
const char* INIT_ARRAY = "upcr_startup_initarray";
const char* PINIT_ARRAY = "upcr_startup_initparray";
const char* SHALLOC = "upcr_startup_shalloc";
const char* PSHALLOC = "upcr_startup_pshalloc";
const char* SHALLOC_T = "upcr_startup_shalloc_t";
const char* PSHALLOC_T = "upcr_startup_pshalloc_t";
const char* MY = "upcr_mythread";
const char* INIT = "UPCR_INITIALIZED_SHARED";
const char* INIT_P = "UPCR_INITIALIZED_PSHARED";
const char* TLD_DECL = "UPCR_TLD_DEFINE";
const char* TLD_ADDR = "UPCR_TLD_ADDR";


//soem helper functions
string utoa(UINT64 i) {

  string val = "";
  while (i >= 10) {
    val = (char) ((i % 10) + '0') + val;  
    i = i / 10;
  }
  return ((char) (i + '0')) + val;
}
 
string itoa(INT64 i) {
  
  if (i < 0) {
    return "-" + utoa(-i);
  } else {
    return utoa(i);
  }
}

//Convert name to a legal C identifier,
//by stripping off .c in end of filename, as well as any leading /
//If id == true, this also converts the string into a legal C identifier(letters+digits)
string strip(string name, bool id = false) {

  string ans;
  int index = name.find_last_of(".");
  ans = (index > 0) ? name.substr(0, index) : name;
  index = ans.find_last_of("/");
  ans = (index > 0) ? ans.substr(index+1, ans.size() - index - 1) : ans;
  if (id) {
    if (!isalpha(ans[0])) {
      //convert first character to a letter
      ans[0] = '_';
    }
    for (int i = 1; i < ans.size(); i++) {
      if (!isalnum(ans[i])) {
	ans[i] = '_';
      }
    }
  }
  return ans;
}

static string remove_addrof(string s) {
  if (s[0] == '&') {
    return s.substr(1, s.size() - 1);
  } 
  return s;
}

extern char* get_type(int mtype);

//generate the type string for the given ty entry
//For arrays, the base(non-array) type is printed
static string print_type(TY_IDX ty) {

  string result = "";

  switch (TY_kind(ty)) {
  case KIND_SCALAR:
    result = get_type(TY_mtype(ty));
    break;
  case KIND_ARRAY: 
    result = print_type(TY_etype(ty));
    break;
  case KIND_STRUCT: 
    result = TY_is_union(ty) ? "union " : "struct ";
    result += (string) TY_name(ty);
    for (int i = result.find("."); i >= 0; i = result.find(".")) { 
      result = result.replace(i, 1, "");
    }    
    break;
  case KIND_POINTER:
    if (TY_kind(TY_pointed(ty)) == KIND_VOID) {
      //treat "void *" as a special case
      result = "VOID*";
    } else {
      //we don't care about the type of a pointer
    }
    break;
  default:
    FmtAssert(false, ("Invalid kind of types in print_type"));
  }
  return result;
}


static void init_shared_decl_str() {

  static bool init = false;
  if (!init) {
    file_token = strip(Orig_Src_File_Name, true) + "_" + utoa(string_hash(Orig_Src_File_Name));

    //declarations the allocation and initialization function for shared data
    static_alloc += "void UPCRI_ALLOC_" + file_token + "() { \n";
    static_alloc += "UPCR_BEGIN_FUNCTION();\n";
    static_init_decls += "void UPCRI_INIT_" + file_token + "() { \n";
    static_init_decls += "UPCR_BEGIN_FUNCTION();\n";
    
    static_init += "UPCR_SET_SRCPOS(\"_" + file_token + "_INIT\",0);\n";

    //output the internally used symbol "upc_forall_control"
    // DOB: now handled in upcr_trans_extra.w2c.h
    //decls += "extern int upcr_forall_control;\n";
    decls += "#define UPCR_SHARED_SIZE_ " + utoa(shared_size) + "\n";
    decls += "#define UPCR_PSHARED_SIZE_ " + utoa(pshared_size) + "\n";
    init = true;
  }
}

//For shared arrays, thread_dim marks the dimension that contains 
//the THREADS (0 means THREADS does not appear in the declaration)
//The value is currently not used as we don't support static initializer
//for shared arrays
void process_shared(ST_IDX st, int thread_dim, string init_exp) {

  //fprintf(stderr, "%s: %s\n", ST_name(st), init_exp.c_str());

  init_shared_decl_str();
  TY_IDX ty = ST_type(st);
  FmtAssert(TY_is_shared(ty), ("Non-share symbol in process_shared"));
  UINT64 size  = get_real_size(ty);
  UINT64 eltSize = (TY_kind(ty) == KIND_ARRAY) ? get_real_size(Get_Inner_Array_Type(ty)) : size;
  UINT64 bsize = Get_Type_Block_Size(ty); //the number of bytes per block 
  if (TY_kind(ty) != KIND_ARRAY || bsize == 0) {
    //all non-array variables only reside on node 0
    bsize = size; 
  } else {
    bsize = min(bsize * eltSize, size);
  }

  UINT64 numBlk = (size % bsize == 0) ? (size / bsize) : size / bsize + 1; 
  string base_type = print_type (ty);
  string name = (string) ST_name(st);
  UINT64 num_dim; //for array only
  for (num_dim = 0; init_exp[num_dim] == '['; num_dim++);

  //output the declaration
  if(Debug_Level)
    decls += "__BMN_"+Mangle_Type(ty) + " "; 
  else
    decls += TY_is_pshared(ty) ? PSHARED : SHARED;
  decls += name;
  if (init_exp != "NONE") {
    decls += " = ";
    if (TY_kind(ty) == KIND_POINTER && init_exp == "0") {
      decls += TY_is_pshared(ty) ? "UPCR_NULL_PSHARED" : "UPCR_NULL_SHARED";
    } else {
      decls += TY_is_pshared(ty) ? "UPCR_INITIALIZED_PSHARED" : "UPCR_INITIALIZED_SHARED";
    }  
  }
  decls += ";\n";
  
  //Output the allocation code 
  string flag = "0";
  if (TY_kind(ty) == KIND_ARRAY && threads_int == 0 && Get_Type_Block_Size(ty) != 0) {
    //need to mult by THREADS
    flag = "1";
  }
  if (TY_is_pshared(ty)) {
    pshared += "UPCRT_STARTUP_PSHALLOC(" + name + ", ";
    pshared += utoa(bsize);
    pshared +=  ", " + utoa(numBlk) + ", ";
    pshared += flag + ", ";
    pshared += utoa(eltSize) + ", ";
    pshared += "\"" + Mangle_Type(ty) + "\"), \n";
  } else {
    shared += "UPCRT_STARTUP_SHALLOC(" + name + ", " + utoa(bsize) + ", " +
      utoa(numBlk) + ", ";
    shared += flag + ", ";
    shared += utoa(eltSize) + ", ";
    shared += "\"" + Mangle_Type(ty) + "\"), \n";
  }

  //Output the initialzation code
  if (init_exp != "NONE") {
    switch (TY_kind(ty)) {
    case KIND_STRUCT:
    case KIND_SCALAR:
      //static_init += base_type + " " + name + "_val = " + init_exp + ";\n";
      static_init_decls += base_type + " " + name + "_val = " + init_exp + ";\n";
      break;
    case KIND_POINTER:
      break;
    case KIND_ARRAY:
      break;
      /*
      static_init += base_type + " " + name + "_val";
      for (int i = 0; i < num_dim; i++) {
        static_init += "[" + utoa(init_dim[i]) + "]";
      }
      static_init += " = " + init_exp + ";\n"; 
      static_init += "upcr_startup_arrayinit_diminfo_t ";
      static_init += name + "_diminfos[] = {\n";
      for (int i = 0; i < num_dim; i++) {
	string dim_len = d->get_next_dimlen();
	bool has_thread = false;
	if (dim_len[0] == 'T') {
	  dim_len = dim_len.substr(1, dim_len.size() - 1);
	  has_thread = true;
	}
	static_init += "{" + utoa(d->initDim[i]) + ", " + dim_len + ", " + (has_thread ? "1" : "0") + "},\n";
      static_init += "};\n";
      break;
      */
    }  

    switch (TY_kind(ty)) {
    case KIND_POINTER:
      if (init_exp == "0") {
	break; //Pointer is already initialized to null during declaration
      }
      //fall through
    case KIND_SCALAR:
    case KIND_STRUCT:
      //ouput something like upcr_put_shared(ptr, 0, ptr_val, size)
      static_init += "if (" + (string) MY + "()== 0) { \n";
      static_init += ((string) (TY_is_pshared(ty) ? PUT_P : PUT_S)) + "(" + name;
      static_init += ", 0"; 
      if (TY_kind(ty) == KIND_POINTER) {
	static_init += ", &" + init_exp;
      } else {
	static_init += ", &" + name + "_val"; 
      }
      static_init += ", " + utoa(size) + ");\n";
      static_init += "}\n";
      break;
    case KIND_ARRAY: 
      static_init += ((string) (TY_is_pshared(ty) ? PINIT_ARRAY : INIT_ARRAY)) + "(";
      static_init += name + ", ";
      static_init += name + "_val, ";      
      static_init += name + "_diminfos, ";
      static_init += utoa(num_dim) + ", ";
      static_init += utoa(eltSize) + ", ";
      static_init += utoa(Get_Type_Block_Size(ty)) + ");\n";
      break;
    }
  }
}

void finish_shared() {

  init_shared_decl_str(); // in case there's no shared symbol in the code
  if (shared != "") {
    static_alloc += (string) SHALLOC_T + " info[] = { \n";
    static_alloc += shared;
    static_alloc += " };\n";
  }
  if (pshared != "") {
    static_alloc += (string) PSHALLOC_T + " pinfo[] = { \n";
    static_alloc += pshared;
    static_alloc += " };\n";
  }

  static_alloc += "\n";
  static_alloc += "UPCR_SET_SRCPOS(\"_" + file_token + "_ALLOC\",0);\n";

  if (shared != "") {
    static_alloc += (string) SHALLOC + "(info, ";
    static_alloc += "sizeof(info) / sizeof(" + (string) SHALLOC_T + "));\n";
  }
  if (pshared != "") {
    static_alloc += (string) PSHALLOC + "(pinfo, ";
    static_alloc += "sizeof(pinfo) / sizeof(" + (string) PSHALLOC_T + "));\n";
  }
  decls += "\n";
}

static string get_qualifier(TY_IDX ty) {

  string type_str = "";
  if (TY_is_const(ty)) {
    type_str += "const ";
  }
  if (TY_is_volatile(ty)) {
    type_str += "volatile ";
  }
  if (TY_is_restrict(ty)) {
    type_str += "UPCR_RESTRICT ";
  }
  return type_str;
}

//reconstruct the equivalent C type declaration for the given ty_idx
//The translation is straightforward except for function pointers and arrays,
//where we could not simply place the type string in front of the variable name
//Instead, we generate a typedef for these two special cases, and use
//the new type to declare the variable.
// e.g., int a[2][3] becomes in the output
//
// typedef int _type_a[2][3];
// _type_a TLD_DEFINE_TENTATIVE(a, sizeof(a), alignof(a));
//
//See the UPCR documentation for more details
static string getTypeStr(TY_IDX idx, string name) {

  string type_str;
  string typedef_str = "typedef ";
  
  switch (TY_kind(idx)) {
  case KIND_VOID:
    type_str = get_qualifier(idx) + "void";
    break;
  case KIND_SCALAR: 
    if (TY_mtype(idx) < MTYPE_F4 && TY_is_logical(idx)) {
      /* A known C integral type */
      type_str = TY_name(idx);
    } else {
      type_str = get_type(TY_mtype(idx));
    }
    type_str = get_qualifier(idx) + type_str;
    break;
  case KIND_POINTER: {
    TY_IDX pointed = TY_pointed(idx);
    if (TY_is_shared(pointed)) {
      if(Debug_Level) {
	type_str = "__BMN_" + Mangle_Type(idx);
      } else 
	if (TY_is_pshared(pointed)) {
	  type_str = "upcr_pshared_ptr_t";
	} else {
	  type_str = "upcr_shared_ptr_t";
	}
    } else if (TY_kind(pointed) == KIND_FUNCTION) {
      //function pointers need an extra level of indirection (with a typedef)
      TYLIST_IDX params = TY_parms(pointed);
      typedef_str += getTypeStr(TY_ret_type(pointed), name);
      typedef_str += "(*";
      typedef_str += "_funptr_" + name;
      typedef_str += ")(";
      while (Tylist_Table[params] != 0) {
        typedef_str += getTypeStr(TYLIST_item(Tylist_Table[params]), name);
        params = TYLIST_next(params);
        if (Tylist_Table[params] != 0)
          typedef_str += ",";
      }
      typedef_str += ");\n";
      decls += typedef_str;   //output the function ptr typedef
      type_str = "_funptr_" + name;
    } else {
      type_str = getTypeStr(pointed, name) + "* ";
      type_str += get_qualifier(idx);
    }
    break;
  }
  case KIND_ARRAY: 
    typedef_str += getTypeStr(Get_Inner_Array_Type(idx), name);
    typedef_str += " _type_" + name;
    do {
      if (TY_size(idx) > 0) {
	UINT64 eltSize = TY_size(TY_etype(idx));
	UINT64 length = TY_size(idx) / eltSize;
	typedef_str += "[" + utoa(length) + "]";
      } else {
	typedef_str += "[]";
      }
      idx = TY_etype(idx);
    } while (TY_kind(idx) == KIND_ARRAY);
    decls += typedef_str + ";\n";  //output the array typedef
    //fprintf(stderr, "adding typedef: %s\n", typedef_str.c_str());
    type_str = "_type_" + name;
    break;
  case KIND_STRUCT: {
    char * name = TY_name(idx);
    if (strncmp(name, "T ", 2) == 0) {
      /* We have an anonymous struct, use its typedef name */
      type_str = (string) (name + 2);
    } else {
      type_str = (TY_is_union(idx) ? "union " : "struct ") + (string) name;
      //we need to remove the "." from the TY_name.  This should match with whatever WHIRL2C_make_valid_c_name            
      //returns as long as anonymous structs have names like ".anonymous.i"                                               
      for (int i = type_str.find("."); i >= 0; i = type_str.find(".")) {
        // for anonymous struct (defined via typedefs), we always output their definition           
        Set_TY_is_written(idx);
        type_str = type_str.replace(i, 1, "");
      }
      type_str = get_qualifier(idx) + type_str;
    }
    break;
  }
  default:
    FmtAssert(false, ("IN TREE_SYMTAB, getTypeStr:  UNEXPECTED TYPE"));
  }
    
  return type_str;
}


/**
 *
 * Handles TLD variables
 */
void process_nonshared(ST_IDX st, string init) {

  if (ST_keep_name_w2f(ST_ptr(st)) == 0) {
    return;  //do not output any definitions in a .h file
  }

  //fprintf(stderr, "TLD: %s: %s\n", ST_name(st), init.c_str());

  TY_IDX ty = ST_type(st);
  int kind = TY_kind(ty);
  string name = ST_name(st); 

  //Add a suffix for extern variables to avoid name conflict
  //(i.e., when a file contains both the extern declaration and the definition of a variable)
  string type = getTypeStr(ty, ST_sclass(st) == SCLASS_EXTERN ? name + "_e" : name);

  bool is_null_ptr = TY_kind(ty) == KIND_POINTER && init == "0";

  if (ST_sclass(st) == SCLASS_EXTERN) {
    decls += "extern ";
    if (TY_is_shared(ST_type(st))) {
      decls += TY_is_pshared(ST_type(st)) ? PSHARED : SHARED;
    } else {
      decls += type + " ";
    }
    decls += name;
  } else {
    if (TY_kind(ty) == KIND_POINTER && TY_is_shared(TY_pointed(ty))) {
      if(Debug_Level) 
	decls += "__BMN_"+Mangle_Type(ty) + " ";
      else {
	if (TY_is_pshared(TY_pointed(ty))) {
	  decls += PSHARED;
	} else {
	  decls += SHARED;
	}
      }
    } else {
      decls += type + " ";
    }

    decls += (init != "NONE" ? ((string) TLD_DECL) : ((string) TLD_DECL) + "_TENTATIVE") + "(";
    decls += name;
    UINT align = TY_align(ty);
    if(Type_Is_Shared_Ptr(ty) && TY_kind(ty) != KIND_SCALAR && TY_KIND(ty) != KIND_STRUCT) {
      align = TY_align(TY_To_Sptr_Idx(ty));
    }
    decls += ", " + utoa(get_real_size(ty)) + ", " + utoa(align) + ")";
    if (init != "NONE") {
      //go ahead and initialize 
      decls += " = ";
      TY_IDX ty_idx = ST_type(st);
      if (Type_Is_Shared_Ptr(ty_idx, true)) {
	if (is_null_ptr) {
	  decls += TY_is_pshared(TY_pointed(ty_idx)) ? "UPCR_NULL_PSHARED" : "UPCR_NULL_SHARED"; 
	} else {
	  decls += TY_is_pshared(TY_pointed(ty_idx)) ? "UPCR_INITIALIZED_PSHARED" : "UPCR_INITIALIZED_SHARED";
	}
      } else {
	decls += init;
      }
    }
  }
  decls += ";\n"; 

  //if it's a pointer that's initialized to the address of another global variable, need some extra work to initialize it
  if (TY_kind(ST_type(st)) == KIND_POINTER && init != "NONE" && !is_null_ptr) {
    init = remove_addrof(init); //get rid of the leading '&', if any
    TY_IDX ty_idx = TY_pointed(ST_type(st));

    TLD_init += "*((";
    if (TY_is_shared(ty_idx)) {
       TLD_init += TY_is_pshared(ty_idx) ? PSHARED : SHARED;
       TLD_init += "*) " + (string) TLD_ADDR;
       TLD_init += "(" + name + ")) = ";
       TLD_init += init;
    } else {
      TLD_init += type;
      TLD_init += "*) " + (string) TLD_ADDR;
      TLD_init += "(" + name + ")) = ";
      TLD_init += "(" + type + ")";
      if (init[0] == '\"') {
	TLD_init += init;
      } else {
	TLD_init += (string) TLD_ADDR + "(";
	TLD_init += init + ")";
      }

    }
    TLD_init += ";\n";
  }
}

void output_file() {

  finish_shared();
  string out_file = strip(Orig_Src_File_Name) + ".global_data.c";  
  FILE* out = fopen(out_file.c_str(), "w");

  /* add the #include */
  for (vector<string>::const_iterator iter=h_files.begin(); iter!=h_files.end(); iter++) {
    fputs("#include<", out);
    fputs((*iter).c_str(), out);
    fputs(">\n", out);    
  }

  /* separator for includes and declarations */
  fputs("########################\n", out);
   
  fputs(decls.c_str(), out); 
  fputs(static_alloc.c_str(), out);
  fputs("}\n\n", out);
  fputs(static_init_decls.c_str(), out);
  fputs(static_init.c_str(), out);
  fputs(TLD_init.c_str(), out);
  fputs("}\n", out);
}

