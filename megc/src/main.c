/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/ast.h>
#include <megc/diagno.h>
#include <megc/main.h>
#include <megc/parser.h>
#include <megc/strpool.h>

const char *progname = "megc";
const char *ouputname = "a.out";
const char *src = nullptr;

int main(int argc, char *argv[]) {
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

         case 'o':
            if (argname[1] != '\0') {
               merro("Invalid flag name: '%s'.", argv[i]);
               break;
            }

            if (i + 1 < argc) {
               ouputname = argv[++i];
               break;
            }

            merro("Expected output name after '%s'.", argv[i]);
            return 1;

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
   if (!src) {
      merro("No input file.");
      return 1;
   }

   auto strpool = mstrpool_new();
   auto unit = mparse_unit(src, &strpool);
   if (unit) {
      munit_print(unit);
      munit_del(unit);
   }

   mstrpool_del(&strpool);
   return 0;
}
