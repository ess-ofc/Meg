/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/loc.h>

#include <stdint.h>

enum mtoken_kind {
   mTOK_INVAL = 0,

   mTOK_EOF,
   mTOK_EOL,

   mTOK_DOC,
   mTOK_ID,

   mTOK_TYPE,
   mTOK_LET,
   mTOK_MUT,
   mTOK_NEW,
   mTOK_DEL,
   mTOK_DEFER,
   mTOK_IF,
   mTOK_OR,
   mTOK_ELSE,
   mTOK_FOR,
   mTOK_BREAK,
   mTOK_CONTINUE,

   mTOK_INTEGER,
   mTOK_FLOAT,
   mTOK_BOOL,
   mTOK_STRING,
   mTOK_CHAR,

   mTOK_ADD,
   mTOK_SUB,
   mTOK_MUL,
   mTOK_DIV,
   mTOK_MOD,
   mTOK_AND,
   mTOK_BOR,
   mTOK_EOR,
   mTOK_NEG,
   mTOK_LAND,
   mTOK_LOR,
   mTOK_EQL,
   mTOK_NEQ,
   mTOK_GTR,
   mTOK_LSS,
   mTOK_GEQ,
   mTOK_LEQ,

   mTOK_COMMA,
   mTOK_COLON,
   mTOK_SEMIC,
   mTOK_LPAREN,
   mTOK_LBRCKT,
   mTOK_LBRACE,
   mTOK_RPAREN,
   mTOK_RBRCKT,
   mTOK_RBRACE,
   mTOK_ASSIGN,

   mTOK_DOLLAR,
   mTOK_TILDE,

   mTOK_MAX  // The enum size.
};

struct mtoken {
   struct mloc loc;
   const char *lit;
   enum mtoken_kind kind;
};

const char *mget_token_name(enum mtoken_kind type);
