/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/ast.h>
#include <megc/diagno.h>
#include <megc/lexer.h>
#include <megc/parser.h>
#include <megc/token.h>
#include <megc/tymap.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct parser {
   struct mstrpool *strpool;
   struct mtymap *tymap;
   struct mlexer lex;
   struct mtoken fst, snd;
   const char *buf;
};

static struct mtoken advance(
   struct parser *self
) {
   if (self->snd.kind != mTOK_INVAL) {
      self->fst = self->snd;
      self->snd = mlexer_lex(&self->lex);
   } else {
      self->fst = mlexer_lex(&self->lex);
      self->snd = mlexer_lex(&self->lex);
   }

   return self->fst;
}

/* Current token. */
static inline struct mtoken cur(
   struct parser *self
) {
   return self->fst;
}

/* The next token. */
static inline struct mtoken nxt(
   struct parser *self
) {
   return self->snd;
}

/* Skips mTOK_DOC and mTOK_EOL, if any. */
static inline struct mtoken nxtvalid(
   struct parser *self
) {
   auto t = cur(self);
   while (t.kind != mTOK_EOF) {
      if (t.kind != mTOK_EOL) {
         break;
      }

      t = advance(self);
   }
   return t;
}

static void skipuntil(
   struct parser *self,
   enum mtoken_kind tok
) {
   while (cur(self).kind != tok) {
      auto t = advance(self);
      if (t.kind == mTOK_EOF) {
         return;
      }
   }

   advance(self);
}

static inline bool expect(
   struct parser *self,
   struct mtoken *tok,
   enum mtoken_kind kind
) {
   auto t = nxtvalid(self);
   if (tok) {
      *tok = t;
   }

   if (t.kind == kind) {
      advance(self);
      return true;
   } else {
      return false;
   }
}

static inline bool eol(
   struct parser *self,
   struct mtoken *tok
) {
   *tok = cur(self);
   if (
      tok->kind == mTOK_EOL ||
      tok->kind == mTOK_EOF
   ) {
      advance(self);
      return true;
   } else {
      return false;
   }
}

/* Declares something. */
static void declare(
   struct mscope *scope,
   struct mdecl *decl
) {
   if (!mscope_set(scope, decl)) {
      auto prev = mscope_get(scope, decl->id);
      mferro(decl->loc, "'%s' already declared in this scope.", decl->id);
      mfnote(prev->loc, "Previous declaration is here.");
   }
}

static struct mdecl *padecl(
   struct parser *self
);
static struct mexpr *paexpr(
   struct parser *self,
   int prec
);

/* Type. */

static mqtype pahint(
   struct parser *self
) {
   struct mtoken tok = cur(self);
   struct mtype ret = {};

   enum mtype_qual qual = 0;

   if (tok.kind == mTOK_MUT) {
      qual = mQUAL_MUT;
      advance(self);
   } else if (tok.kind == mTOK_CONST) {
      qual = mQUAL_CONST;
      advance(self);
   }

   tok = cur(self);
   switch (tok.kind) {
   case mTOK_AND:
      advance(self);
      ret.kind = mTYPE_REF;
      ret.as.ref.type = pahint(self);
      break;
   case mTOK_ID:
      advance(self);
      ret.kind = mTYPE_UNA;
      ret.as.una.id = tok.lit;
      break;
   case mTOK_LPAREN:
      /*
       * Functional types are
       * written as:
       *
       * (<parameter list>) <type>
       *
       * EXAMPLE  Functional type
       *          that receives two
       *          integers and
       *          results in a bool.
       *
       * (x: i32, y: i32) bool
       *
       * Parameters must have
       * name.
       */
      advance(self);
      ret.kind = mTYPE_FUNC;

      auto scope = mscope_new();
      if (!expect(self, &tok, mTOK_RPAREN)) {
getparm:
         auto parm = padecl(self);
         if (!parm->kind) {
            mdecl_del(parm);
            mferro(tok.loc, "Expected parameter declaration.");
            skipuntil(self, mTOK_RPAREN);
         } else {
            declare(scope, parm);

            if (expect(self, &tok, mTOK_COMMA)) {
               nxtvalid(self);
               goto getparm;
            }

            if (!expect(self, &tok, mTOK_RPAREN)) {
               mferro(tok.loc, "Ecpected ')'.");
               skipuntil(self, mTOK_RPAREN);
            }
         }
      }

      ret.as.func.scope = scope;
      ret.as.func.type = pahint(self);
      break;
   case mTOK_LBRCKT:
      advance(self);
      tok = nxtvalid(self);

      switch (tok.kind) {
      case mTOK_RBRCKT:
         /*
          * Is a slice, the syntax is:
          * [] <type hint>
          * EXAMPLE An slice of type i32,
          *         size is a u64.
          * [] i32
          */
         ret.kind = mTYPE_SLICE;
         advance(self);

         auto slice = &ret.as.slice;
         slice->type = pahint(self);
         break;
      case mTOK_ID:
         if (nxt(self).kind == mTOK_COLON) {
            /*
             * Is an object declaration,
             * that is, a structure of
             * objects.
             */
            ret.kind = mTYPE_STRUCT;

            auto struc = &ret.as.struc;
            struc->scope = mscope_new();

            if (!expect(self, &tok, mTOK_RBRCKT)) {
               for (;;) {
                  auto field = padecl(self);
                  if (!field->kind) {
                     mdecl_del(field);
                     mferro(cur(self).loc, "Expected declaration.");
                     skipuntil(self, mTOK_RBRCKT);
                     break;
                  }

                  declare(struc->scope, field);

                  if (!expect(self, &tok, mTOK_COMMA)) {
                     if (!expect(self, &tok, mTOK_RBRCKT)) {
                        mferro(tok.loc, "Expected ']'.");
                        skipuntil(self, mTOK_RBRCKT);
                     }

                     break;
                  }

                  if (expect(self, &tok, mTOK_RBRCKT)) {
                     mfwarn(tok.loc, "Dangling comma.");
                     break;
                  }
               }
            }
            break;
         }
      /* fallthrough, it's an array. */
      default:
         /*
          * Should be an array,
          * The syntax is:
          * [<expr>] <type hint>
          * EXAMPLE An i32 array of
          *         size 4.
          * [4] i32
          */
         ret.kind = mTYPE_ARRAY;

         auto array = &ret.as.array;
         nxtvalid(self);
         array->size = paexpr(self, 0);

         if (!expect(self, &tok, mTOK_RBRCKT)) {
            mferro(tok.loc, "Expected ']'.");
            ret.kind = mTYPE_INVAL;
            break;
         }

         array->type = pahint(self);
      }
      break;

   default:
      /* Returns a null type. */
      return mqtype_new(nullptr, qual);
   }

   return mqtype_new(
      mtymap_set(self->tymap, &ret),
      qual
   );
}

/* Statements. */

static struct mstmt *pastmt(
   struct parser *self,
   struct mscope *scope
) {
   auto tok = cur(self);

   struct mstmt *ret = malloc(sizeof *ret);
   *ret = (struct mstmt){
      .loc = tok.loc
   };

   switch (tok.kind) {
   case mTOK_ID:
      switch (nxt(self).kind) {
      case mTOK_COLON:
         /* Special case. */
         auto d = padecl(self);
         if (d->kind) {
            declare(scope, d);
            mstmt_del(ret);
            return nullptr;
         }
         mdecl_del(d);
         goto expr;
      case mTOK_ASSIGN:
         advance(self);
         advance(self);
         ret->kind = mSTMT_ASSIGN;
         ret->as.assign.expr = paexpr(self, 0);
         break;
      default:
         goto expr;
      }
      goto end;
   case mTOK_RESULT:
      advance(self);
      ret->kind = mSTMT_RESULT;
      ret->as.result = paexpr(self, 0);
      goto end;
   default:
      goto expr;
   }

expr:
   ret->kind = mSTMT_EXPR;
   ret->as.expr = paexpr(self, 0);

end:
   return ret;
}

/* Expressions. */

static struct mexpr *paexpr(struct parser *self, int prec);

static struct mexpr *nud(
   struct parser *self
) {
   struct mtoken tok = cur(self);
   assert(tok.kind < mTOK_MAX);

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .loc = tok.loc
   };

   constexpr int ptable[] = {
      [mTOK_ADD] = 90,
      [mTOK_SUB] = 90,
      [mTOK_AND] = 90,
      [mTOK_NEG] = 90,

      [mTOK_LPAREN] = 100,
      [mTOK_LBRACE] = 100
   };

   int prec = ptable[tok.kind];

   switch (tok.kind) {
   case mTOK_ADD:
   case mTOK_SUB:
   case mTOK_AND:
   case mTOK_NEG:
      ret->kind = mEXPR_UNA_OP;
      switch (tok.kind) {
      case mTOK_ADD:
         ret->as
            .una_op.kind = mUNA_OP_PLUS;
         break;
      case mTOK_SUB:
         ret->as
            .una_op.kind = mUNA_OP_MINUS;
         break;
      case mTOK_NEG:
         ret->as
            .una_op.kind = mUNA_OP_NEG;
         break;
      default:
      }

      advance(self);
      ret->as
         .una_op.oprnd = paexpr(self, prec);
      break;
   case mTOK_LPAREN:
      ret->kind = mEXPR_PAREN;

      advance(self);
      ret->as
         .paren.child = paexpr(self, 0);
      if (!expect(self, &tok, mTOK_RPAREN)) {
         mferro(tok.loc, "Expected ')'.");
         skipuntil(self, mTOK_LPAREN);
         return ret;
      }
      break;
   case mTOK_LBRACE:
      ret->kind = mEXPR_OPERATION;
      auto oper = &ret->as.operation;

      advance(self);
      oper->scope = mscope_new();

      if (!expect(self, &tok, mTOK_RBRACE)) {
         struct mstmt *lst = pastmt(self, oper->scope);
         if (!lst) {
            mferro(lst->loc, "Empty operation.");
            skipuntil(self, mTOK_RBRACE);
         }
         oper->stmts = lst;

         for (;;) {
            tok = cur(self);
            if (eol(self, &tok)) {
               continue;
            }

            if (expect(self, &tok, mTOK_RBRACE)) {
               break;
            }
            /*
             * If stmt == nullptr,
             * It's not an error, it's
             * a special case.
             */
            auto stmt = pastmt(self, oper->scope);
            if (stmt) {
               lst->next = stmt;
               lst = stmt;
            }

            if (!eol(self, &tok)) {
               if (!expect(self, nullptr, mTOK_RBRACE)) {
                  mferro(tok.loc, "Expected newline or '}'.");
                  skipuntil(self, mTOK_EOL);
               }
               break;
            }
         }
      }
      break;
   case mTOK_LBRCKT:
      /*
       * This expression can be
       * a structure or an array:
       *
       * structures, are lists of
       * declarations, like:
       *
       * [field1: type, field2 := 10]
       *
       * arrays, are lists of
       * expressions, like:
       *
       * [10, 20, 30, 50]
       */
      advance(self);
      tok = nxtvalid(self);

      if (
         tok.kind == mTOK_RBRCKT ||
         (tok.kind == mTOK_ID &&
            nxt(self).kind == mTOK_COLON)
      ) {  // It's a struct.
         ret->kind = mEXPR_STRUCT;
         auto scope = mscope_new();

         if (!expect(self, &tok, mTOK_RBRCKT)) {
getfield:
            auto field = padecl(self);
            if (!field->kind) {
               mdecl_del(field);
               mferro(tok.loc, "Expected field declaration.");
               skipuntil(self, mTOK_RBRCKT);
            } else {
               declare(scope, field);

               if (expect(self, &tok, mTOK_COMMA)) {
                  nxtvalid(self);
                  goto getfield;
               }

               if (!expect(self, &tok, mTOK_RBRCKT)) {
                  mferro(tok.loc, "Expected ']'.");
                  skipuntil(self, mTOK_RBRCKT);
               }
            }
         }

         ret->as.struc.scope = scope;
      } else {  // It's an array.
         ret->kind = mEXPR_ARRAY;

         struct mexpr *lst = paexpr(self, 0);
         ret->as.array.list = lst;
         while (expect(self, &tok, mTOK_COMMA)) {
            lst->next = paexpr(self, 0);
            lst = lst->next;
         }

         if (!expect(self, &tok, mTOK_RBRCKT)) {
            mferro(tok.loc, "Expected ']'.");
            skipuntil(self, mTOK_RBRCKT);
         }
      }
      break;
   case mTOK_ID:
      ret->kind = mEXPR_DECL_REF;
      advance(self);
      ret->as
         .decl_ref.declid = tok.lit;
      break;
   case mTOK_STRING:
      ret->kind = mEXPR_LIT;
      advance(self);
      ret->as.lit.kind = mLIT_STRING;
      ret->as.lit.as
         .s.str = tok.lit;
      ret->as.lit.as
         .s.len = tok.data;
      break;
   case mTOK_RUNE:
      ret->kind = mEXPR_LIT;
      advance(self);
      ret->as.lit.kind = mLIT_RUNE;
      ret->as.lit.as
         .r = tok.data;
      break;
   case mTOK_INTEGER:
      ret->kind = mEXPR_LIT;
      advance(self);
      ret->as.lit.kind = mLIT_INTEGER;
      ret->as.lit.as
         .uneva.buf = tok.lit;
      ret->as.lit.as
         .uneva.base = tok.data;
      break;
   case mTOK_FLOAT:
      ret->kind = mEXPR_LIT;
      advance(self);
      ret->as.lit.kind = mLIT_FLOAT;
      ret->as.lit.as
         .uneva.buf = tok.lit;
      break;
   default:
      mferro(tok.loc, "Expected expression.");
      ret->kind = mEXPR_INVAL;
      return ret;
   }

   return ret;
}

/*
 * Should return nullptr
 * if no expression
 * exists.
 */
static struct mexpr *led(
   struct parser *self,
   struct mexpr *left,
   int prec
) {
   auto tok = cur(self);
   assert(tok.kind < mTOK_MAX);

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .loc = tok.loc
   };

   constexpr int ptable[] = {
      [mTOK_LOR] = 10,

      [mTOK_LAND] = 20,

      [mTOK_BOR] = 30,

      [mTOK_EOR] = 40,

      [mTOK_AND] = 50,

      [mTOK_EQL] = 60,
      [mTOK_NEQ] = 60,
      [mTOK_GTR] = 60,
      [mTOK_LSS] = 60,
      [mTOK_GEQ] = 60,
      [mTOK_LEQ] = 60,

      [mTOK_ADD] = 70,
      [mTOK_SUB] = 70,

      [mTOK_MUL] = 80,
      [mTOK_DIV] = 80,
      [mTOK_MOD] = 80,

      [mTOK_LPAREN] = 100
   };

   if (ptable[tok.kind] <= prec) {
      goto fail;
   }
   prec = ptable[tok.kind];

   switch (tok.kind) {
   case mTOK_AND:
      ret->as.bin_op
         .kind = mBIN_OP_AND;
      goto binop;
   case mTOK_LAND:
      ret->as.bin_op
         .kind = mBIN_OP_LAND;
      goto binop;
   case mTOK_BOR:
      ret->as.bin_op
         .kind = mBIN_OP_BOR;
      goto binop;
   case mTOK_LOR:
      ret->as.bin_op
         .kind = mBIN_OP_LOR;
      goto binop;
   case mTOK_EQL:
      ret->as.bin_op
         .kind = mBIN_OP_EQL;
      goto binop;
   case mTOK_NEQ:
      ret->as.bin_op
         .kind = mBIN_OP_NEQ;
      goto binop;
   case mTOK_GTR:
      ret->as.bin_op
         .kind = mBIN_OP_GTR;
      goto binop;
   case mTOK_LSS:
      ret->as.bin_op
         .kind = mBIN_OP_LSS;
      goto binop;
   case mTOK_GEQ:
      ret->as.bin_op
         .kind = mBIN_OP_GEQ;
      goto binop;
   case mTOK_LEQ:
      ret->as.bin_op
         .kind = mBIN_OP_LEQ;
      goto binop;
   case mTOK_ADD:
      ret->as.bin_op
         .kind = mBIN_OP_ADD;
      goto binop;
   case mTOK_SUB:
      ret->as.bin_op
         .kind = mBIN_OP_SUB;
      goto binop;
   case mTOK_MUL:
      ret->as.bin_op
         .kind = mBIN_OP_MUL;
      goto binop;
   case mTOK_DIV:
      ret->as.bin_op
         .kind = mBIN_OP_DIV;
      goto binop;
   case mTOK_MOD:
      ret->as.bin_op
         .kind = mBIN_OP_MOD;
/* fallthrough */
binop:
      ret->kind = mEXPR_BIN_OP;
      advance(self);

      auto rhs = paexpr(self, prec);
      if (!rhs) {
         mexpr_del(rhs);
         goto fail;
      }

      ret->as.bin_op.lhs = left;
      ret->as.bin_op.rhs = rhs;
      break;
   case mTOK_LPAREN:
      ret->kind = mEXPR_CALL;
      if (left->kind != mEXPR_DECL_REF) {
         mferro(tok.loc, "Calling non object expression.");
      }

      advance(self);
      if (!expect(self, &tok, mTOK_RPAREN)) {
         auto fst = paexpr(self, 0);
         auto lst = fst;
         if (fst) {
            goto fstfail;
         }

         while (expect(self, &tok, mTOK_COMMA)) {
            auto expr = paexpr(self, 0);
            if (!expr) {
               mexpr_del(expr);
               goto fstfail;
            }

            lst->next = expr;
            lst = expr;
         }

         if (!expect(self, &tok, mTOK_RPAREN)) {
            goto fstfail;
         }
         ret->as.call.args = fst;
         break;

fstfail:
         mexpr_del(fst);
         skipuntil(self, mTOK_RPAREN);
         goto fail;
      }

      ret->as.call.decl = left;
      break;
   default:
      /*
       * Don't emit
       * "Expected expression."
       * error here.
       */
      goto fail;
   }

   return ret;

fail:
   mexpr_del(ret);
   return nullptr;
}

static struct mexpr *paexpr(
   struct parser *self,
   int prec
) {
   nxtvalid(self);
   struct mexpr *expr = nud(self);
   if (!expr->kind) {
      return expr;
   }

   while (true) {
      auto r = led(self, expr, prec);
      if (r) {
         expr = r;
         continue;
      }
      break;
   }

   return expr;
}

/* Declarations. */

static struct mdecl *padecl(
   struct parser *self
) {
   auto tok = cur(self);

   struct mdecl *ret = malloc(sizeof *ret);
   *ret = (struct mdecl){
      .loc = tok.loc
   };

   switch (tok.kind) {
   case mTOK_ID:
      switch (nxt(self).kind) {
      case mTOK_COLON:
         ret->kind = mDECL_OBJ;
         ret->id = tok.lit;

         advance(self);
         tok = advance(self);
         ret->type = pahint(self);

         if (expect(self, &tok, mTOK_ASSIGN)) {
            auto expr = paexpr(self, 0);
            if (expr) {
               ret->as.obj.expr = expr;
            }
         }
         break;
      case mTOK_LPAREN:
         ret->kind = mDECL_FUNC;
         ret->id = tok.lit;
         auto func = &ret->as.func;

         advance(self);
         tok = advance(self);

         func->scope = mscope_new();
         if (!expect(self, &tok, mTOK_RPAREN)) {
getparm:
            auto parm = padecl(self);
            if (!parm->kind) {
               mdecl_del(parm);
               mferro(tok.loc, "Expected parameter declaration.");
               skipuntil(self, mTOK_RPAREN);
            } else {
               declare(func->scope, parm);

               if (expect(self, &tok, mTOK_COMMA)) {
                  nxtvalid(self);
                  goto getparm;
               }

               if (!expect(self, &tok, mTOK_RPAREN)) {
                  mferro(tok.loc, "Ecpected ')'.");
                  skipuntil(self, mTOK_RPAREN);
               }
            }
         }

         if (!expect(self, &tok, mTOK_COLON)) {
            mferro(tok.loc, "Expected ':' and result type.");
            skipuntil(self, mTOK_EOL);
            goto inval;
         }
         ret->type = pahint(self);

         if (!eol(self, &tok)) {
            if (expect(self, &tok, mTOK_ASSIGN)) {
               func->expr = paexpr(self, 0);
            } else {
               mferro(tok.loc, "Expected newline or '='.");
               skipuntil(self, mTOK_EOL);
            }
         }
         break;
      default:
         goto inval;
      }
      break;
   default:
inval:
      ret->kind = mDECL_INVAL;
      {
         /* Gives it an ID. */
         static int invalc = 0;
         invalc++;
         char buf[32];
         snprintf(buf, sizeof buf, "!(%.28d)", invalc);
         ret->id = mstrpool_insert(
            self->strpool,
            buf,
            sizeof buf
         );
      }
      return ret;
   }

   return ret;
}

/* Unit */

static struct munit *paunit(
   struct parser *self,
   const char *name
) {
   struct munit *ret = malloc(sizeof *ret);
   auto tok = advance(self);
   *ret = (struct munit){
      .name = name,
      .scope = mscope_new()
   };

   for (;;) {
      tok = cur(self);

      switch (tok.kind) {
      case mTOK_INVAL:
         madeus("Parser found an invalid token.");

      case mTOK_EOF:
         goto end;

      case mTOK_EOL:
         advance(self);
         break;

      default:
         auto decl = padecl(self);
         if (!decl->kind) {
            tok = cur(self);
            mdecl_del(decl);
            mferro(tok.loc, "Expected declaration.");
            skipuntil(self, mTOK_EOL);
            continue;
         }
         declare(ret->scope, decl);
      }
   }

end:
   return ret;
}

struct munit *mparse_unit(
   const char *src,
   struct mstrpool *strpool,
   struct mtymap *tymap
) {
   /* The file exists? */
   FILE *file = fopen(src, "r");
   if (!file) {
      merro(
         "Unabe to load the '%s' unit, system: %s.",
         src,
         strerror(errno)
      );
      return nullptr;
   }

   /* Gets the file size. */
   fseek(file, 0, SEEK_END);
   size_t filesz = ftell(file);

   /* Allocates a buffer and copies the file. */
   char *buf = malloc(filesz + 1);
   fseek(file, 0, SEEK_SET);
   fread(buf, filesz, 1, file);
   buf[filesz] = '\0';

   /* Creates a instance. */
   struct parser self = {
      .strpool = strpool,
      .tymap = tymap,
      .lex = mlexer_new(strpool, src, buf, filesz),
      .buf = buf
   };

   /* Parse! */
   auto unit = paunit(&self, src);
   unit->name = src;

   fclose(file);
   free(buf);
   return unit;
}
