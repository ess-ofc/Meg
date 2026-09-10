/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/declmap.h>
#include <megc/diagno.h>
#include <megc/sema.h>

#include <assert.h>
#include <malloc.h>

enum scope_kind {
   SCOPE_INVAL = 0,
   SCOPE_UNIT,
   SCOPE_LOCAL
};

struct scope {
   struct scope *parent;
   struct mdeclmap decls;
   enum scope_kind kind;
};

struct analyzer {
   /* Current scope. */
   struct scope *scope;
};

static void newscope(
   struct analyzer *self,
   enum scope_kind kind
) {
   struct scope *s = malloc(sizeof *s);
   s->parent = self->scope;
   s->decls = mdeclmap_new();
   s->kind = kind;

   self->scope = s;
}

static void delscope(
   struct analyzer *self
) {
   assert(self->scope);

   auto s = self->scope;
   self->scope = s->parent;
   mdeclmap_del(&s->decls);
   free(s);
}

void anexpr(struct mexpr *expr) {
   switch (expr->kind) {
   case mEXPR_BIN_OP:
      auto binop = &expr->as.bin_op;
      anexpr(binop->lhs);
      anexpr(binop->rhs);
      break;

   case mEXPR_UNA_OP:
      auto unaop = &expr->as.una_op;
      anexpr(unaop->oprnd);
      break;

   case mEXPR_DECL_REF:
      break;

   case mEXPR_CALL:
      auto call = &expr->as.call;
      auto arg = call->args;
      while (arg) {
         anexpr(arg);
         arg = arg->next;
      }
      break;

   case mEXPR_LIT:
      break;

   case mEXPR_PAREN:
      auto paren = &expr->as.paren;
      anexpr(paren->child);

   case mEXPR_OPERATION:
      break;

   case mEXPR_INVAL:
      break;
   }
}

void andecl(struct mdecl *decl) {
   bool infer = decl->type->kind == mTYPE_INVAL;

   switch (decl->kind) {
   case mDECL_FUNC:
      auto func = &decl->as.func;
      if (func->params) {
         auto param = func->params;
         while (param) {
            andecl(param);
            param = param->next;
         }
      }

      if (func->expr) {
         anexpr(func->expr);
      } else {
         if (infer) {
            mferro(decl->loc, "Can't infer result type.");
            return;
         }
      }
      break;

   case mDECL_OBJ:
      break;

   case mDECL_INVAL:
   }
}

bool manalyze(struct munit *unit) {
   minfo("Analyzing the '%s' unit.", unit->name);

   struct analyzer self = {};
   newscope(&self, SCOPE_UNIT);

   auto decl = unit->decls;
   while (decl) {
      andecl(decl);
      decl = decl->next;
   }

   delscope(&self);
   return true;
}
