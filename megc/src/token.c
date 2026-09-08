/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/token.h>

const char *toknames[] = {
   [mTOK_INVAL] = "1Nv@ĺð",

   [mTOK_EOF] = "EOF",
   [mTOK_EOL] = "EOL",

   [mTOK_DOC] = "DOC",
   [mTOK_ID] = "ID",

   [mTOK_TYPE] = "type",
   [mTOK_LET] = "let",
   [mTOK_NEW] = "new",
   [mTOK_DEL] = "del",
   [mTOK_DEFER] = "defer",
   [mTOK_IF] = "if",
   [mTOK_OR] = "or",
   [mTOK_ELSE] = "else",
   [mTOK_FOR] = "for",
   [mTOK_BREAK] = "break",
   [mTOK_CONTINUE] = "continue",

   [mTOK_INTEGER] = "I",
   [mTOK_FLOAT] = "F",
   [mTOK_BOOL] = "B",
   [mTOK_STRING] = "S",
   [mTOK_CHAR] = "C",

   [mTOK_ADD] = "+",
   [mTOK_SUB] = "-",
   [mTOK_MUL] = "*",
   [mTOK_DIV] = "/",
   [mTOK_MOD] = "%",
   [mTOK_AND] = "&",
   [mTOK_BOR] = "|",
   [mTOK_EOR] = "^",
   [mTOK_NEG] = "!",
   [mTOK_LAND] = "&&",
   [mTOK_LOR] = "||",
   [mTOK_EQL] = "==",
   [mTOK_NEQ] = "!=",
   [mTOK_GTR] = ">",
   [mTOK_LSS] = "<",
   [mTOK_GEQ] = ">=",
   [mTOK_LEQ] = "<=",

   [mTOK_COMMA] = ",",
   [mTOK_COLON] = ":",
   [mTOK_SEMIC] = ";",
   [mTOK_LPAREN] = "(",
   [mTOK_LBRCKT] = "[",
   [mTOK_LBRACE] = "{",
   [mTOK_RPAREN] = ")",
   [mTOK_RBRCKT] = "]",
   [mTOK_RBRACE] = "}",
   [mTOK_ASSIGN] = "=",

   [mTOK_DOLLAR] = "$",
   [mTOK_TILDE] = "~"
};

const char *mget_token_name(enum mtoken_kind kind) {
   return toknames[kind];
}
