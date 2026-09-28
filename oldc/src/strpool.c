/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/strpool.h>

#include <malloc.h>
#include <xxh3.h>

struct mstrpool mstrpool_new() {
   constexpr size_t INITIAL_SIZE = 4;
   return (struct mstrpool){
      .count = 0,
      .size = 4,
      .entries = calloc(
         INITIAL_SIZE,
         sizeof(struct mstrentry)
      )
   };
}

void mstrpool_del(struct mstrpool *self) {
   for (size_t i = 0; i < self->size; i++) {
      if (self->entries[i].str) {
         free(self->entries[i].str);
      }
   }
   free(self->entries);
   self->entries = nullptr;
}

static struct mstrentry *insert(
   struct mstrpool *self,
   const char *str,
   size_t size,
   uint64_t hash
) {
   size_t pos = hash % self->size;

   auto ent = &self->entries[pos];
   while (true) {
      if (ent->str) {
         if (ent->size == size && ent->hash == hash) {
            return ent;
         }
         pos = (pos + 1) % self->size;
         ent = &self->entries[pos];
      } else {
         char *dest = malloc(size + 1);
         memcpy(dest, str, size);
         dest[size] = '\0';

         ent->hash = hash;
         ent->size = size;
         ent->str = dest;
         self->count++;
         return ent;
      }
   }
}

static void checksize(struct mstrpool *self) {
   if ((double) self->count / self->size <= 0.80) {
      return;
   }

   self->count = 0;

   size_t olds = self->size;
   struct mstrentry *oldp = self->entries;
   self->size *= 3;
   self->entries = calloc(self->size, sizeof(struct mstrentry));

   for (size_t i = 0; i < olds; i++) {
      auto ent = &oldp[i];
      if (ent->str) {
         size_t pos = ent->hash % self->size;
         while (true) {
            struct mstrentry *dent = &self->entries[pos];
            if (dent->str) {
               pos = (pos + 1) % self->size;
               continue;
            }

            self->count++;
            *dent = *ent;
            break;
         }
      }
   }

   free(oldp);
}

const char *mstrpool_insert(
   struct mstrpool *self,
   const char *str,
   size_t size
) {
   checksize(self);

   uint64_t hash = XXH3_64bits(str, size);
   return insert(self, str, size, hash)->str;
}
