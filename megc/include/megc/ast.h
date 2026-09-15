/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/declmap.h>
#include <megc/loc.h>

#include <stddef.h>
#include <stdint.h>

/* Unit. */

struct munit {
   const char *name;
   struct mdeclmap scope;
};

void munit_del(struct munit *self);

/* Hints. */

enum mhint_mode {
   mMODE_NONE = 0,
   mMODE_REF,
   mMODE_POSS
};

enum mhint_qual {
   mQUAL_NONE = 0,
   mQUAL_MUT,
   mQUAL_CONST
};

enum mhint_kind {
   mHINT_INVAL = 0,
   mHINT_UNA,
   mHINT_STRUCT,
   mHINT_ARRAY,
   mHINT_SLICE
};

struct mhint {
   struct mloc loc;

   /* Articulation. */
   union {
      struct muna {
         const char *id;
         /* In semantic analysis. */
         struct mtypedef *def;
      } una;

      struct mstruct {
         struct mdeclmap *scope;
      } struc;

      struct marray {
         struct mhint *type;
         struct mexpr *size;
      } array;

      struct mslice {
         struct mhint *type;
      } slice;
   } as;

   /* Sets. */
   enum mhint_mode mode;
   enum mhint_qual qual;
   enum mhint_kind kind;
};

void mhint_del(struct mhint *self);

/* Expressions. */

enum mexpr_kind : uint8_t {
   mEXPR_INVAL = 0,
   mEXPR_BIN_OP,
   mEXPR_UNA_OP,
   mEXPR_DECL_REF,
   mEXPR_CALL,
   mEXPR_LIT,
   mEXPR_PAREN,
   mEXPR_OPERATION
};

enum mbin_op_kind : uint8_t {
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

enum muna_op_kind : uint8_t {
   mUNA_OP_INVAL = 0,
   mUNA_OP_PLUS,
   mUNA_OP_MINUS,
   mUNA_OP_NEG
};

enum mlit_kind : uint8_t {
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
   struct mhint *type;
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
         union {
            struct {
               const char *buf;
               int base;  // Integers only.
            } uneva;      // While eval = false.
            uint64_t u;
            int64_t i;
            double f;
            size_t l;  // Strings length.
         } as;
         enum mlit_kind kind;
         bool eval;
      } lit;

      struct mparen {
         struct mexpr *child;
      } paren;

      struct moperation {
         struct mdeclmap *scope;
         struct mstmt *stmts;
      } operation;
   } as;
   enum mexpr_kind kind;
};

void mexpr_del(struct mexpr *self);

/* Declarations. */

enum mdecl_kind : uint8_t {
   mDECL_INVAL = 0,
   mDECL_TYPE,
   mDECL_FUNC,
   mDECL_OBJ
};

struct mdecl {
   struct mloc loc;
   const char *id;
   struct mhint *type;
   union {
      /* For aliases and primitives. */
      struct mtype {
         /* Used in semantic analysis. */
         struct mtypedef *def;
      } type;

      struct mfunc {
         struct mdeclmap *scope;  // Only params.
         struct mexpr *expr;
      } func;
   } as;
   enum mdecl_kind kind;
};

void mdecl_del(struct mdecl *self);

/* Statements. */

enum mstmt_kind {
   mSTMT_INVAL = 0,
   mSTMT_ASSIGN,
   mSTMT_RESULT,
   mSTMT_DEL
};

struct mstmt {
   struct mstmt *next;
   struct mloc loc;
   union {
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

void mprunit(struct munit *u);
void mprhint(struct mhint *h);
void mprdecl(struct mdecl *d);
void mprexpr(struct mexpr *e);
void mprstmt(struct mstmt *s);
