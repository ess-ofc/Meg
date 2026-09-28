/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <megc/ast.h>
#include <megc/tymap.h>

bool msema_analyze(
   struct munit *unit,
   struct mtymap *tymap
);
