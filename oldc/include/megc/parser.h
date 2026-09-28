/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/strpool.h>
#include <megc/tymap.h>

struct munit *mparse_unit(
   const char *unit_name,
   struct mstrpool *strpool,
   struct mtymap *tymap
);
