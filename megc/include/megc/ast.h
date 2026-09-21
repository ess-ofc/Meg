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

/* Qualified type. */

struct mtype;

enum mtype_qual {
   mQUAL_NONE = 0,
   mQUAL_MUT,
   mQUAL_CONST
};

typedef uintptr_t mqtype;

mqtype mqtype_new(
   struct mtype *type,
   enum mtype_qual qual
);

bool mqtype_isnil(mqtype qty);

struct mtype *mqtype_get(mqtype qty);

enum mtype_qual mqtype_qual(mqtype qty);

/* Types */

enum mtype_kind {
   mTYPE_INVAL = 0,
   mTYPE_UNA,
   mTYPE_REF,
   mTYPE_STRUCT,
   mTYPE_ARRAY,
   mTYPE_SLICE
};

struct mtype {
   union {
      struct muna {
         /* In semantic analysis. */
         struct mdecl *decl;
         const char *id;
      } una;

      struct mref {
         mqtype type;
      } ref;

      struct mstruct {
         struct mscope *scope;
      } struc;

      struct marray {
         struct mexpr *size;
         mqtype type;
      } array;

      struct mslice {
         mqtype type;
      } slice;
   } as;
   enum mtype_kind kind;
   bool checked;  // In semantic analysis.
};

void mtype_del(struct mtype *self);

/* Scopes. */

struct mscope {
   size_t size, count;
   struct mdeclentry *array;

   /*
    * We want the order that the
    * declarations was set.
    * `fst` to the first and
    * `lst` points to the
    * last declaration set.
    */
   struct mdeclentry {
      struct mdeclentry *next;
      uint64_t hash;
      size_t off, len;
      struct mdecl *decl;
   } *fst, *lst;
};

struct mscope *mscope_new();

void mscope_del(struct mscope *self);

/* Adds a new key and value. */
bool mscope_set(
   struct mscope *self,
   struct mdecl *decl
);

/* Gets a new value by key. */
struct mdecl *mscope_get(
   struct mscope *self,
   const char *id
);

/* Unit. */

struct munit {
   const char *name;
   struct mscope *scope;
};

void munit_del(struct munit *self);

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
   mqtype type;
   union {
      struct {
         struct mexpr *lhs, *rhs;
         enum mbin_op_kind kind;
      } bin_op;

      struct {
         struct mexpr *oprnd;
         enum muna_op_kind kind;
      } una_op;

      struct {
         const char *declid;
      } decl_ref;

      struct {
         struct mexpr *decl;  // Any callable expr.
         struct mexpr *args;
      } call;

      struct {
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

      struct {
         struct mexpr *child;
      } paren;

      struct {
         struct mscope *scope;
         struct mstmt *stmts;
      } operation;
   } as;
   enum mexpr_kind kind;
};

void mexpr_del(struct mexpr *self);

/* Declarations. */

enum mtypedef_kind {
   mTYPEDEF_NONE = 0,
   mTYPEDEF_DEF,
   mTYPEDEF_ALIAS
};

enum mdecl_kind {
   mDECL_INVAL = 0,
   mDECL_TYPE,
   mDECL_FUNC,
   mDECL_OBJ
};

struct mdecl {
   struct mloc loc;
   const char *id;
   mqtype type;
   union {
      /* For aliases and primitives. */
      struct {
         /* Used in semantic analysis. */
         union {
            struct mtypedef {
               size_t alignment;
               size_t size;
            } def;

            struct mtype *alias;
         } as;
         enum mtypedef_kind kind;
      } type;

      struct {
         struct mscope *scope;  // Only params.
         struct mexpr *expr;
      } func;

      struct {
         struct mexpr *expr;
      } obj;
   } as;
   enum mdecl_kind kind;
};

void mdecl_del(struct mdecl *self);

/* Statements. */

enum mstmt_kind {
   mSTMT_INVAL = 0,
   mSTMT_ASSIGN,
   mSTMT_EXPR,
   mSTMT_RESULT
};

struct mstmt {
   struct mstmt *next;
   struct mloc loc;
   union {
      struct {
         struct mexpr *decl;
         struct mexpr *expr;
      } assign;

      struct mexpr *expr;

      struct mexpr *result;
   } as;
   enum mstmt_kind kind;
};

void mstmt_del(struct mstmt *self);

void mprunit(struct munit *u);
void mprqtype(mqtype qt);
void mprdecl(struct mdecl *d);
void mprexpr(struct mexpr *e);
void mprstmt(struct mstmt *s);
