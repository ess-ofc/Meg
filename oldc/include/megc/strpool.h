/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <malloc.h>
#include <stdint.h>

struct mstrentry {
   uint64_t hash;
   unsigned size;
   char *str;
};

/*
 * This struct is purpused only
 * to store strings and avoid
 * allocating too many memory
 * for them.
 */
struct mstrpool {
   unsigned count;
   unsigned size;  // Capacity/size of the array.
   struct mstrentry *entries;
};

struct mstrpool mstrpool_new();

void mstrpool_del(struct mstrpool *self);

const char *mstrpool_insert(
   struct mstrpool *self,
   const char *str,
   size_t size
);
