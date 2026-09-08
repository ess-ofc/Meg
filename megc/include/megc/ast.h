/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/loc.h>

#include <stddef.h>
#include <stdint.h>

/* Unit. */

struct munit {
   const char *name;
   struct mdecl *decls;
};

void munit_del(struct munit *self);

/* Concepts. */

enum mtype_kind {
   mTYPE_INVAL = 0,
   mTYPE_IDENT,
   mTYPE_STRUCT,
   mTYPE_ARRAY,
   mTYPE_SLICE
};

struct mtype {
   union {
      struct mident {
         const char *name;
      } ident;

      struct mstruct {
         struct mdecl *fields;
      } struc;

      struct marray {
         struct mtype *type;
         struct mexpr *size;
      } array;

      struct mslice {
         struct mtype *szty;
         struct mtype *rgty;
      } slice;
   } as;
   bool mut;
   enum mtype_kind kind;
};

void mtype_del(struct mtype *self);

/* Expressions. */

enum mexpr_kind {
   mEXPR_INVAL = 0,
   mEXPR_BIN_OP,
   mEXPR_UNA_OP,
   mEXPR_DECL_REF,
   mEXPR_CALL,
   mEXPR_LIT,
   mEXPR_PAREN,
   mEXPR_OPERATION
};

enum mbin_op_kind {
   mBIN_OP_INVAL = 0,
   mBIN_OP_ADD,
   mBIN_OP_SUB,
   mBIN_OP_MUL,
   mBIN_OP_DIV,
   mBIN_OP_MOD,
   mBIN_OP_AND,
   mBIN_OP_BOR,
   mBIN_OP_EOR,
   mBIN_OP_LAND,
   mBIN_OP_LOR,
   mBIN_OP_EQL,
   mBIN_OP_NEQ,
   mBIN_OP_GTR,
   mBIN_OP_LSS,
   mBIN_OP_GEQ,
   mBIN_OP_LEQ
};

enum muna_op_kind {
   mUNA_OP_INVAL = 0,
   mUNA_OP_PLUS,
   mUNA_OP_MINUS,
   mUNA_OP_NEG
};

enum mlit_kind {
   mLIT_INVAL = 0,
   mLIT_INTEGER,
   mLIT_FLOAT,
   mLIT_BOOL,
   mLIT_STRING,
   mLIT_CHAR
};

struct mexpr {
   struct mexpr *next;  // Used only in lists.
   struct mloc loc;
   union {
      struct mbin_op {
         struct mexpr *lhs, *rhs;
         enum mbin_op_kind kind;
      } bin_op;

      struct muna_op {
         struct mexpr *oprnd;
         enum muna_op_kind kind;
      } una_op;

      struct mdecl_ref {
         const char *declid;
      } decl_ref;

      struct mcall {
         struct mexpr *decl;  // Any callable expr.
         struct mexpr *args;
      } call;

      struct mlit {
         const char *buf;
         enum mlit_kind kind;
      } lit;

      struct mparen {
         struct mexpr *child;
      } paren;

      struct moperation {
         struct mstmt *stmts;
      } operation;
   } as;
   enum mexpr_kind kind;
};

void mexpr_del(struct mexpr *self);

/* Declarations. */

enum mdecl_kind {
   mDECL_INVAL = 0,
   mDECL_FUNC,
   mDECL_OBJ
};

struct mdecl {
   struct mdecl *next;
   struct mloc loc;
   const char *id;
   struct mtype *type;
   union {
      struct mfunc_decl {
         struct mdecl *params;
         struct mexpr *expr;
      } func;
   } as;
   enum mdecl_kind kind;
};

void mdecl_del(struct mdecl *self);

/* Statements. */

enum mstmt_kind {
   mSTMT_INVAL = 0,
   mSTMT_DEF,
   mSTMT_ASSIGN,
   mSTMT_RESULT,
   mSTMT_DEL
};

struct mstmt {
   struct mstmt *next;
   struct mloc loc;
   union {
      struct mdef {
         struct mdecl *decl;
         struct mexpr *init;
      } def;

      struct massign {
         struct mexpr *decl;
         struct mexpr *expr;
      } assign;

      struct mnew {
         const char *id;
         uint64_t objid;
      } new;

      struct mdel {
         struct mexpr *expr;
      } del;

      struct mresult {
         struct mexpr *expr;
      } result;
   } as;
   enum mstmt_kind kind;
};

void mstmt_del(struct mstmt *self);

void munit_print(struct munit *u);
void mdecl_print(struct mdecl *e, int ind);
void mexpr_print(struct mexpr *e, int ind);
void mstmt_print(struct mstmt *s, int ind);
