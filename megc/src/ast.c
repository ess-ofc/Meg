/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/ast.h>

#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>

static void delmap(struct mdeclmap *map) {
   auto bukp = map->fst;
   while (bukp) {
      mdecl_del(bukp->decl);
      bukp = bukp->next;
   }

   mdeclmap_del(map);
}

void munit_del(struct munit *self) {
   delmap(&self->scope);
   free(self);
}

void mhint_del(struct mhint *self) {
   if (self) {
      switch (self->kind) {
      case mHINT_INVAL:
         /*
          * Invalid nodes cannot
          * have memory allocations.
          */
         break;
      case mHINT_UNA:
         break;
      case mHINT_STRUCT:
         delmap(self->as.struc.scope);
         free(self->as.struc.scope);
         break;
      case mHINT_ARRAY:
         mhint_del(self->as.array.type);
         mexpr_del(self->as.array.size);
         break;
      case mHINT_SLICE:
         mhint_del(self->as.slice.type);
         break;
      }

      free(self);
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
      case mEXPR_PAREN:
         mexpr_del(self->as.paren.child);
         break;
      case mEXPR_OPERATION:
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
         break;
      case mDECL_TYPE:
         free(self->as.type.def);
         break;
      case mDECL_FUNC:
         delmap(self->as.func.scope);
         free(self->as.func.scope);
         mexpr_del(self->as.func.expr);
         break;
      case mDECL_OBJ:
         break;
      }

      mhint_del(self->type);
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
      case mSTMT_RESULT:
         mexpr_del(self->as.result.expr);
         break;
      case mSTMT_DEL:
         mexpr_del(self->as.del.expr);
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

static void prhint(
   const char *name,
   struct mhint *h
) {
   PRCONCEPT("Hint");
   printf("\033[1;38;2;255;255;150m%s\033[0m; ", name);

   switch (h->mode) {
   case mMODE_NONE:
      name = "none ";
      break;
   case mMODE_POSS:
      name = "$";
      break;
   case mMODE_REF:
      name = "&";
      break;
   }
   printf("%s", name);

   switch (h->qual) {
   case mQUAL_NONE:
      name = "none";
      break;
   case mQUAL_MUT:
      name = "mut";
      break;
   case mQUAL_CONST:
      name = "const";
      break;
   }
   puts(name);
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

static void prmap(struct mdeclmap *map) {
   auto decl = map->fst;
   while (decl) {
      mprdecl(decl->decl);
      decl = decl->next;
   }
}
void mprunit(struct munit *u) {
   indentation = 0;
   prunit(u->name);
   prmap(&u->scope);
}

void mprhint(struct mhint *t) {
   indent();

   switch (t->kind) {
   case mHINT_INVAL:
      prhint("inval", t);
      break;
   case mHINT_UNA:
      prhint("una", t);
      break;
   case mHINT_STRUCT:
      prhint("struct", t);
      prmap(t->as.struc.scope);
      break;
   case mHINT_ARRAY:
      prhint("array", t);
      mprexpr(t->as.array.size);
      mprhint(t->as.array.type);
      break;
   case mHINT_SLICE:
      prhint("slice", t);
      mprhint(t->as.slice.type);
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
   case mDECL_FUNC:
      prdecl("func", d->id);
      mprhint(d->type);
      prmap(d->as.func.scope);
      mprexpr(d->as.func.expr);
      break;
   case mDECL_OBJ:
      prdecl("obj", d->id);
      mprhint(d->type);
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
         prexpr("EXPR inval", nullptr);
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
   case mSTMT_RESULT:
      prstmt("result");
      mprexpr(s->as.result.expr);
      break;
   case mSTMT_DEL:
      prstmt("del");
      mprexpr(s->as.result.expr);
      break;
   }

   dedent();

   if (s->next) {
      mprstmt(s->next);
   }
}
