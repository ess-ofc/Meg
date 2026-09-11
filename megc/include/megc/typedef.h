/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

#include <stddef.h>

enum mtypedef_kind {
   mTYPEDEF_INVAL = 0,
   mTYPEDEF_PRIMITIVE
};

struct mtypedef {
   union {
      struct mprimitive {
         size_t size;  // In bytes.
      } primitive;
   } as;
   enum mtypedef_kind kind;
};
