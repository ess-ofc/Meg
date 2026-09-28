/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/ast.h>
#include <megc/cgen.h>
#include <megc/diagno.h>
#include <megc/main.h>
#include <megc/parser.h>
#include <megc/sema.h>
#include <megc/strpool.h>

#include <string.h>

const char *progname = "megc";
const char *ouputname = "a.out";
const char *src = nullptr;
static bool
   syntaxonly = false,
   semanticonly = false;

int main(int argc, char *argv[]) {
   bool ok = true;
   progname = argv[0];

   // Parses arguments.
   for (int i = 1; i < argc; i++) {
      // Flags.
      if (argv[i][0] == '-') {
         const char *argname = &argv[i][1];
         switch (argname[0]) {
         case '\0':
            merro("Expected flag name.");
            return 1;

         case 'f':
            if (!strcmp(argv[i], "-fsyntaxonly")) {
               syntaxonly = true;
            } else if (!strcmp(argv[i], "-fsemanticonly")) {
               semanticonly = true;
            } else {
               merro("Undefined flag '%s'.", argv[i]);
               ok = false;
            }
            break;

         default:
            merro("Unknown flag: '%s'.", argv[i]);
         }

         continue;
      }

      // Main source file.
      if (!src) {
         src = argv[i];
         continue;
      } else {
         merro("Too many source files.");
      }
   }

   // Check.
   if (!ok) {
      return 1;
   }

   if (!src) {
      merro("No input file.");
      return 1;
   }

   auto strpool = mstrpool_new();
   auto tymap = mtymap_new();

   auto unit = mparse_unit(src, &strpool, &tymap);
   if (!unit) {
      goto end;
   }
   puts("\nAST after parsing:");
   mprunit(unit);
   if (syntaxonly) {
      goto end;
   }

   ok = msema_analyze(unit, &tymap);
   puts("\nAST after semantic analysis:");
   mprunit(unit);
   if (semanticonly) {
      goto end;
   }

   if (ok && mgeterrc() == 0) {
      mcgen_genunit(unit);
   }

end:
   munit_del(unit);
   mtymap_del(&tymap);
   mstrpool_del(&strpool);
   return 0;
}
