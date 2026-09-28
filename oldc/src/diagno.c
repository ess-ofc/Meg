/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/diagno.h>
#include <megc/main.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int errc = 0;

int mgeterrc() {
   return errc;
}

static inline void print(const char *pfx, const char *str, va_list va) {
   printf("%s - %s:\n    ", progname, pfx);
   vprintf(str, va);
   puts("");
}

static inline void fprint(
   struct mloc loc,
   const char *pfx,
   const char *str,
   va_list va
) {
   printf(
      "%s - %s: %s:%u,%u\n    ",
      progname,
      pfx,
      loc.filename,
      loc.line,
      loc.column
   );
   vprintf(str, va);
   puts("");
}

void madeus(const char *str, ...) {
   va_list va;
   va_start(va);
   print("\033[1;31mPanic!\033[0m", str, va);
   va_end(va);
   abort();
}

#define IMPL_PRINT(name, pfx) \
   void name(const char* str, ...) { \
      va_list va; \
      va_start(va); \
      print(pfx, str, va); \
      va_end(va); \
   }

#define IMPL_FPRINT(name, pfx) \
   void name(struct mloc loc, const char* str, ...) { \
      va_list va; \
      va_start(va); \
      fprint(loc, pfx, str, va); \
      va_end(va); \
   }

void merro(const char *str, ...) {
   va_list va;
   va_start(va);
   print("\033[1;31merror\033[0m", str, va);
   va_end(va);
   errc++;
}

IMPL_PRINT(minfo, "\033[1;32minfo\033[0m")
IMPL_PRINT(mwarn, "\033[1;33mwarn\033[0m")
IMPL_PRINT(mnote, "\033[1;36mnote\033[0m")

void mferro(struct mloc loc, const char *str, ...) {
   va_list va;
   va_start(va);
   fprint(loc, "\033[1;31merror\033[0m", str, va);
   va_end(va);
   errc++;
}

IMPL_FPRINT(mfinfo, "\033[1;32minfo\033[0m")
IMPL_FPRINT(mfwarn, "\033[1;33mwarn\033[0m")
IMPL_FPRINT(mfnote, "\033[1;36mnote\033[0m")
