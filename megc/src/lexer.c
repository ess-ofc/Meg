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
      ch = peek(self);
      next(self);
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
      {3, "new", mTOK_NEW},
      {3, "del", mTOK_DEL},
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
      .lit = entry
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

static const char *getstr(struct mlexer *self) {
   struct mloc loc = {
      .filename = self->fname,
      .line = self->line,
      .column = self->column
   };

   char str[16 * 1024];
   size_t len = 0;

   char c = cur(self);
   while (true) {
      if (c == '"') {
         break;
      }

      if (len >= sizeof str) {
         mferro(loc, "String too large.");
         str[sizeof str - 1] = '\0';

         /* skipuntil 'gambiarra'. */
         while (c != '"') {
            c = next(self);
            if (c == '\0' || c == '\n') {
               auto cloc = loc;
               cloc.column = self->column;
               mferro(cloc, "Unterminated string.");
               readuntil(self, '"');  // Tries to find '"'.
               next(self);
               return nullptr;
            }
         }
         break;
      }

      switch (c) {
      case '\n':
      case '\0':
         auto cloc = loc;
         cloc.column = self->column;
         mferro(cloc, "Unterminated string literal.");
         readuntil(self, '"');  // Tries to find '"'.
         next(self);
         return nullptr;

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
   return mstrpool_insert(
      self->strpool,
      str,
      len
   );
}

static struct mtoken getnum(struct mlexer *self) {
   struct mtoken ret = {
      .loc = {
         .filename = self->fname,
         .line = self->line,
         .column = self->column - 1
      },
      .kind = mTOK_INTEGER
   };

   char c = cur(self);
   int base = 10;
   bool f = false;  // Is float.
   const char *bname = "decimal";

   /* Checks the numeber prefix. */
   if (c == '0') {
      switch (next(self)) {
      case 'o':
         c = next(self);
         base = 8;
         bname = "octal";
         break;
      case 'b':
         c = next(self);
         base = 2;
         bname = "binary";
         break;
      case 'x':
         c = next(self);
         base = 16;
         bname = "hexadecimal";
         break;
      }
   }

   char buf[1024];
   size_t busz = 0;
   bool issep = false;
   int64_t val = 0;
   while (true) {
      auto cloc = ret.loc;
      cloc.column++;

      switch (c) {
      case '\0':
         goto final;

      case '.':
         if (f) {
            mferro(cloc, "Extra '.'.");
         } else {
            buf[busz++] = '.';
         }
         f = true;
         /* throughout */
      case ';':
         if (issep) {
            mferro(cloc, "Consecutive separators.");
         }
         issep = true;
         c = next(self);
         continue;

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
         goto eval;
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
         if (base != 16) {
            mferro(cloc, "Using hexadecimal digits in a %s literal.", bname);
            break;
         }
         val = (c - 'a') + 10;
         /* throughout */
eval:
         if (val >= base) {
            mferro(cloc, "Invalid digit in a %s literal.", bname);
            break;
         }

         buf[busz++] = c;
         break;

      case 'E':
         buf[busz++] = 'e';
         f = true;
         c = next(self);

         /* Appends the signal. */
         if (c == '-' || c == '+') {
            buf[busz++] = c;
            c = next(self);
         }

         while (isdigit(c)) {
            buf[busz++] = c;
            c = next(self);
         }

         /* Checks out if the exponent has digits. */
         c = self->buf[self->off - 1];
         if (c == 'E' || c == '-' || c == '+') {
            mferro(cloc, "Exponent has no digits.");
         }

         /* throughout */
      default:
         goto final;
      }

      if (busz == sizeof buf) {
         mferro(ret.loc, "Number too long.");
         /* skipuntil 'gambiarra'. */
         while (
            isdigit(c) ||
            (base == 16 ?
                  c == 'a' ||
                     c == 'b' ||
                     c == 'c' ||
                     c == 'd' ||
                     c == 'e' ||
                     c == 'f' :
                  false)
         ) {
            if (c == 'E') {
               c = next(self);
               while (
                  isdigit(c) ||
                  c == '+' ||
                  c == '-'
               ) {
                  c = next(self);
               }
               break;
            }

            c = next(self);
         }

         return (struct mtoken){
            .kind = mTOK_INVAL
         };
      }

      c = next(self);
      issep = false;
   }

final:
   /* Trying to not return nullptr. */
   if (busz == 0) {
      buf[0] = 0;
      busz = 1;
   }

   /* Adds in the sting pool. */
   ret.lit = mstrpool_insert(
      self->strpool,
      buf,
      busz
   );
   ret.kind = f ? mTOK_FLOAT : mTOK_INTEGER;
   return ret;
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

   switch (c) {
      char tc = 0;

   case '\0':
      ret.kind = mTOK_EOF;
      break;

   case '\\':
      next(self);
      readuntil(self, '\\');
      ret.kind = mTOK_DOC;
      break;
   case '"':
      next(self);
      ret.kind = mTOK_STRING;
      ret.lit = getstr(self);
      goto end;
   case '\'':
      ret.kind = mTOK_CHAR;
      char ch;

      /* Gets the content. */
      ch = peek(self);
      if (ch == '\\') {  // Is scape.
         next(self);
         ch = getscape(self);
      } else {
         if (ch == '\'') {  // Is the left quote.
            mferro(ret.loc, "Empty char literal.");
            next(self);
            ret.lit = " ";
            break;
         } else if (ch == '\0' || ch == '\n') {  // Is EOF or EOL.
            mferro(ret.loc, "Unterminated char literal.");
            readuntil(self, '\'');  // Tries to find '\''.
            ret.lit = " ";
            break;
         }
      }

      c = peek(self);
      if (c != '\'') {
         mferro(ret.loc, "Incomplete char literal.");
         readuntil(self, '\'');
         ret.lit = " ";
         break;
      }

      ret.lit = mstrpool_insert(
         self->strpool,
         &ch,
         1
      );
      goto end;

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

   case '$':
      ret.kind = mTOK_DOLLAR;
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
