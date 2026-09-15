/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

struct mdeclentry {
   struct mdeclentry *next;
   uint64_t hash;
   size_t off, len;
   struct mdecl *decl;
};

/*
 * This struct is developed
 * specifically for semantic
 * scope declaration searching.
 */
struct mdeclmap {
   size_t size, count;
   struct mdeclentry *array;

   /*
    * We the order that the
    * declarations was set.
    * `fst` to the first and
    * `lst` points to the
    * last declaration set.
    */
   struct mdeclentry *fst, *lst;
};

struct mdeclmap mdeclmap_new();

void mdeclmap_del(struct mdeclmap *self);

/* Adds a new key and value. */
bool mdeclmap_set(
   struct mdeclmap *self,
   struct mdecl *decl
);

/* Gets a new value by key. */
struct mdecl *mdeclmap_get(
   struct mdeclmap *self,
   const char *id
);
