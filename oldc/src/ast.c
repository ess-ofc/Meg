/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/ast.h>
#include <megc/diagno.h>

#include <assert.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <xxh3.h>

#define QTYMASK ((mqtype) ~0b111)
#define SETQUAL(ptr, qual) (ptr ^ (qual & (mqtype)0b111))

mqtype mqtype_new(
   struct mtype *type,
   enum mtype_qual qual
) {
   auto ret = (mqtype) type;
   assert(
      !(ret & ~QTYMASK) &&
      "`type` must be aligned at 8 bytes."
   );

   ret = SETQUAL(ret, qual);
   return ret;
}

bool mqtype_isnil(mqtype qty) {
   return !mqtype_get(qty);
}

struct mtype *mqtype_get(mqtype qty) {
   return (void *) (qty & QTYMASK);
}

enum mtype_qual mqtype_qual(mqtype qty) {
   return qty & ~QTYMASK;
}

/* Scope. */

struct mscope *mscope_new() {
   struct mscope *ret = malloc(sizeof *ret);
   *ret = (struct mscope){};
   ret->size = 4;
   ret->count = 0;
   ret->array = calloc(
      ret->size,
      sizeof(struct mdeclentry)
   );
   return ret;
}

void mscope_del(
   struct mscope *self
) {
   auto bukp = self->fst;
   while (bukp) {
      mdecl_del(bukp->decl);
      bukp = bukp->next;
   }

   free(self->array);
   free(self);
}

static void checksize(
   struct mscope *self
) {
   if ((float) self->count / self->size > 0.70) {
      /* Old size and array. */
      auto olda = self->array;
      auto bukp = self->fst;

      /* Resets the array. */
      self->fst = nullptr;
      self->lst = nullptr;
      self->count = 0;
      self->size *= 4;
      self->array = calloc(
         self->size,
         sizeof(struct mdeclentry)
      );

      /* Remaps every valid entry. */
      while (bukp) {
         mscope_set(self, bukp->decl);
         bukp = bukp->next;
      }

      /* Frees the old array. */
      free(olda);
   }
}

bool mscope_set(
   struct mscope *self,
   struct mdecl *decl
) {
   checksize(self);

   size_t len = strlen(decl->id);
   uint64_t hash = XXH3_64bits(decl->id, len);
   struct mdeclentry buk = {
      .hash = hash,
      .len = len,
      .decl = decl
   };

   size_t pos = hash % self->size;
   auto bukp = &self->array[pos];
   while (true) {
      if (bukp->decl) {
         if (
            bukp->hash == hash &&
            bukp->len == len &&
            strncmp(
               bukp->decl->id,
               decl->id,
               len
            ) == 0
         ) {
            return false;
         }
      } else {
         *bukp = buk;
         break;
      }

      pos = (pos + 1) % self->size;
      bukp = &self->array[pos];
   }

   if (self->lst) {
      self->lst->next = bukp;
   } else {
      self->fst = bukp;
   }
   self->lst = bukp;
   self->count++;
   return true;
}

struct mdecl *mscope_get(
   struct mscope *self,
   const char *id
) {
   size_t len = strlen(id);
   uint64_t hash = XXH3_64bits(id, len);
   size_t pos = hash % self->size;

   auto bukp = &self->array[pos];
   while (true) {
      if (bukp->decl) {
         if (
            bukp->hash == hash &&
            bukp->len == len &&
            strncmp(
               bukp->decl->id,
               id,
               len
            ) == 0
         ) {
            return bukp->decl;
         }

         pos = (pos + 1) % self->size;
         bukp = &self->array[pos];
         continue;
      }

      return nullptr;
   }
}

/* Deleters. */

void munit_del(struct munit *self) {
   if (self) {
      mscope_del(self->scope);
      free(self);
   }
}

void mtype_del(struct mtype *self) {
   if (self) {
      switch (self->kind) {
      case mTYPE_INVAL:
      case mTYPE_UNA:
      case mTYPE_REF:
      case mTYPE_SLICE:
      case mTYPE_STRUCT:
         auto struc = &self->as.struc;
         mscope_del(struc->scope);
         break;
      case mTYPE_ARRAY:
         auto array = &self->as.array;
         mexpr_del(array->size);
         break;
      case mTYPE_FUNC:
         auto func = &self->as.func;
         mscope_del(func->scope);
         break;
      }
   }
}

void mexpr_del(struct mexpr *self) {
   if (self) {
      if (self->next) {
         /* It's an expression list. */
         mexpr_del(self->next);
      }

      switch (self->kind) {
      case mEXPR_INVAL:
         /*
          * Invalid nodes cannot
          * have memory allocations.
          */
         break;
      case mEXPR_BIN_OP:
         mexpr_del(self->as.bin_op.lhs);
         mexpr_del(self->as.bin_op.rhs);
         break;
      case mEXPR_UNA_OP:
         mexpr_del(self->as.una_op.oprnd);
         break;
      case mEXPR_DECL_REF:
         break;
      case mEXPR_CALL:
         mexpr_del(self->as.call.decl);
         mexpr_del(self->as.call.args);
         break;
      case mEXPR_LIT:
         break;
      case mEXPR_STRUCT:
         break;
      case mEXPR_ARRAY:
         mexpr_del(self->as.array.list);
         break;
      case mEXPR_PAREN:
         mexpr_del(self->as.paren.child);
         break;
      case mEXPR_OPERATION:
         mscope_del(self->as.operation.scope);
         free(self->as.operation.scope);
         mstmt_del(self->as.operation.stmts);
         break;
      }

      free(self);
   }
}

void mdecl_del(struct mdecl *self) {
   if (self) {
      switch (self->kind) {
      case mDECL_INVAL:
         /*
          * Invalid nodes cannot
          * have memory allocations.
          */
      case mDECL_TYPE:
      case mDECL_VALUE:
         break;
      case mDECL_FUNC:
         mscope_del(self->as.func.scope);
         mexpr_del(self->as.func.expr);
         break;
      case mDECL_OBJ:
         mexpr_del(self->as.obj.expr);
         break;
      }

      free(self);
   }
}

void mstmt_del(struct mstmt *self) {
   if (self) {
      if (self->next) {
         mstmt_del(self->next);
      }

      switch (self->kind) {
      case mSTMT_INVAL:
         /*
          * Invalid nodes cannot
          * have memory allocations.
          */
         break;
      case mSTMT_ASSIGN:
         mexpr_del(self->as.assign.decl);
         mexpr_del(self->as.assign.expr);
         break;
      case mSTMT_EXPR:
         mexpr_del(self->as.expr);
         break;
      case mSTMT_RESULT:
         mexpr_del(self->as.result);
         break;
      }

      free(self);
   }
}

/* AST printer. */

#define PRCONCEPT(name) \
   printf("\033[1;38;2;255;100;100m" name " \033[0m")

thread_local static int indentation = 0;

static void prunit(const char *name) {
   PRCONCEPT("Unit");
   printf("'%s'\n", name);
}

static void prtype(
   const char *name,
   enum mtype_qual qual,
   const char *fmt,
   ...
) {
   PRCONCEPT("Type");
   printf("\033[1;38;2;255;255;180m%s\033[0m", name);

   const char *qtab[] = {
      [mQUAL_NONE] = "\b",
      [mQUAL_MUT] = "mut",
      [mQUAL_CONST] = "const"
   };

   if (fmt) {
      printf("; %s ", qtab[qual]);
      va_list va;
      va_start(va);
      vprintf(fmt, va);
      va_end(va);
   }

   puts("");
}

static void prexpr(
   const char *name,
   const char *fmt,
   ...
) {
   PRCONCEPT("Expr");
   printf("\033[1;38;2;255;180;180m%s\033[0m", name);

   if (fmt) {
      printf("; ");
      va_list va;
      va_start(va);
      vprintf(fmt, va);
      va_end(va);
   }

   puts("");
}

static void prdecl(
   const char *name,
   const char *id
) {
   PRCONCEPT("Decl");
   printf(
      "\033[1;38;2;180;255;180m"
      "%s\033[0m; '%s'\n",
      name,
      id
   );
}

static void prstmt(
   const char *name
) {
   PRCONCEPT("Stmt");
   printf("\033[1;38;2;180;180;255m%s\033[0m\n", name);
}

static inline void indent() {
   indentation++;
   int i = 0;
   while (i++ < indentation) {
      printf("\033[1;90m\u2502 \033[0m");
   }
}

static inline void dedent() {
   indentation--;
}

static void prmap(struct mscope *map) {
   auto decl = map->fst;
   while (decl) {
      mprdecl(decl->decl);
      decl = decl->next;
   }
}

void mprunit(struct munit *u) {
   indentation = 0;
   prunit(u->name);
   prmap(u->scope);
}

void mprqtype(mqtype qt) {
   indent();

   auto t = mqtype_get(qt);
   auto qual = mqtype_qual(qt);

   if (!t) {
      prtype("null", mQUAL_NONE, nullptr);
      dedent();
      return;
   }

   switch (t->kind) {
   case mTYPE_INVAL:
      prtype("inval", qual, nullptr);
      break;
   case mTYPE_UNA:
      prtype("una", qual, t->as.una.id);
      break;
   case mTYPE_REF:
      prtype("ref", qual, nullptr);
      mprqtype(t->as.ref.type);
      break;
   case mTYPE_STRUCT:
      prtype("struct", qual, nullptr);
      auto b = t->as.struc.scope->fst;
      while (b) {
         mprdecl(b->decl);
         b = b->next;
      }
      break;
   case mTYPE_ARRAY:
      prtype("array", qual, nullptr);
      mprexpr(t->as.array.size);
      mprqtype(t->as.array.type);
      break;
   case mTYPE_SLICE:
      prtype("slice", qual, nullptr);
      mprqtype(t->as.slice.type);
      break;
   case mTYPE_FUNC:
      prtype("func", qual, nullptr);
      mprqtype(t->as.func.type);
      break;
   }

   dedent();
}

void mprdecl(struct mdecl *d) {
   indent();
   if (!d) {
      prdecl("null", "\b");
      dedent();
      return;
   }

   switch (d->kind) {
   case mDECL_INVAL:
      prdecl("inval", "\b");
      break;
   case mDECL_TYPE:
      prdecl("type", d->id);
      break;
   case mDECL_VALUE:
      prdecl("value", d->id);
      break;
   case mDECL_FUNC:
      prdecl("func", d->id);
      mprqtype(d->type);
      prmap(d->as.func.scope);
      mprexpr(d->as.func.expr);
      break;
   case mDECL_OBJ:
      prdecl("obj", d->id);
      mprqtype(d->type);
      break;
   }

   dedent();
}

void mprexpr(struct mexpr *e) {
   indent();
   if (!e) {
      prexpr("null", nullptr);
      dedent();
      return;
   }

   const char *name = nullptr;

   switch (e->kind) {
   case mEXPR_INVAL:
      prexpr("inval", nullptr);
      break;
   case mEXPR_BIN_OP:
      switch (e->as.bin_op.kind) {
      case mBIN_OP_INVAL:
         prexpr("inval", nullptr);
         goto end;
      case mBIN_OP_ADD:
         name = "+";
         break;
      case mBIN_OP_SUB:
         name = "-";
         break;
      case mBIN_OP_MUL:
         name = "*";
         break;
      case mBIN_OP_DIV:
         name = "/";
         break;
      case mBIN_OP_MOD:
         name = "%";
         break;
      case mBIN_OP_AND:
         name = "&";
         break;
      case mBIN_OP_BOR:
         name = "|";
         break;
      case mBIN_OP_EOR:
         name = "^";
         break;
      case mBIN_OP_LAND:
         name = "&&";
         break;
      case mBIN_OP_LOR:
         name = "||";
         break;
      case mBIN_OP_EQL:
         name = "==";
         break;
      case mBIN_OP_NEQ:
         name = "!=";
         break;
      case mBIN_OP_GTR:
         name = ">";
         break;
      case mBIN_OP_LSS:
         name = "<";
         break;
      case mBIN_OP_GEQ:
         name = ">=";
         break;
      case mBIN_OP_LEQ:
         name = "<=";
         break;
      }
      prexpr("bin_op", "%s", name);
      mprexpr(e->as.bin_op.lhs);
      mprexpr(e->as.bin_op.rhs);
      break;
   case mEXPR_UNA_OP:
      switch (e->as.una_op.kind) {
      case mUNA_OP_INVAL:
         prexpr("inval", nullptr);
         goto end;
      case mUNA_OP_PLUS:
         name = "+";
         break;
      case mUNA_OP_MINUS:
         name = "-";
         break;
      case mUNA_OP_NEG:
         name = "!";
         break;
      }
      prexpr("una_op", "%s", name);
      mprexpr(e->as.una_op.oprnd);
      break;
   case mEXPR_DECL_REF:
      prexpr("decl_ref", "'%s'", e->as.decl_ref.declid);
      break;
   case mEXPR_CALL:
      prexpr("call", nullptr);
      mprexpr(e->as.call.decl);
      mprexpr(e->as.call.args);
      break;
   case mEXPR_LIT:
      if (e->as.lit.eval) {
         prexpr("lit", "eval = true");
      } else {
         prexpr("lit", "eval = false");
      }
      break;
   case mEXPR_STRUCT:
      prexpr("struct", nullptr);
      break;
   case mEXPR_ARRAY:
      prexpr("array", nullptr);
      if (e->as.array.list) {
         mprexpr(e->as.array.list);
      }
      break;
   case mEXPR_PAREN:
      prexpr("paren", nullptr);
      mprexpr(e->as.paren.child);
      break;
   case mEXPR_OPERATION:
      prexpr("operation", nullptr);
      prmap(e->as.operation.scope);
      mprstmt(e->as.operation.stmts);
      break;
   }

   dedent();

end:
   if (e->next) {
      mprexpr(e->next);
   }
}

void mprstmt(struct mstmt *s) {
   indent();
   if (!s) {
      prstmt("null");
      dedent();
      return;
   }

   switch (s->kind) {
   case mSTMT_INVAL:
      prstmt("inval");
      break;
   case mSTMT_ASSIGN:
      prstmt("assign");
      mprexpr(s->as.assign.decl);
      mprexpr(s->as.assign.expr);
      break;
   case mSTMT_EXPR:
      prstmt("expr");
      mprexpr(s->as.expr);
      break;
   case mSTMT_RESULT:
      prstmt("result");
      mprexpr(s->as.result);
      break;
   }

   dedent();

   if (s->next) {
      mprstmt(s->next);
   }
}
