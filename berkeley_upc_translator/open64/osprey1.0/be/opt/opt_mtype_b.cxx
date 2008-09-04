//-*-c++-*-
// ====================================================================
// ====================================================================
//
// Module: opt_mtype_b.cxx
// $Revision: 1.2 $
// $Date: 2003/03/04 06:16:03 $
// $Author: wychen $
// $Source: /var/local/cvs/compilers/open64/osprey1.0/be/opt/opt_mtype_b.cxx,v $
//
// ====================================================================
//
// Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
//
// Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
// Mountain View, CA 94043, or:
//
// http://www.sgi.com
//
// For further information regarding this notice, see:
//
// http://oss.sgi.com/projects/GenInfo/NoticeExplan
//
// ====================================================================
//
// Description: Introduction of MTYPE_B into the code
//
// MTYPE_B is now generated by wopt for targets that provide a distinct
// set of predicate registers.  Phases before wopt will never need to
// handle MTYPE_B.  The purpose of using MTYPE_B is to encourage the
// use of predicate  registers to keep the results of comparisons.
// This avoids unnecessary moves of values from predicate registers to
// integer registers.  It also reduces the register pressure for
// integer registers.
//
// The result types of EQ, NE, LT, LE, GT, GE will be changed to
// MTYPE_B.  The result type of LNOT will be changed to MTYPE_B only if
// its operand is also of MTYPE_B.  An operation that has MTYPE_B as
// result type will later  be saved to MTYPE_B PREGs by wopt's PRE,
// which will become predicate registers in CG.  MTYPE_B values (which
// could come from a BBLDID of a preg) can be operated on directly by
// FALSEBR, TRUEBR, ASSERT and the first kid of SELECT.  The effect is
// that a CSE between two (i < j) that are operands of branches will
// only involve predicate registers.
//
// MTYPE_B cannot be operated on by any other operators like ADD or
// STID, since the hardware does not allow it.  wopt will generate
// I4BCVT, which will translate to a move of the value from predicate
// register to integer register, before other operations on them.  
//
// The new constructs generated are:
//
// - LT, LE, GT, GE of rtype MTYPE_B
// - EQ, NE, LNOT of rtype MTYPE_B and/or desc MTYPE_B
// - I4BCVT, I8BCVT, U4BCVT, U8BCVT
// - BBLDID preg
// - BSTID preg
// - BINTCONST
//
// ====================================================================
// ====================================================================

#ifdef USE_PCH
#include "opt_pch.h"
#endif // USE_PCH
#pragma hdrstop


#include "defs.h"
#include "errors.h"
#include "erglob.h"
#include "glob.h"	// for Cur_PU_Name
#include "mempool.h"
#include "tracing.h"	/* for TFile */
#include "cxx_memory.h"

#include "opt_defs.h"
#include "opt_cfg.h"
#include "opt_main.h"
#include "opt_htable.h"


class OPT_MTYPE_B {
private:
  CFG        *_cfg;             // the control flow graph
  CODEMAP    *_htable;          // the hash table

  CODEREP *Do_mtype_b_cr(CODEREP *cr);
  CODEREP *Create_BCVT(MTYPE to_type, CODEREP *x);

  OPT_MTYPE_B(void);               // REQUIRED UNDEFINED UNWANTED methods
  OPT_MTYPE_B(const OPT_MTYPE_B&); // REQUIRED UNDEFINED UNWANTED methods
  OPT_MTYPE_B& operator = (const OPT_MTYPE_B&); // REQUIRED UNDEFINED UNWANTED methods

public:
  OPT_MTYPE_B( CODEMAP *htable, CFG *cfg): _htable(htable), _cfg(cfg) { }
  ~OPT_MTYPE_B(void) {}

  void Do_mtype_b(void);
}; // end of class OPT_MTYPE_B


// ====================================================================
// Create_BCVT - create a CVT node that converts from MTYPE_B to to_type
// ====================================================================
CODEREP *
OPT_MTYPE_B::Create_BCVT(MTYPE to_type, CODEREP *x)
{
  CODEREP stack_cr; 
  stack_cr.Init_expr(OPCODE_make_op(OPR_CVT, to_type, MTYPE_B), x);
  return _htable->Rehash(&stack_cr);
}

// ====================================================================
// Do_mtype_b_cr - determine whether given coderep node should have 
// MTYPE_B as result type; if yes, change result type to MTYPE_B.
// If its operand is MTYPE_B but the operator cannot operate on MTYPE_B, 
// insert a CVT for the operand.  Return the rehashed coderep node whenever
// it is not the original node.
// ====================================================================
CODEREP *
OPT_MTYPE_B::Do_mtype_b_cr(CODEREP *cr)
{
  CODEREP *x;
  CODEREP *new_cr = Alloc_stack_cr(cr->Extra_ptrs_used());
  BOOL need_rehash;
  INT32 i;
  MTYPE dtyp;
  OPERATOR opr;
  switch (cr->Kind()) {
  case CK_CONST:
  case CK_RCONST:
  case CK_LDA:
  case CK_VAR:
    return NULL;
  case CK_IVAR:
    need_rehash = FALSE;
    new_cr->Copy(*cr);
    x = Do_mtype_b_cr(cr->Ilod_base());
    if (x) {
      need_rehash = TRUE;
      if (x->Dtyp() == MTYPE_B) {
        Is_True(cr->Opr() == OPR_PARM,
	        ("OPT_MTYPE_B::Do_mtype_b_cr: iload base cannot be MTYPE_B"));
        x = Create_BCVT(cr->Ilod_base()->Dtyp(), x);
      }
      new_cr->Set_ilod_base(x);
    }
    if (cr->Opr() == OPR_MLOAD) {
      x = Do_mtype_b_cr(cr->Mload_size());
      if (x) {
	need_rehash = TRUE;
        new_cr->Set_mload_size(x);
        Is_True(x->Dtyp() != MTYPE_B,
	        ("OPT_MTYPE_B::Do_mtype_b_cr: mload size cannot be MTYPE_B"));
      }
    }
    if (need_rehash) { 
      new_cr->Set_istr_base(NULL);
      new_cr->Set_usecnt(0);
      new_cr->Set_ivar_occ(cr->Ivar_occ());
      cr->DecUsecnt();
      return _htable->Rehash(new_cr);
    }
    return NULL;
  case CK_OP:
    need_rehash = FALSE;
    new_cr->Copy(*cr);
    new_cr->Set_usecnt(0);
    opr = cr->Opr();
    // call recursively
    for (i = 0; i < cr->Kid_count(); i++) {
      dtyp = cr->Opnd(i)->Dtyp();
      x = Do_mtype_b_cr(cr->Opnd(i));
      if (x) {
	need_rehash = TRUE;
	new_cr->Set_opnd(i, x);
      }
      else new_cr->Set_opnd(i, cr->Opnd(i));
      if (new_cr->Opnd(i)->Dtyp() == MTYPE_B) {
        need_rehash = TRUE;
	if (opr == OPR_LNOT || opr == OPR_EQ || opr == OPR_NE ||
	    (opr == OPR_SELECT && i == 0)) {
	  // LNOT, EQ, NE and 0th operand of SELECT can take a boolean operand
	  new_cr->Set_dsctyp(MTYPE_B); 
	}
        else new_cr->Set_opnd(i, Create_BCVT(dtyp, new_cr->Opnd(i)));
      }
    }
    if (opr == OPR_EQ || opr == OPR_NE) { // special treatments for EQ and NE
      if (new_cr->Opnd(0)->Dtyp() == MTYPE_B && 
	  new_cr->Opnd(1)->Dtyp() != MTYPE_B) {
	if (new_cr->Opnd(1)->Kind() != CK_CONST) {
	  new_cr->Set_dsctyp(cr->Dsctyp());
          new_cr->Set_opnd(0, Create_BCVT(cr->Dsctyp(), new_cr->Opnd(0)));
	}
	else {
	  cr->DecUsecnt();
	  if (opr == OPR_NE && new_cr->Opnd(1)->Const_val() == 0 ||
	      opr == OPR_EQ && new_cr->Opnd(1)->Const_val() == 1)
	    return new_cr->Opnd(0);	// delete the NE or EQ
	  else if (opr == OPR_EQ && new_cr->Opnd(1)->Const_val() == 0 ||
		   opr == OPR_NE && new_cr->Opnd(1)->Const_val() == 1) {
	    new_cr->Set_opr(OPR_LNOT);
	    opr = OPR_LNOT;
	    new_cr->Set_kid_count(1);
	  }
	  else {
	    cr->DecUsecnt_rec();
	    return _htable->Add_const(cr->Dtyp(), opr == OPR_EQ ? 0 : 1);
	  }
	}
      }
      else if (new_cr->Opnd(0)->Dtyp() != MTYPE_B && 
	       new_cr->Opnd(1)->Dtyp() == MTYPE_B) {
	if (new_cr->Opnd(0)->Kind() != CK_CONST) {
	  new_cr->Set_dsctyp(cr->Dsctyp());
          new_cr->Set_opnd(1, Create_BCVT(cr->Dsctyp(), new_cr->Opnd(1)));
	}
	else {
	  cr->DecUsecnt();
	  if (opr == OPR_NE && new_cr->Opnd(0)->Const_val() == 0 ||
	      opr == OPR_EQ && new_cr->Opnd(0)->Const_val() == 1)
	    return new_cr->Opnd(1);	// delete the NE or EQ
	  else if (opr == OPR_EQ && new_cr->Opnd(0)->Const_val() == 0 ||
		   opr == OPR_NE && new_cr->Opnd(0)->Const_val() == 1) {
	    new_cr->Set_opr(OPR_LNOT);
	    opr = OPR_LNOT;
	    new_cr->Set_kid_count(1);
	    new_cr->Set_opnd(0, new_cr->Opnd(1));
	  }
	  else {
	    cr->DecUsecnt_rec();
	    return _htable->Add_const(MTYPE_B, opr == OPR_EQ ? 0 : 1);
	  }
	}
      }
    }

    switch (opr) {
    case OPR_LNOT: 
    case OPR_EQ: case OPR_NE: 
    case OPR_LT: case OPR_LE: case OPR_GT: case OPR_GE:
      need_rehash = TRUE;
      new_cr->Set_dtyp(MTYPE_B);
      break;
    default: ;
    }

    if (need_rehash) {
      cr->DecUsecnt();
      return _htable->Rehash(new_cr);
    }
    return NULL;
  }

  Fail_FmtAssertion("OPT_MTYPE_B::Do_mtype_b_cr should not get here");
  return NULL;
}

// ====================================================================
// Do_mtype_b - go over all statements and expressions in the PU, set
// appropriate coderep nodes' result type to MTYPE_B, and insert CVTs
// if it is necessary to convert the boolean type back to integer type
// ====================================================================
void
OPT_MTYPE_B::Do_mtype_b(void)
{
  CFG_ITER cfg_iter(_cfg);
  BB_NODE *bb;
  INT32 i;
  // visit all blocks 
  FOR_ALL_NODE( bb, cfg_iter, Init() ) {
    STMTREP_ITER stmt_iter(bb->Stmtlist());
    STMTREP *stmt;
    FOR_ALL_NODE(stmt, stmt_iter, Init()) {
      OPERATOR opr = stmt->Opr();
      CODEREP *rhs = stmt->Rhs();
      CODEREP *x;
      MTYPE dtyp;
      if (OPERATOR_is_call(opr) || opr == OPR_ASM_STMT) {
	for (i = 0; i < rhs->Kid_count(); i++) {
	  dtyp = rhs->Opnd(i)->Dtyp();
	  x = Do_mtype_b_cr(rhs->Opnd(i));
	  if (x)
	    rhs->Set_opnd(i, x);
	  if (rhs->Opnd(i)->Dtyp() == MTYPE_B)
	    rhs->Set_opnd(i, Create_BCVT(dtyp, rhs->Opnd(i)));
	}
	continue;
      }
      if (rhs) {
	if (opr == OPR_PREFETCH) {
	  x = Do_mtype_b_cr(rhs->Ilod_base());
	  if (x)
	    rhs->Set_ilod_base(x);
	  // Ilod_base() impossible to be MTYPE_B
          Is_True(rhs->Ilod_base()->Dtyp() != MTYPE_B,
	          ("OPT_MTYPE_B::Do_mtype_b: iload base cannot be MTYPE_B"));
	}
	else {
	  dtyp = rhs->Dtyp();
	  x = Do_mtype_b_cr(rhs);
	  if (x) {
	    stmt->Set_rhs(x);
	    rhs = x;
	  }
	  if (opr != OPR_FALSEBR && opr != OPR_TRUEBR && opr != OPR_ASSERT &&
	      rhs->Dtyp() == MTYPE_B)
	    stmt->Set_rhs(Create_BCVT(dtyp, rhs));
	}
      }
      if (OPERATOR_is_store(opr)) {
	CODEREP *lhs = stmt->Lhs();
        switch (opr) {
        case OPR_MSTORE:
	  x = Do_mtype_b_cr(lhs->Mstore_size());
	  if (x)
	    lhs->Set_mstore_size(x);
	  Is_True(lhs->Mstore_size()->Dtyp() != MTYPE_B,
		  ("OPT_MTYPE_B::Do_mtype_b: mstore size cannot be MTYPE_B"));
	  // fall thru
        case OPR_ISTORE:
	case OPR_ISTBITS:
	  x = Do_mtype_b_cr(lhs->Istr_base());
	  if (x)
	    lhs->Set_istr_base(x);
	  // Istr_base() impossible to be MTYPE_B
	  Is_True(lhs->Istr_base()->Dtyp() != MTYPE_B,
		  ("OPT_MTYPE_B::Do_mtype_b: istore base cannot be MTYPE_B"));
	  break;
        default: ;
        }
      }
    }
  }
}

// ====================================================================
//  Introduce_mtype_bool - Set up the OPT_MTYPE_B environment and call 
//  the relevant member functions 
// ====================================================================
void	       
COMP_UNIT::Introduce_mtype_bool(void)
{
  OPT_MTYPE_B opt_mtype_b(Htable(), Cfg());

  opt_mtype_b.Do_mtype_b();

  if ( Get_Trace(TP_GLOBOPT, BOOL_SIMP_FLAG)) {
    fprintf( TFile, "%sAfter COMP_UNIT::Introduce_mtype_bool\n%s",
             DBar, DBar );
    Cfg()->Print(TFile);
  }
}
