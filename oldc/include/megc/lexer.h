/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/strpool.h>
#include <megc/token.h>

struct mlexer {
   struct mstrpool *strpool;
   const char *fname;
   const char *buf;
   unsigned busz;
   unsigned off;
   unsigned line, column;
};

struct mlexer mlexer_new(
   struct mstrpool *strpool,
   const char *fname,
   const char *buf,
   unsigned n
);

struct mtoken mlexer_lex(struct mlexer *self);
