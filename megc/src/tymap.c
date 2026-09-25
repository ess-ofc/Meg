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
      .size = 64,
      .map = calloc(
         64,
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
         free(b->ty);
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

static struct mtype *get(
   struct mtymap *self,
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
         return nullptr;
      }

      pos = (pos + 1) % self->size;
   }
}

static void check(
   struct mtymap *self
) {
   if ((double) self->count / self->size > 0.70) {
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

[[gnu::always_inline]]
static uint64_t wymix(uint64_t h1, uint64_t h2) {
   __uint128_t r = h1;
   r *= h2;
   return (uint64_t) (r ^ (r >> 64));
}

struct mtype *mtymap_set(
   struct mtymap *self,
   struct mtype *type
) {
   check(self);

   /* I found this seed n internet, sorry. */
   constexpr uint64_t seed = 0x9e3779b97f4a7c15;

   uint64_t hash = seed;

   /* Mix all! */
   hash = wymix(hash, type->kind);
   switch (type->kind) {
   case mTYPE_INVAL:
      break;
   case mTYPE_UNA:
      hash = wymix(
         hash,
         XXH3_64bits(
            type->as.una.id,
            strlen(type->as.una.id)
         )
      );
      break;
   case mTYPE_REF:
      hash = wymix(hash, type->as.ref.type);
      break;
   case mTYPE_STRUCT:
      auto b = type->as.struc.scope->fst;
      while (b) {
         hash = wymix(hash, b->decl->type);
         b = b->next;
      }
      break;
   case mTYPE_ARRAY:
      /* Not working well. */
      hash = wymix(hash, type->as.array.type);
      break;
   case mTYPE_SLICE:
      hash = wymix(hash, type->as.slice.type);
      break;
   case mTYPE_FUNC:
      auto p = type->as.func.scope->fst;
      while (p) {
         hash = wymix(hash, p->decl->type);
         p = p->next;
      }
      break;
   }

   auto ret = get(self, hash);
   if (!ret) {
      ret = malloc(sizeof *type);
      *ret = *type;
      ret = set(self, ret, hash);
      /* `type` moved to the map, clear it. */
      *type = (struct mtype){};
   } else {
      /* `type` must mot return. */
      mtype_del(type);
      *type = (struct mtype){};  // For safety.
   }
   return ret;
}
