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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct parser {
   struct mlexer lex;
   struct mtoken fst, snd;
   const char *buf;
};

static struct mtoken advance(struct parser *self) {
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
      if (
         t.kind != mTOK_EOL &&
         t.kind != mTOK_DOC
      ) {
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
   *tok = nxtvalid(self);
   if (tok->kind == kind) {
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
again:
   *tok = cur(self);
   if (tok->kind == mTOK_DOC) {
      advance(self);
      goto again;
   }

   if (tok->kind == mTOK_EOL) {
      advance(self);
      return true;
   } else {
      return false;
   }
}

/* Concepts. */

static struct mtype *parse_type(struct parser *self) {
   struct mtoken tok;
   struct mtype *ret = malloc(sizeof *ret);
   *ret = (struct mtype){};

   if (expect(self, &tok, mTOK_MUT)) {
      ret->mut = true;
   }

   if (expect(self, &tok, mTOK_ID)) {
      ret->kind = mTYPE_IDENT;
      ret->as.ident.name = tok.lit;
   }

   return ret;
}

/* Expressions. */

struct expr {
   int prec;
   struct mexpr *(*nud)(
      struct parser *self,
      struct mtoken tok
   );
   struct mexpr *(*led)(
      struct parser *self,
      struct mtoken tok,
      struct mexpr *left
   );
};

static struct mexpr *parse_expr(
   struct parser *self,
   int prec
);
static struct mexpr *parse_bin_op(
   struct parser *self,
   struct mtoken tok,
   struct mexpr *expr
);
static struct mexpr *parse_una_op(
   struct parser *self,
   struct mtoken tok
);
static struct mexpr *parse_decl_ref(
   struct parser *self,
   struct mtoken tok
);
static struct mexpr *parse_lit(
   struct parser *self,
   struct mtoken tok
);
static struct mexpr *parse_paren(
   struct parser *self,
   struct mtoken tok
);
static struct mexpr *parse_operation(
   struct parser *self,
   struct mtoken tok
);
static struct mexpr *parse_call(
   struct parser *self,
   struct mtoken tok,
   struct mexpr *expr
);

/*
 * Operator precedence:
 * 999: Literals and declaration references;
 * 100: `()` and `.`;
 *  90: `$`, `&`, `!`, and `+` and `-` unaries;
 *  80: `*`, `/` and `%`;
 *  70: `+` and `-` binaries;
 *  60: `<`, `>`, `<=`, `>=`, `==` and `!=`;
 *  50: `&` bitwise;
 *  40: `^`;
 *  30: `|`;
 *  20: `&&`;
 *  10: `||`;
 */
static struct expr NUD_OPS[mTOK_MAX] = {
   [mTOK_ADD] = {90, parse_una_op, nullptr},
   [mTOK_SUB] = {90, parse_una_op, nullptr},
   [mTOK_AND] = {90, parse_una_op, nullptr},
   [mTOK_NEG] = {90, parse_una_op, nullptr},

   [mTOK_LPAREN] = {100, parse_paren, nullptr},
   [mTOK_LBRACE] = {100, parse_operation, nullptr},

   [mTOK_ID] = {999, parse_decl_ref, nullptr},
   [mTOK_INTEGER] = {999, parse_lit, nullptr},
};

static struct expr LED_OPS[mTOK_MAX] = {
   [mTOK_LOR] = {10, nullptr, parse_bin_op},

   [mTOK_LAND] = {20, nullptr, parse_bin_op},

   [mTOK_BOR] = {30, nullptr, parse_bin_op},

   [mTOK_EOR] = {40, nullptr, parse_bin_op},

   [mTOK_AND] = {50, nullptr, parse_bin_op},

   [mTOK_EQL] = {60, nullptr, parse_bin_op},
   [mTOK_NEQ] = {60, nullptr, parse_bin_op},
   [mTOK_GTR] = {60, nullptr, parse_bin_op},
   [mTOK_LSS] = {60, nullptr, parse_bin_op},
   [mTOK_GEQ] = {60, nullptr, parse_bin_op},
   [mTOK_LEQ] = {60, nullptr, parse_bin_op},

   [mTOK_ADD] = {70, nullptr, parse_bin_op},
   [mTOK_SUB] = {70, nullptr, parse_bin_op},

   [mTOK_MUL] = {80, nullptr, parse_bin_op},
   [mTOK_DIV] = {80, nullptr, parse_bin_op},
   [mTOK_MOD] = {80, nullptr, parse_bin_op},

   [mTOK_LPAREN] = {100, nullptr, parse_call},
};

static struct mstmt *parse_result(
   struct parser *self
);
static struct mstmt *parse_objinit(
   struct parser *self
);
static struct mstmt *parse_assign(
   struct parser *self
);
static struct mstmt *parse_del(
   struct parser *self
);

static struct mexpr *parse_operation(
   struct parser *self,
   struct mtoken tok
) {
   assert(tok.kind == mTOK_LBRACE);
   advance(self);  // Skips the left brace.

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_OPERATION,
      .loc = tok.loc
   };

   if (!eol(self, &tok)) {
      mferro(tok.loc, "Expected new line after '{'.");
      ret->kind = mEXPR_INVAL;
      return ret;
   }

   struct mstmt *fst = nullptr, *lst = fst;

   /* Main statement parsing loop. */
   while (true) {
      tok = cur(self);
      struct mstmt *stmt = nullptr;

      switch (tok.kind) {
      case mTOK_EOL:
         advance(self);
         continue;

      case mTOK_RBRACE:
         advance(self);
         goto end;

      case mTOK_DEL:
         stmt = parse_del(self);
         break;

      case mTOK_ID:
         if (nxt(self).kind == mTOK_COLON) {
            stmt = parse_objinit(self);
         } else if (nxt(self).kind == mTOK_ASSIGN) {
            stmt = parse_assign(self);
         } else {
            goto result;
         }
         break;

      default:
result:
         stmt = parse_result(self);
      }

      if (!fst) {
         fst = stmt;
         lst = fst;
      } else {
         lst->next = stmt;
         lst = stmt;
      }

      if (!eol(self, &tok)) {
         mferro(tok.loc, "Expected newline.");
         skipuntil(self, mTOK_RBRACE);
         goto inval;
      }
      continue;
   }

end:
   ret->as.operation.stmts = fst;
   return ret;

inval:
   if (ret->as.operation.stmts) {
      mstmt_del(ret->as.operation.stmts);
   }
   ret->kind = mEXPR_INVAL;
   return ret;
}

static struct mexpr *parse_call(
   struct parser *self,
   struct mtoken tok,
   struct mexpr *expr
) {
   assert(tok.kind == mTOK_LPAREN);
   advance(self);  // Skips the left paren.

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_CALL,
      .loc = tok.loc,
      .as.call = {
         .decl = expr
      }
   };

   struct mexpr *farg = nullptr;  // Fisrt arg.
   struct mexpr *larg = farg;     // Last arg.
   if (!expect(self, &tok, mTOK_RPAREN)) {
      while (true) {
         if (expect(self, &tok, mTOK_COMMA)) {
            mferro(tok.loc, "Expected argument expression.");
            continue;
         }

         auto arg = parse_expr(self, 0);
         if (!farg) {
            farg = arg;
            larg = farg;
         } else {
            larg->next = arg;
            larg = arg;
         }

         struct mtoken tok;
         if (!expect(self, &tok, mTOK_COMMA)) {
            if (!expect(self, &tok, mTOK_RPAREN)) {
               mferro(tok.loc, "Expected ',' or ')' in argument list.");
               skipuntil(self, mTOK_RPAREN);
               /* Free and invalidate. */
               mexpr_del(expr);
               mexpr_del(arg);
               goto inval;
            }
            break;
         }
      }
   }

   ret->as.call.args = farg;
   return ret;

inval:
   ret->kind = mEXPR_INVAL;
   return ret;
}

static struct mexpr *parse_paren(
   struct parser *self,
   struct mtoken tok
) {
   assert(tok.kind == mTOK_LPAREN);
   advance(self);

   if (cur(self).kind == mTOK_RPAREN) {
      mferro(tok.loc, "Expected expression.");
      return nullptr;
   }

   struct mexpr *expr = parse_expr(self, 0);

   struct mtoken rp;
   if (!expect(self, &rp, mTOK_RPAREN)) {
      mferro(rp.loc, "Expected ')'.");
      skipuntil(self, mTOK_RPAREN);
   }

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_PAREN,
      .loc = tok.loc,
      .as.paren = {
         .child = expr
      }
   };

   return ret;
}

static struct mexpr *parse_decl_ref(
   struct parser *self,
   struct mtoken tok
) {
   assert(tok.kind == mTOK_ID);
   advance(self);

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_DECL_REF,
      .loc = tok.loc,
      .as.decl_ref = {
         .declid = tok.lit
      }
   };

   return ret;
}

static struct mexpr *parse_lit(
   struct parser *self,
   struct mtoken tok
) {
   assert(tok.kind == mTOK_INTEGER);
   [[maybe_unused]]
   auto curt = cur(self);
   advance(self);

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_LIT,
      .loc = tok.loc,
      .as.lit = {
         .kind = mLIT_INTEGER,
         .buf = tok.lit
      }
   };

   return ret;
}

static struct mexpr *parse_bin_op(
   struct parser *self,
   struct mtoken tok,
   struct mexpr *expr
) {
   advance(self);

   enum mbin_op_kind kind = mBIN_OP_INVAL;
   switch (tok.kind) {
   case mTOK_ADD:
      kind = mBIN_OP_ADD;
      break;
   case mTOK_SUB:
      kind = mBIN_OP_SUB;
      break;
   case mTOK_MUL:
      kind = mBIN_OP_MUL;
      break;
   case mTOK_DIV:
      kind = mBIN_OP_DIV;
      break;
   case mTOK_MOD:
      kind = mBIN_OP_MOD;
      break;
   case mTOK_AND:
      kind = mBIN_OP_AND;
      break;
   case mTOK_BOR:
      kind = mBIN_OP_BOR;
      break;
   case mTOK_EOR:
      kind = mBIN_OP_EOR;
      break;
   case mTOK_LAND:
      kind = mBIN_OP_LAND;
      break;
   case mTOK_LOR:
      kind = mBIN_OP_LOR;
      break;
   case mTOK_EQL:
      kind = mBIN_OP_EQL;
      break;
   case mTOK_NEQ:
      kind = mBIN_OP_NEQ;
      break;
   case mTOK_GTR:
      kind = mBIN_OP_GTR;
      break;
   case mTOK_LSS:
      kind = mBIN_OP_LSS;
      break;
   case mTOK_GEQ:
      kind = mBIN_OP_GEQ;
      break;
   case mTOK_LEQ:
      kind = mBIN_OP_LEQ;
      break;
   default:
      madeus("Invalid binary operator.");
   }

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_BIN_OP,
      .loc = tok.loc,
      .as.bin_op = {
         .kind = kind,
         .lhs = expr,
      }
   };

   int prec = LED_OPS[tok.kind].prec;
   ret->as.bin_op.rhs = parse_expr(self, prec);
   if (!ret->as.bin_op.rhs) {
      mferro(cur(self).loc, "Invalid operand.");
      /* Free and invalidate */
      mexpr_del(expr);
      mexpr_del(ret->as.bin_op.rhs);
      goto inval;
   }

   return ret;

inval:
   ret->kind = mEXPR_INVAL;
   return ret;
}

static struct mexpr *parse_una_op(
   struct parser *self,
   struct mtoken tok
) {
   advance(self);

   enum muna_op_kind kind = mUNA_OP_INVAL;
   switch (tok.kind) {
   case mTOK_ADD:
      kind = mUNA_OP_PLUS;
      break;
   case mTOK_SUB:
      kind = mUNA_OP_MINUS;
      break;
   case mTOK_NEG:
      kind = mUNA_OP_NEG;
      break;
   default:
      madeus("Invalid unary operator.");
   }

   struct mexpr *ret = malloc(sizeof *ret);
   *ret = (struct mexpr){
      .kind = mEXPR_UNA_OP,
      .loc = tok.loc,
      .as.una_op = {
         .kind = kind
      }
   };

   int prec = NUD_OPS[tok.kind].prec;
   ret->as.una_op.oprnd = parse_expr(self, prec);
   if (!ret->as.una_op.oprnd) {
      mferro(cur(self).loc, "Invalid operand.");
      /* Free and invalidate. */
      mexpr_del(ret->as.una_op.oprnd);
      goto inval;
   }

   return ret;

inval:
   ret->kind = mEXPR_INVAL;
   return ret;
}

static struct expr nud(struct mtoken tok) {
   if (tok.kind < mTOK_MAX) {
      return NUD_OPS[tok.kind];
   }

   return (struct expr){};
}

static struct expr led(struct mtoken tok) {
   if (tok.kind < mTOK_MAX) {
      return LED_OPS[tok.kind];
   }

   return (struct expr){};
}

static struct mexpr *parse_expr(
   struct parser *self,
   int prec
) {
   auto tok = cur(self);
   struct expr left = nud(tok);
   if (!left.nud) {
      mferro(tok.loc, "Expected expression.");
      return nullptr;
   }

   struct mexpr *expr = left.nud(self, tok);

   while (true) {
      tok = cur(self);
      left = led(tok);
      if (left.prec > prec && left.led) {
         expr = left.led(self, tok, expr);
      } else {
         break;
      }
   }

   return expr;
}

/* Declarations. */

static struct mdecl *parse_objdecl(struct parser *self) {
   /*
    * Object declarations syntax
    * is as follows:
    * [ID][COLON] <type> [ASSIGN: optional] <expr>
    */

   auto tok = cur(self);
   assert(tok.kind == mTOK_ID && "Not an obj decl");

   struct mdecl *ret = malloc(sizeof *ret);
   *ret = (struct mdecl){
      .kind = mDECL_OBJ,
      .id = tok.lit
   };

   /* Skips the id and colon. */
   advance(self);
   if (!expect(self, &tok, mTOK_COLON)) {
      mferro(tok.loc, "Expected colon in object declaration.");
      goto inval;
   }

   ret->type = parse_type(self);
   return ret;

inval:
   ret->kind = mDECL_INVAL;
   return ret;
}

static struct mdecl *parse_initlist(
   struct parser *self,
   enum mtoken_kind ter  // Terminator.
) {
   /*
    * Initialization lists are
    * sequences of declarations
    * comma-separated. It may be
    * default initialized.
    */
   struct mtoken tok = cur(self);
   struct mdecl *fst = nullptr, *lst = fst;
   while (tok.kind == mTOK_ID) {
      auto obj = parse_objdecl(self);
      if (!fst) {
         fst = obj;
         lst = fst;
      } else {
         lst->next = obj;
         lst = obj;
      }

      if (lst->kind == mDECL_INVAL) {
         /* The state may be corrupted, skip. */
         skipuntil(self, ter);
         return fst;
      }

      if (expect(self, &tok, mTOK_COMMA)) {
         tok = nxtvalid(self);
         if (tok.kind == mTOK_ID) {
            continue;
         }

         mfwarn(tok.loc, "Dangling comma.");
      }
      break;
   }

   if (!expect(self, &tok, ter)) {
      mferro(self->fst.loc, "Expected ')'.");
      skipuntil(self, ter);
   }
   return fst;
}

static struct mdecl *parse_func(struct parser *self) {
   assert(
      self->fst.kind == mTOK_ID &&
      self->snd.kind == mTOK_LPAREN &&
      "Not a function decl"
   );

   auto tok = cur(self);

   struct mdecl *ret = malloc(sizeof *ret);
   *ret = (struct mdecl){
      .kind = mDECL_FUNC,
      .id = tok.lit
   };

   advance(self);
   tok = advance(self);

   /* Parses all the parameters if any. */
   if (!expect(self, &tok, mTOK_RPAREN)) {
      ret->as.func.params =
         parse_initlist(self, mTOK_RPAREN);
   }

   tok = cur(self);
   if (tok.kind != mTOK_COLON) {
      mferro(tok.loc, "Expected ':' followed by the result type.");
      skipuntil(self, mTOK_EOL);
      goto inval;
   }
   advance(self);
   ret->type = parse_type(self);

   if (!eol(self, &tok)) {
      if (expect(self, &tok, mTOK_ASSIGN)) {
         ret->as.func.expr = parse_expr(self, 0);
      } else {
         mferro(tok.loc, "Expected the end of the line.");
         skipuntil(self, mTOK_EOL);
      }
   }

   return ret;

inval:
   ret->kind = mDECL_INVAL;
   return ret;
}

/* Statements. */

static struct mstmt *parse_result(
   struct parser *self
) {
   struct mtoken tok;
   struct mstmt *ret = malloc(sizeof *ret);
   *ret = (struct mstmt){
      .kind = mSTMT_RESULT,
      .loc = tok.loc
   };

   auto expr = parse_expr(self, 0);
   if (!expr) {
      skipuntil(self, mTOK_EOL);
      goto inval;
   }

   ret->as.result.expr = expr;
   return ret;

inval:
   ret->kind = mSTMT_INVAL;
   return ret;
}

static struct mstmt *parse_del(
   struct parser *self
) {
   auto tok = cur(self);
   assert(tok.kind == mTOK_DEL);

   struct mstmt *ret = malloc(sizeof *ret);
   *ret = (struct mstmt){
      .kind = mSTMT_DEL,
      .loc = tok.loc
   };

   /* Skips the 'del' keyword. */
   advance(self);

   auto expr = parse_expr(self, 0);
   if (!expr) {
      skipuntil(self, mTOK_EOL);
      goto inval;
   }
   ret->as.del.expr = expr;

   return ret;

inval:
   ret->kind = mSTMT_INVAL;
   return ret;
}

static struct mstmt *parse_objinit(
   struct parser *self
) {
   auto tok = cur(self);
   assert(
      tok.kind == mTOK_ID &&
      nxt(self).kind == mTOK_COLON
   );

   struct mstmt *ret = malloc(sizeof *ret);
   *ret = (struct mstmt){
      .kind = mSTMT_DEF,
      .loc = tok.loc
   };

   ret->as.def.decl = parse_objdecl(self);
   if (!ret->as.def.decl) {
      skipuntil(self, mTOK_EOL);
      goto inval;
   }

   if (expect(self, &tok, mTOK_ASSIGN)) {
      ret->as.def.init = parse_expr(self, 0);
      if (!ret->as.def.init) {
         skipuntil(self, mTOK_EOL);
         goto inval;
      }
   }

   return ret;

inval:
   ret->kind = mSTMT_INVAL;
   return ret;
}

static struct mstmt *parse_assign(
   struct parser *self
) {
   auto tok = cur(self);
   assert(tok.kind == mTOK_ID);

   struct mstmt *ret = malloc(sizeof *ret);
   *ret = (struct mstmt){
      .kind = mSTMT_ASSIGN,
      .loc = tok.loc
   };

   struct mexpr *declref = parse_expr(self, 0);
   if (!declref) {
      skipuntil(self, mTOK_ASSIGN);
   }
   ret->as.assign.decl = declref;

   /* Skips the '='. */
   assert(cur(self).kind == mTOK_ASSIGN);
   tok = advance(self);

   struct mexpr *expr = parse_expr(self, 0);
   if (!expr) {
      skipuntil(self, mTOK_EOL);
      goto inval;
   }
   ret->as.assign.expr = expr;

   return ret;

inval:
   if (ret->as.assign.decl) {
      mexpr_del(ret->as.assign.decl);
   }
   if (ret->as.assign.expr) {
      mexpr_del(ret->as.assign.expr);
   }
   ret->kind = mSTMT_INVAL;
   return ret;
}

/* Unit */

static struct munit *parse_unit(struct parser *self) {
   struct munit *ret = malloc(sizeof *ret);
   auto tok = advance(self);
   *ret = (struct munit){};

   struct mdecl *fst = nullptr, *lst = fst;
   while (true) {
      switch (tok.kind) {
      case mTOK_INVAL:
         madeus("Parser found an invalid token.");

      case mTOK_EOF:
         goto end;

      case mTOK_DOC:
      case mTOK_EOL:
         advance(self);
         break;

      case mTOK_ID:
         /*
          * Declaration syntax is simple:
          * [ID][COLON][...] is object;
          * [ID][LPAREN][...] is function.
          */
         switch (self->snd.kind) {
         case mTOK_COLON:
            mferro(tok.loc, "Objects are not supported yet.");
            skipuntil(self, mTOK_EOL);
            break;

         case mTOK_LPAREN:
            auto fdecl = parse_func(self);
            if (!fst) {
               fst = fdecl;
               lst = fst;
            } else {
               lst->next = fdecl;
               lst = fdecl;
            }
            break;

         default:
            mferro(tok.loc, "Expected colon or left paren.");
            skipuntil(self, mTOK_EOL);
         }
         break;

      default:
         mferro(tok.loc, "Expected declaration.");
         skipuntil(self, mTOK_EOL);
      }

      tok = cur(self);
   }

end:
   ret->decls = fst;
   return ret;
}

bool mparse_unit(const char *src) {
   /* The file exists? */
   FILE *file = fopen(src, "r");
   if (!file) {
      merro(
         "Unabe to load the '%s' unit, system: %s.",
         src,
         strerror(errno)
      );
      return false;
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
   struct mstrpool strpool = mstrpool_new();
   struct parser self = {
      .lex = mlexer_new(&strpool, src, buf, filesz),
      .buf = buf
   };

   /* Parse! */
   auto unit = parse_unit(&self);
   unit->name = src;

   /* Print! */
   munit_print(unit);

   munit_del(unit);
   mstrpool_del(&strpool);
   fclose(file);
   free(buf);
   return true;
}
