/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/ast.h>

struct mtybucket {
   uint64_t hash;
   struct mtype *ty;
};

/*
 * A type map, we don't
 * need to copy a type
 * everytime we need it.
 */
struct mtymap {
   size_t size, count;
   struct mtybucket *map;
};

struct mtymap mtymap_new();

void mtymap_del(struct mtymap *self);

struct mtype *mtymap_set(
   struct mtymap *self,
   struct mtype *type
);
