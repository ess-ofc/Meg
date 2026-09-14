/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

// TODO: Rewrite all the AST print system.

#include <megc/ast.h>

#include <malloc.h>
#include <stdio.h>

void prmap(struct mdeclmap *map, int ind) {
   auto a = map->array;
   for (size_t i = 0; i < map->size; i++) {
      if (a[i].decl) {
         mdecl_print(a[i].decl, ind);
      }
   }
}

void delmap(struct mdeclmap *map) {
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
      if (self->as.array.type) {
         mhint_del(self->as.array.type);
      }
      if (self->as.array.size) {
         mexpr_del(self->as.array.size);
      }
      break;
   case mHINT_SLICE:
      if (self->as.slice.type) {
         mhint_del(self->as.slice.type);
      }
      break;
   }

   free(self);
}

void mexpr_del(struct mexpr *self) {
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
      if (self->as.bin_op.rhs) {  // Optional field.
         mexpr_del(self->as.bin_op.rhs);
      }
      break;
   case mEXPR_UNA_OP:
      mexpr_del(self->as.una_op.oprnd);
      break;
   case mEXPR_DECL_REF:
      break;
   case mEXPR_CALL:
      mexpr_del(self->as.call.decl);
      if (self->as.call.args) {
         mexpr_del(self->as.call.args);
      }
      break;
   case mEXPR_LIT:
      break;
   case mEXPR_PAREN:
      mexpr_del(self->as.paren.child);
      break;
   case mEXPR_OPERATION:
      if (self->as.operation.stmts) {
         mstmt_del(self->as.operation.stmts);
      }
      break;
   }

   free(self);
}

void mdecl_del(struct mdecl *self) {
   switch (self->kind) {
   case mDECL_INVAL:
      /*
       * Invalid nodes cannot
       * have memory allocations.
       */
      break;
   case mDECL_TYPE:
      if (self->as.type.def) {
         free(self->as.type.def);
      }
      break;
   case mDECL_FUNC:
      delmap(self->as.func.scope);
      free(self->as.func.scope);
      if (self->as.func.expr) {
         mexpr_del(self->as.func.expr);
      }
      break;
   case mDECL_OBJ:
      break;
   }

   if (self->type) {
      mhint_del(self->type);
   }
   free(self);
}

void mstmt_del(struct mstmt *self) {
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
   case mSTMT_DEF:
      if (self->as.def.decl) {
         mdecl_del(self->as.def.decl);
      }
      if (self->as.def.init) {
         mexpr_del(self->as.def.init);
      }
      break;
   case mSTMT_ASSIGN:
      if (self->as.assign.decl) {
         mexpr_del(self->as.assign.decl);
      }
      if (self->as.assign.expr) {
         mexpr_del(self->as.assign.expr);
      }
      break;
   case mSTMT_RESULT:
      if (self->as.result.expr) {
         mexpr_del(self->as.result.expr);
      }
      break;
   case mSTMT_DEL:
      if (self->as.del.expr) {
         mexpr_del(self->as.del.expr);
      }
      break;
   }

   free(self);
}

static void indent(int ind) {
   while (ind--) {
      printf("   ");
   }
}

void munit_print(struct munit *u) {
   printf("Unit '%s' {\n", u->name);
   prmap(&u->scope, 1);
   puts("}");
}

void mhint_print(struct mhint *t, int ind) {
   indent(ind);

   switch (t->kind) {
   case mHINT_INVAL:
      puts("Hint {");
      break;
   case mHINT_UNA:
      printf("HintUna '%s' {\n", t->as.una.id);
      break;
   case mHINT_STRUCT:
      puts("HintStruct {");
      prmap(t->as.struc.scope, ind + 1);
      break;
   case mHINT_ARRAY:
      puts("HintArray {");
      mexpr_print(t->as.array.size, ind + 1);
      mhint_print(t->as.array.type, ind + 1);
      break;
   case mHINT_SLICE:
      puts("HintSlice {");
      mhint_print(t->as.slice.type, ind + 1);
      break;
   }

   if (t->mode) {
      indent(ind + 1);
      puts(
         t->mode == mMODE_REF ?
            "mode &" :
            "mode $"
      );
   }
   if (t->qual) {
      indent(ind + 1);
      puts(
         t->qual == mQUAL_MUT ?
            "qual mut" :
            "qual const"
      );
   }
   indent(ind);
   puts("}");
}

void mdecl_print(struct mdecl *d, int ind) {
   indent(ind);

   switch (d->kind) {
   case mDECL_INVAL:
      puts("Decl is Invalid");
      break;
   case mDECL_TYPE:
      printf("DeclType '%s'\n", d->id);
      break;
   case mDECL_FUNC:
      printf("DeclFunc '%s' {\n", d->id);
      if (d->type) {
         mhint_print(d->type, ind + 1);
      }
      if (d->as.func.scope) {
         prmap(d->as.func.scope, ind + 1);
      }
      if (d->as.func.expr) {
         mexpr_print(d->as.func.expr, ind + 1);
         indent(ind);
      }

      puts("}");
      break;
   case mDECL_OBJ:
      printf("DeclObj '%s'", d->id);
      if (d->type) {
         puts(" {");
         mhint_print(d->type, ind + 1);
         indent(ind);
         puts("}");
      } else {
         puts("");
      }
      break;
   }
}

void mexpr_print(struct mexpr *e, int ind) {
   const char *name = 0;

   indent(ind);

   switch (e->kind) {
   case mEXPR_INVAL:
      puts("Expr is invalid");
      break;
   case mEXPR_BIN_OP:
      switch (e->as.bin_op.kind) {
      case mBIN_OP_INVAL:
         puts("ExpxBinOp is invalid");
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
      printf("ExprBinOp %s {\n", name);
      mexpr_print(e->as.bin_op.lhs, ind + 1);
      mexpr_print(e->as.bin_op.rhs, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mEXPR_UNA_OP:
      switch (e->as.una_op.kind) {
      case mUNA_OP_INVAL:
         puts("ExprUnaOp is invalid");
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
      printf("ExprUnaOp %s {\n", name);
      mexpr_print(e->as.una_op.oprnd, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mEXPR_DECL_REF:
      printf("ExprDeclRef '%s'\n", e->as.decl_ref.declid);
      break;
   case mEXPR_CALL:
      puts("ExprCall {");
      mexpr_print(e->as.call.decl, ind + 1);
      if (e->as.call.args) {
         mexpr_print(e->as.call.args, ind + 1);
      }
      indent(ind);
      puts("}");
      break;
   case mEXPR_LIT:
      printf(
         "ExprLit evaluated = %s, value = ",
         e->as.lit.eval ? "true" : "false"
      );
      if (e->as.lit.eval) {
         switch (e->as.lit.kind) {
            break;
         case mLIT_INTEGER:
            printf("%zu\n", e->as.lit.as.i);
            break;
         case mLIT_FLOAT:
            printf("%f\n", e->as.lit.as.f);
            break;

         default:
            puts("UnsupportedLiteral");
         }
      } else {
         puts(e->as.lit.as.uneva.buf);
      }
      break;
   case mEXPR_PAREN:
      printf("ExprParen {\n");
      mexpr_print(e->as.paren.child, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mEXPR_OPERATION:
      printf("ExprOperation {\n");
      mstmt_print(e->as.operation.stmts, ind + 1);
      indent(ind);
      puts("}");
      break;
   }

end:
   if (e->next) {
      mexpr_print(e->next, ind);
   }
}

void mstmt_print(struct mstmt *s, int ind) {
   indent(ind);

   switch (s->kind) {
   case mSTMT_INVAL:
      puts("Stmt is Invalid");
      break;
   case mSTMT_DEF:
      puts("StmtDef {");
      mdecl_print(s->as.def.decl, ind + 1);
      mexpr_print(s->as.def.init, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mSTMT_ASSIGN:
      puts("StmtAssign {");
      mexpr_print(s->as.assign.decl, ind + 1);
      mexpr_print(s->as.assign.expr, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mSTMT_RESULT:
      puts("StmtResult {");
      mexpr_print(s->as.result.expr, ind + 1);
      indent(ind);
      puts("}");
      break;
   case mSTMT_DEL:
      puts("StmtDel {");
      mexpr_print(s->as.result.expr, ind + 1);
      indent(ind);
      puts("}");
      break;
   }

   if (s->next) {
      mstmt_print(s->next, ind);
   }
}
