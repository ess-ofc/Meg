/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/declmap.h>

#include <malloc.h>
#include <xxh3.h>

struct mdeclmap mdeclmap_new() {
   struct mdeclmap ret;
   ret.size = 4;
   ret.count = 0;
   ret.array = calloc(
      ret.size,
      sizeof(struct mdeclentry)
   );
   return ret;
}

void mdeclmap_del(struct mdeclmap *self) {
   free(self->array);
}

static void checksize(struct mdeclmap *self) {
   if ((float) self->count / self->size > 0.80) {
      /* Old size and array. */
      size_t olds = self->size;
      auto olda = self->array;

      /* Resets the array. */
      self->count = 0;
      self->size *= 2;
      self->array = calloc(
         self->size,
         sizeof(struct mdeclentry)
      );

      /* Remaps every valid entry. */
      for (size_t i = 0; i < olds; i++) {
         if (olda[i].decl) {
            mdeclmap_set(self, olda[i].decl);
         }
      }

      /* Frees the old array. */
      free(olda);
   }
}

struct mdecl *mdeclmap_get(
   struct mdeclmap *self,
   const char *id
) {
   size_t len = strlen(id);
   uint64_t hash = XXH3_64bits(id, len);
   size_t pos = hash % self->size;

   size_t psl = 0;
   auto bukp = &self->array[pos];
   while (true) {
      if (bukp->decl) {
         if (
            bukp->hash == hash &&
            bukp->len == len &&
            strncmp(
               bukp->decl->id,
               id,
               len
            ) == 0
         ) {
            return bukp->decl;
         }

         if (bukp->psl < psl) {
            return nullptr;
         }

         psl++;
         pos = (pos + 1) % self->size;
         bukp = &self->array[pos];
         continue;
      }

      return nullptr;
   }
}

bool mdeclmap_set(
   struct mdeclmap *self,
   struct mdecl *decl
) {
   checksize(self);

   size_t len = strlen(decl->id);
   uint64_t hash = XXH3_64bits(decl->id, len);
   struct mdeclentry buk = {
      .hash = hash,
      .len = len,
      .decl = decl
   };

   size_t pos = hash % self->size;
   auto bukp = &self->array[pos];
   while (true) {
      if (bukp->decl) {
         if (
            bukp->hash == hash &&
            bukp->len == len &&
            strncmp(
               bukp->decl->id,
               decl->id,
               len
            ) == 0
         ) {
            return false;
         }

         if (bukp->psl < buk.psl) {
            auto tmp = *bukp;
            *bukp = buk;
            buk = tmp;
         }
      } else {
         *bukp = buk;
         break;
      }

      buk.psl++;
      pos = (pos + 1) % self->size;
      bukp = &self->array[pos];
   }

   self->count++;
   return true;
}
