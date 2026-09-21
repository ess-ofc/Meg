/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/sema.h>

#include <megc/ast.h>
#include <megc/diagno.h>
#include <megc/tymap.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sema {
   struct mtymap *tymap;
   struct mscope *slist[32];  // Max of 32 scopes.
   size_t scount;
};

/* Scopes. */

static void openscope(
   struct sema *self,
   struct mscope *scope
) {
   if (self->scount == 32) {
      madeus("Scope overflow.");
   }
   self->slist[self->scount++] = scope;
}

static void closescope(
   struct sema *self
) {
   if (self->scount == 0) {
      madeus("Scope count error.");
   }
   self->scount--;
}

static struct mdecl *getdecl(
   struct sema *self,
   struct mloc loc,
   const char *id
) {
   auto i = self->scount;
   while (i--) {
      auto x = mscope_get(self->slist[i], id);
      if (x) {
         return x;
      }
   }

   mferro(loc, "'%s' is undeclared.", id);
   return nullptr;
}

static mqtype getprimitive(
   struct sema *self,
   const char *id
) {
   auto s = self->slist[0];
   auto x = mscope_get(s, id);
   if (
      x && x->kind == mDECL_TYPE &&
      x->as.type.kind == mTYPEDEF_DEF
   ) {
      return mqtype_new(
         mtymap_set(
            self->tymap,
            &(struct mtype){
               .kind = mTYPE_UNA,
               .checked = true,
               .as.una = {
                  .id = id,
                  .decl = x
               }
            }
         ),
         mQUAL_CONST
      );
   }

   madeus(
      "'%s' is not a primitive!",
      id
   );
}

static size_t typename(
   mqtype qty,
   char *buf,
   size_t bufsz
) {
   assert(bufsz >= 64);
   auto type = mqtype_get(qty);
   auto qual = mqtype_qual(qty);
   char *b = buf;
   const char *bend = &b[bufsz - 1];
   size_t n = 0;

   switch (qual) {
   case mQUAL_NONE:
      break;
   case mQUAL_MUT:
      strcpy(b, "mut ");
      b += 4;
      break;
   case mQUAL_CONST:
      strcpy(buf + n, "const ");
      b += 6;
      break;
   }

   if (type) {
      switch (type->kind) {
      case mTYPE_INVAL:
         strcpy(b, "none");
         b += 4;
         break;
      case mTYPE_UNA:
         b += snprintf(
            b,
            bend - b,
            "%s",
            type->as.una.id
         );
         break;
      case mTYPE_REF:
         *b++ = '&';
         b += typename(
            type->as.ref.type,
            b,
            bend - b
         );
         break;
      case mTYPE_STRUCT:
         *b++ = '[';

         auto buk =
            type->as.struc.scope->fst;
         while (buk) {
            b += typename(
               buk->decl->type,
               b,
               bend - b
            );
            buk = buk->next;
            if (buk) {
               if ((uintptr_t) (bend - b) < 2) {
                  break;
               }
               strcpy(b, ", ");
               b += 2;
            }
         }

         if (b < bend) {
            *b++ = ']';
         }
         break;
      case mTYPE_ARRAY:
         // TODO: Print array size.
         strcpy(b, "[TODO]");
         b += 2;
         b += typename(
            type->as.array.type,
            b,
            bend - b
         );
         break;
      case mTYPE_SLICE:
         strcpy(b, "[~]");
         b += 3;
         b += typename(
            type->as.slice.type,
            b,
            bend - b
         );
         break;
      }
   }

   *b = 0;
   return b - buf;
}

static void aply(
   struct mloc loc,
   mqtype *type,
   mqtype other
) {
   auto t1 = mqtype_get(*type);
   auto t2 = mqtype_get(other);

   if (!t1 || t1 == t2) {
      *type = mqtype_new(
         t2,
         mqtype_qual(*type)
      );
   } else {
      char ts1[128], ts2[128];
      typename(*type, ts1, 128);
      typename(other, ts2, 128);
      mferro(loc, "Aplying '%s' to '%s'.", ts2, ts1);
   }
}

/* Node analysis. */

static void antype(struct sema *self, struct mloc, mqtype qty);
static void andecl(struct sema *self, struct mdecl *decl);
static void anexpr(struct sema *self, struct mexpr *expr);
static void anstmt(struct sema *self, struct mstmt *stmt);

static void antype(
   struct sema *self,
   struct mloc loc,
   mqtype qty
) {
   auto type = mqtype_get(qty);
   if (type && !type->checked) {
      type->checked = true;

      switch (type->kind) {
      case mTYPE_INVAL:
         madeus("An invalid type.");
         break;
      case mTYPE_UNA:
         auto una = &type->as.una;
         if (!una->decl) {
            auto decl = getdecl(
               self,
               loc,
               una->id
            );
            if (decl) {
               if (decl->kind == mDECL_TYPE) {
                  una->decl = decl;
               } else {
                  mferro(
                     loc,
                     "'%s' is not a type.",
                     una->id
                  );
               }
            }
         } else {
            /* Must be a primitive here. */
            assert(
               una->decl->kind == mDECL_TYPE &&
               una->decl->as.type.kind == mTYPEDEF_DEF
            );
         }
         break;
      case mTYPE_REF:
         auto ref = &type->as.ref;
         if (mqtype_isnil(ref->type)) {
            mferro(loc, "Untyped reference.");
            break;
         }

         antype(self, loc, ref->type);
         break;
      case mTYPE_STRUCT:
         auto field =
            type->as.struc.scope->fst;
         while (field) {
            andecl(self, field->decl);
            field = field->next;
         }
         break;
      case mTYPE_ARRAY:
         // TODO: Get the array size as a const expr value.
         auto array = &type->as.array;
         anexpr(self, array->size);
         if (mqtype_isnil(array->type)) {
            mferro(loc, "Untyped array.");
         }
         antype(self, loc, array->type);
         break;
      case mTYPE_SLICE:
         auto slice = &type->as.slice;
         if (mqtype_isnil(slice->type)) {
            mferro(loc, "Untyped array.");
         }
         antype(self, loc, slice->type);
         break;
      }

      type->checked = true;
   }
}

static void andecl(
   struct sema *self,
   struct mdecl *decl
) {
   switch (decl->kind) {
   case mDECL_INVAL:
      break;
   case mDECL_FUNC:
      auto func = &decl->as.func;
      antype(self, decl->loc, decl->type);

      /* Analyze parameters. */
      auto par = func->scope->fst;
      while (par) {
         andecl(self, par->decl);
         par = par->next;
      }

      /* Analyze expression and hint. */
      if (func->expr) {
         anexpr(self, func->expr);
         aply(
            decl->loc,
            &decl->type,
            func->expr->type
         );
      } else {
         if (mqtype_isnil(decl->type)) {
            mferro(decl->loc, "Can't infer result type.");
         }
      }

      char buf[128];
      typename(decl->type, buf, 128);
      minfo("Type `%s`.", buf);
      break;
   case mDECL_OBJ:
      auto obj = &decl->as.obj;
      antype(self, decl->loc, decl->type);
      if (obj->expr) {
         anexpr(self, obj->expr);
         aply(
            decl->loc,
            &decl->type,
            obj->expr->type
         );
      } else if (mqtype_isnil(decl->type)) {
         mferro(decl->loc, "Can't infer result type.");
      }
      break;

   case mDECL_TYPE:
      auto type = &decl->as.type;
      if (type->kind == mTYPEDEF_DEF) {
         break;
      }
      madeus("Developed types are unsupported.");
      break;
   }
}

static void anexpr(
   struct sema *self,
   struct mexpr *expr
) {
   switch (expr->kind) {
   case mEXPR_INVAL:
      break;
   case mEXPR_BIN_OP:
      auto lhs = expr->as.bin_op.lhs;
      auto rhs = expr->as.bin_op.rhs;
      anexpr(self, lhs);
      anexpr(self, rhs);

      if (
         mqtype_get(lhs->type) ==
         mqtype_get(rhs->type)
      ) {
         aply(expr->loc, &expr->type, lhs->type);
         aply(expr->loc, &expr->type, rhs->type);
      } else {
         mferro(expr->loc, "Invalid operands.");
      }
      break;
   case mEXPR_UNA_OP:
      auto oprnd = expr->as.una_op.oprnd;
      anexpr(self, oprnd);

      aply(expr->loc, &expr->type, oprnd->type);
      break;
   case mEXPR_DECL_REF:
      auto declref = &expr->as.decl_ref;
      auto x = getdecl(
         self,
         expr->loc,
         declref->declid
      );
      if (x) {
         if (x->kind == mDECL_OBJ) {
            aply(expr->loc, &expr->type, x->type);
         } else {
            mferro(expr->loc, "'%s' is not an object.");
         }
      }
      break;
   case mEXPR_CALL:
      auto call = &expr->as.call;
      anexpr(self, call->decl);
      madeus("Function calls are still unsupported.");
      break;
   case mEXPR_LIT:
      auto lit = &expr->as.lit;
      lit->eval = true;
      switch (lit->kind) {
      case mLIT_INVAL:
         lit->eval = false;
         break;
      case mLIT_INTEGER:
         lit->as.i = strtoull(
            lit->as.uneva.buf,
            nullptr,
            lit->as.uneva.base
         );
         expr->type =
            getprimitive(self, "i32");
         break;
      case mLIT_FLOAT:
      default:
         madeus("Unsupported literal.");
      }
      break;
   case mEXPR_PAREN:
      auto paren = &expr->as.paren;
      anexpr(self, paren->child);
      aply(expr->loc, &expr->type, paren->child->type);
      break;
   case mEXPR_OPERATION:
      madeus("Operations are not unsupported yet.");
   }
}

[[maybe_unused]]
static void anstmt(struct sema *, struct mstmt *) {
}

bool msema_analyze(
   struct munit *unit,
   struct mtymap *tymap
) {
   struct sema self = {
      .tymap = tymap
   };

   openscope(&self, unit->scope);
   auto bukp = unit->scope->fst;
   while (bukp) {
      andecl(&self, bukp->decl);
      bukp = bukp->next;
   }
   closescope(&self);

   return true;
}
