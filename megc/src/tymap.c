/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/tymap.h>

#include <malloc.h>
#include <string.h>
#include <xxh3.h>

struct mtymap mtymap_new() {
   struct mtymap ret = {
      .size = 4,
      .map = calloc(
         4,
         sizeof(struct mtybucket)
      )
   };
   return ret;
}

void mtymap_del(struct mtymap *self) {
   while (self->size--) {
      auto b = &self->map[self->size];
      if (b->ty) {
         mtype_del(b->ty);
      }
   }

   free(self->map);
   self->map = nullptr;
}

static struct mtype *set(
   struct mtymap *self,
   struct mtype *ty,
   uint64_t hash
) {
   /* Finds the bucket. */
   size_t pos = hash % self->size;
   for (;;) {
      auto b = &self->map[pos];

      if (b->ty) {
         if (b->hash == hash) {
            return b->ty;
         }
      } else {
         b->hash = hash;
         b->ty = ty;

         self->count++;
         return b->ty;
      }

      pos = (pos + 1) % self->size;
   }
}

static void check(
   struct mtymap *self
) {
   if ((double) self->count / self->size > 0.90) {
      size_t olds = self->size;
      auto oldmap = self->map;

      self->size *= 4;
      self->map = calloc(
         self->size,
         sizeof(struct mtybucket)
      );

      while (olds--) {
         auto b = &oldmap[olds];
         if (b->ty) {
            set(self, b->ty, b->hash);
         }
      }

      free(oldmap);
   }
}

struct mtype *mtymap_set(
   struct mtymap *self,
   struct mtype *type
) {
   check(self);

   // TODO: Use some alternative to `calloc()`.
   struct mtype *ty = calloc(
      1,
      sizeof *ty
   );

   ty->kind = type->kind;
   /*
    * Copies field by field,
    * we don't need unintialized
    * memory.
    */
   switch (ty->kind) {
   case mTYPE_INVAL:
      break;
   case mTYPE_UNA:
      ty->as.una.id = type->as.una.id;
      ty->as.una.decl = type->as.una.decl;
      break;
   case mTYPE_REF:
      ty->as.ref.type = type->as.ref.type;
      break;
   case mTYPE_STRUCT:
      ty->as.struc.scope = type->as.struc.scope;
      break;
   case mTYPE_ARRAY:
      ty->as.array.size = type->as.array.size;
      ty->as.array.type = type->as.array.type;
      break;
   case mTYPE_SLICE:
      ty->as.slice.type = type->as.slice.type;
      break;
   }

   uint64_t hash = XXH3_64bits(ty, sizeof *ty);
   return set(self, ty, hash);
}
