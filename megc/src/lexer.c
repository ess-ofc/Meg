/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/diagno.h>
#include <megc/lexer.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct mlexer mlexer_new(
   struct mstrpool *strpool,
   const char *fname,
   const char *buf,
   unsigned n
) {
   return (struct mlexer){
      .strpool = strpool,
      .fname = fname,
      .buf = buf,
      .busz = n,
      .line = 1,
      .column = 1
   };
}

static inline char next(struct mlexer *self) {
   if (self->off < self->busz) {
      char c = self->buf[self->off++];
      if (c == '\n') {
         self->line++;
         self->column = 0;
      }

      self->column++;
      return self->buf[self->off];
   }
   return '\0';
}

static inline char cur(struct mlexer *self) {
   if (self->off < self->busz) {
      return self->buf[self->off];
   }
   return '\0';
}

static inline char peek(struct mlexer *self) {
   if (self->off + 1 < self->busz) {
      return self->buf[self->off + 1];
   }
   return '\0';
}

static inline void readuntil(struct mlexer *self, char c) {
   char ch = cur(self);
   while (ch != c) {
      ch = next(self);
      if (ch == '\0') {
         break;
      }
   }
}

static enum mtoken_kind iskeyword(
   const char *id,
   size_t len
) {
   constexpr struct {
      size_t len;
      const char kw[16];
      enum mtoken_kind kind;
   } keywords[] = {
      {4, "type", mTOK_TYPE},
      {3, "let", mTOK_LET},
      {3, "mut", mTOK_MUT},
      {5, "const", mTOK_CONST},
      {5, "defer", mTOK_DEFER},
      {2, "if", mTOK_IF},
      {2, "or", mTOK_OR},
      {4, "else", mTOK_ELSE},
      {3, "for", mTOK_FOR},
      {5, "break", mTOK_BREAK},
      {8, "continue", mTOK_CONTINUE}
   };

   constexpr int arrsz =
      sizeof keywords / sizeof keywords[0];

   for (int i = 0; i < arrsz; i++) {
      if (keywords[i].len == len) {
         if (!memcmp(keywords[i].kw, id, len)) {
            return keywords[i].kind;
         }
      }
   }

   return mTOK_ID;
}

static struct mtoken getid(
   struct mlexer *self,
   struct mloc loc
) {
   const char *beg = &self->buf[self->off];
   size_t len = 0;

   while (self->off < self->busz) {
      char c = self->buf[self->off];
      if (!isalnum(c) && c != '_') {
         break;
      }

      len++;
      self->off++;
   }

   const char *entry = mstrpool_insert(
      self->strpool,
      beg,
      len
   );

   self->column += len;
   return (struct mtoken){
      .kind = iskeyword(beg, len),
      .loc = loc,
      .lit = entry,
      .data = len
   };
}

static char getscape(struct mlexer *self) {
   struct mloc loc = {
      .filename = self->fname,
      .line = self->line,
      .column = self->column
   };

   char c = cur(self);
   next(self);
   switch (c) {
   case '\0':
   case '\n':
      mferro(loc, "Unterminated scape sequence.");
      return 0;
   case '0':
      return '\0';
   case 'n':
      return '\n';
   case 't':
      return '\t';
   case 'a':
      return '\a';
   case 'b':
      return '\b';
   case 'r':
      return '\r';
   case 'v':
      return '\v';
   case 'f':
      return '\f';
   case '\\':
      return '\\';
   case '"':
      return '"';
   case '\'':
      return '\'';
   case '?':
      return '\?';

   default:
      mferro(loc, "Unknown scape sequence: '%c'.", c);
      return 0;
   }
}

static struct mtoken getstr(struct mlexer *self) {
   struct mtoken ret = {
      .loc = {
         .filename = self->fname,
         .line = self->line,
         .column = self->column
      },
      .kind = mTOK_STRING
   };

   char str[16 * 1024];
   size_t len = 0;

   while (true) {
      char c = cur(self);

      if (c == '"') {
         next(self);
         break;
      }

      if (len >= sizeof str) {
         mferro(ret.loc, "String too large.");
         str[sizeof str - 1] = '\0';

         /* skipuntil 'gambiarra'. */
         while (c != '"') {
            c = next(self);
            if (c == '\0' || c == '\n') {
               auto cloc = ret.loc;
               cloc.column = self->column;
               mferro(cloc, "Unterminated string.");
               readuntil(self, '"');  // Tries to find '"'.
               next(self);
               goto inval;
            }
         }
         break;
      }

      switch (c) {
      case '\n':
      case '\0':
         auto cloc = ret.loc;
         cloc.column = self->column;
         mferro(cloc, "Unterminated string literal.");
         readuntil(self, '"');  // Tries to find '"'.
         next(self);
         goto inval;

      case '\\':
         c = getscape(self);
         str[len++] = c;
         break;

      default:
         str[len++] = c;
      }

      next(self);
   }

   // Inserts the string in the pool.
   ret.data = len;
   ret.lit = mstrpool_insert(
      self->strpool,
      str,
      len
   );
   return ret;

inval:
   ret.kind = mTOK_INVAL;
   return ret;
}

static struct mtoken getrune(
   struct mlexer *self
) {
   struct mtoken ret = {
      .kind = mTOK_RUNE,
      .loc = {
         .filename = self->fname,
         .column = self->column,
         .line = self->line
      }
   };

   char c = cur(self);
   switch (c) {
   case '\0':
   case '\n':
      goto unterminated;
   case '\'':
      mferro(ret.loc, "Empty rune literal.");
      ret.data = '\0';
      break;
      ;
   case '\\':
      ret.data = getscape(self);
      c = cur(self);
   default:
      ret.data = c;
      c = next(self);
   }

   if (c != '\'') {
      goto unterminated;
   }

   next(self);
   return ret;

unterminated:
   mferro(ret.loc, "Unterminated rune literal.");
   readuntil(self, '\'');
   return ret;
}

static struct mtoken getnum(
   struct mlexer *self
) {
   struct mtoken ret = {
      .loc = {
         .filename = self->fname,
         .line = self->line,
         .column = self->column - 1
      },
      .kind = mTOK_INTEGER
   };

   char c = cur(self);
   size_t base = 10;
   bool f = false;  // Is float.
   const char *bname = "decimal";

   /* Checks the numeber prefix. */
   if (c == '0') {
      char p = peek(self);
      switch (p) {
      case 'o':
         next(self);
         c = next(self);
         base = 8;
         bname = "octal";
         break;
      case 'b':
         next(self);
         c = next(self);
         base = 2;
         bname = "binary";
         break;
      case 'x':
         next(self);
         c = next(self);
         base = 16;
         bname = "hexadecimal";
         break;
      }
   }
   ret.data = base;

   char buf[1024];
   size_t busz = 0;
   bool issep = false;
   unsigned val = 0;

   for (;; c = next(self)) {
      struct mloc cloc = ret.loc;
      cloc.column = self->column;

      switch (c) {
      case '\0':
      case '\n':
         goto end;

      case '.':
         if (issep) {
            mfwarn(cloc, "Consecutive separators.");
         }
         buf[busz++] = '.';
         next(self);
         f = true;
         goto frac;
      case ';':
         if (issep) {
            mfwarn(cloc, "Consecutive separators.");
         }
         issep = true;
         next(self);
         break;

      case 'E':
         buf[busz++] = 'E';
         next(self);
         f = true;
         goto exp;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         val = c - '0';
         if (val >= base) {
            mferro(cloc, "Invalid digit in %s literal.", bname);
            break;
         }
         buf[busz++] = c;
         break;

      case 'f':
         if (f && base != 16) {
            goto sfx;
         }
         /* fallthrough */
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
         val = 10 + c - 'a';
         if (f) {
            mferro(cloc, "Hex digit in float literal.");
            break;
         }
         if (base != 16) {
            mferro(
               cloc,
               "Invalid digit in %s literal.",
               bname
            );
         }
         buf[busz++] = c;
         break;

      default:
         goto end;
      }
   }

end:
   if (busz == 0) {
      buf[0] = '0';
      buf[1] = '\0';
      busz = 1;
   }

   /* Adds in the sting pool. */
   ret.lit = mstrpool_insert(
      self->strpool,
      buf,
      busz
   );
   ret.kind = f ?
      mTOK_FLOAT :
      mTOK_INTEGER;
   return ret;

exp:
   c = cur(self);
   if (c == '-' || c == '+') {
      buf[busz++] = c;
      c = next(self);
   }

   if (!isdigit(c)) {
      mferro(ret.loc, "Empty exponent.");
      buf[busz++] = '0';
      goto end;
   }

   for (;; c = next(self)) {
      struct mloc cloc = ret.loc;
      cloc.column = self->column;

      switch (c) {
      case '\0':
      case '\n':
         next(self);
         goto end;

      case 'f':
         goto sfx;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         buf[busz++] = c;
         break;

      default:
         goto end;
      }
   }
   goto end;

frac:
   c = cur(self);
   if (!isdigit(c)) {
      mferro(ret.loc, "Empty fractional part.");
      buf[busz++] = '0';
      goto end;
   }

   for (;; c = next(self)) {
      switch (c) {
      case '\0':
      case '\n':
         goto end;

      case 'E':
         buf[busz++] = 'E';
         next(self);
         goto exp;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         buf[busz++] = c;
         break;

      default:
         goto end;
      }
   }
   goto end;

sfx:
   c = cur(self);
   switch (c) {
   case 'i':
   case 'u':
   case 'f':
      c = next(self);
      if (isdigit(c)) {
         buf[busz++] = c;
         c = next(self);
         if (!isdigit(c)) {
            mferro(
               ret.loc,
               "Suffixes must have 2 size"
               " digits or no one."
               " Like 'u32' or 'u'."
            );
            buf[busz++] = 0;
            goto end;
         }
         buf[busz++] = c;
         goto end;
      }
   default:
      goto end;
   }
}

struct mtoken mlexer_lex(struct mlexer *self) {
again:
   struct mtoken ret = {
      .loc = {
         .filename = self->fname,
         .line = self->line,
         .column = self->column
      },
      .kind = mTOK_INVAL
   };

   char c = cur(self);
   if (isspace(c)) {
      while (isspace(c)) {
         if (c == '\n') {
            next(self);
            ret.kind = mTOK_EOL;
            goto end;
         }

         if (self->off < self->busz) {
            c = peek(self);
            next(self);
            continue;
         }
         goto end;
      }

      goto again;
   }

   if (isalpha(c) || c == '_') {
      return getid(self, ret.loc);
   }

   if (isdigit(c)) {
      return getnum(self);
   }

   char ch = '\0';
   switch (c) {
      char tc = 0;

   case '\0':
      ret.kind = mTOK_EOF;
      break;

   case '\\':
      next(self);
      readuntil(self, '\\');
      if (cur(self) != '\\') {
         mferro(ret.loc, "Unterminated comment.");
      }
      next(self);
      goto again;
   case '"':
      next(self);
      return getstr(self);
   case '\'':
      next(self);
      return getrune(self);
   case '+':
      ret.kind = mTOK_ADD;
      break;
   case '-':
      ret.kind = mTOK_SUB;
      break;
   case '*':
      ret.kind = mTOK_MUL;
      break;
   case '/':
      ret.kind = mTOK_DIV;
      break;
   case '%':
      ret.kind = mTOK_MOD;
      break;
   case '&':
      tc = peek(self);
      if (tc == '&') {
         next(self);
         ret.kind = mTOK_LAND;
         break;
      }
      ret.kind = mTOK_AND;
      break;
   case '|':
      tc = peek(self);
      if (tc == '|') {
         next(self);
         ret.kind = mTOK_LOR;
         break;
      }
      ret.kind = mTOK_BOR;
      break;
   case '^':
      ret.kind = mTOK_EOR;
      break;
   case '!':
      ch = peek(self);
      if (ch == '=') {
         next(self);
         ret.kind = mTOK_NEQ;
         break;
      }
      ret.kind = mTOK_NEG;
      break;

   case ',':
      ret.kind = mTOK_COMMA;
      break;
   case ':':
      ret.kind = mTOK_COLON;
      break;
   case ';':
      ret.kind = mTOK_SEMIC;
      break;
   case '(':
      ret.kind = mTOK_LPAREN;
      break;
   case '[':
      ret.kind = mTOK_LBRCKT;
      break;
   case '{':
      ret.kind = mTOK_LBRACE;
      break;
   case ')':
      ret.kind = mTOK_RPAREN;
      break;
   case ']':
      ret.kind = mTOK_RBRCKT;
      break;
   case '}':
      ret.kind = mTOK_RBRACE;
      break;
   case '=':
      ch = peek(self);
      if (ch == '=') {
         next(self);
         ret.kind = mTOK_EQL;
         break;
      }
      if (ch == '>') {
         next(self);
         ret.kind = mTOK_RESULT;
         break;
      }
      ret.kind = mTOK_ASSIGN;
      break;
   case '>':
      ch = peek(self);
      if (ch == '=') {
         next(self);
         ret.kind = mTOK_GEQ;
         break;
      }
      ret.kind = mTOK_GTR;
      break;
   case '<':
      ch = peek(self);
      if (ch == '=') {
         next(self);
         ret.kind = mTOK_LEQ;
         break;
      }
      ret.kind = mTOK_LSS;
      break;

   case '~':
      ret.kind = mTOK_TILDE;
      break;

   default:
      mferro(ret.loc, "Undefined character '%c'.", c);
   }

   next(self);

end:
   return ret;
}
