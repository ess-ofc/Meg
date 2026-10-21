/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

/* Large includes. */
#include <xxh3.h>

typedef struct mem mem;
struct mem {
	struct mem *next;
	char data[];
};

typedef struct tbuk tbuk;
struct tbuk {
	tbuk *next;
	u64 hs;
	type *t;
};

/* List of memory blocks. */
LOCAL static mem *mlist;
/* Allocation count. */
LOCAL static int allc;
/* Bytes allocated. */
LOCAL static size bytes;

void
meminit() {
}

void
memdnit() {
	if(fdump) {
		info(
			"--Memory Information--\n"
			"  |Blocks allocated: %d\n"
			"  |Bytes allocated: %zu",
			allc,
			bytes
		);
	}

	while(mlist) {
		mem *next = mlist->next;
		free(mlist);

		allc--;
		mlist = next;
	}

	if(allc != 0)
		adeus("Allocator error.");
}

void *
alloc(size n) {
	mem *mem = malloc(n + 8);
	mem->next = mlist;
	mlist = mem;

	bytes += n;
	allc++;
	return mem->data;
}

/* scope */

scope *
newscope() {
	scope *s = alloc(sizeof *s);
	s->fst = nullptr;
	s->lst = nullptr;
	s->maps = 16;
	s->dc = 0;

	size bytes = sizeof *s->map * s->maps;
	s->map = alloc(bytes);
	memset(s->map, 0, bytes);
	return s;
}

static void
scopecheck(scope *s) {
	if((float) s->dc / s->maps < 0.80)
		return;

	auto bukp = s->fst;

	s->lst = nullptr;
	s->dc = 0;
	s->maps *= 4;

	size bytes = sizeof *s->map * s->maps;
	s->map = alloc(bytes);
	memset(s->map, 0, bytes);

	while(bukp) {
		declare(s, bukp->d);
		bukp = bukp->next;
	}
}

void
declare(scope *s, decl *d) {
	scopecheck(s);

	size len = strlen(d->id);
	u64 h = XXH3_64bits(d->id, len);

	size pos = h & (s->maps - 1);
	for(;;) {
		auto p = &s->map[pos];

		if(p->h == h && p->len == len) {
			lerro(d->l, "Redeclaration of '%s'.", d->id);
			lnote(p->d->l, "Previous declaration is here.");
			return;
		}

		if(!p->d) {
			p->d = d;
			p->h = h;
			p->len = len;

			if(s->lst)
				s->lst->next = p;
			else
				s->fst = p;
			s->lst = p;
			d->i = s->dc++;
			return;
		}

		pos = (pos + 1) & (s->maps - 1);
	}
}

decl *
getdecl(scope *s, str id) {
	size len = strlen(id);
	u64 h = XXH3_64bits(id, len);

	size pos = h & (s->maps - 1);
	for(;;) {
		auto p = &s->map[pos];

		if(p->h == h && p->len == len)
			return p->d;

		if(!p->d)
			return nullptr;

		pos = (pos + 1) & (s->maps - 1);
	}
}

/* list */

list *
newlist() {
	list *l = alloc(sizeof *l);
	*l = (list){};
	return l;
}

void
append(list *l, expr *e) {
	l->ec++;
	e->next = nullptr;
	if(!l->fst) {
		l->fst = e;
		l->lst = e;
		return;
	}
	l->lst->next = e;
	l->lst = e;
}
/* type */

LOCAL static tbuk *tymap;
LOCAL static size sz, tc;

[[gnu::always_inline]]
static void
wymix(u64 *h1, u64 h2) {
	__uint128_t r = *h1;
	r *= h2;
	*h1 = (u64) (r ^ (r >> 64));
}

type *
internty(type *t) {
	u64 hash = 0x9e3779b97f4a7c15;

	if(!sz) {
		sz = 64;
		tymap = calloc(
			sz,
			sizeof(tbuk)
		);
	}

	if((double) tc / sz > 0.80) {
		size osz = sz;
		tbuk *omap = tymap;

		tc = 0;
		sz *= 4;
		tymap = calloc(
			sz,
			sizeof(tbuk)
		);

		while(osz--)
			if(omap[osz].t)
				internty(omap[osz].t);
		free(omap);
	}

	if(t->d)
		wymix(&hash, (u64) t->d);

	switch(t->k) {
		dbuk *b;

	case TNONE:
		break;
	case TEXPR:
		adeus("internty() received TEXPR.");
	case TINT:
	case TFLOAT:
	case TBOOL:
		wymix(&hash, t->sz);
		wymix(&hash, t->sign);
		break;
	case TFUNC:
		wymix(
			&hash,
			(u64) internty(t->ty)
		);
		/* fallthrough */
	case TSTRUC:
	case TUNION:
		b = t->s->fst;
		while(b) {
			wymix(
				&hash,
				(u64) internty(b->d->ty)
			);
			wymix(
				&hash,
				(u64) XXH3_64bits(
					b->d->id,
					strlen(b->d->id)
				)
			);
			b = b->next;
		}
		break;
	case TARRAY:
		wymix(&hash, (u64) t->len);
		/* fallthrough */
	case TREF:
	case TSLICE:
	case TTYPE:
		wymix(
			&hash,
			(u64) internty(t->ty)
		);
		break;
	}

	wymix(&hash, t->k ^ 0xe7037ed1a0b428db);
	wymix(&hash, (u64) t->e ^ 0xe7037ed1a0b428db);

	size pos = hash & (sz - 1);
	for(;;) {
		tbuk *b = &tymap[pos];
		if(!b->t) {
			b->hs = hash;
			b->t = t;
			tc++;
			break;
		}

		if(b->hs == hash) {
			t = b->t;
			break;
		}

		pos = (pos + 1) & (sz - 1);
	}

	return t;
}

/* block */

block *
newblock() {
	block *ret = alloc(sizeof *ret);
	*ret = (block){
		.s = newscope()  // For analyser.
	};
	return ret;
}

void
join(block *b, stmt *s) {
	b->sc++;
	s->next = nullptr;
	if(!b->fst) {
		b->fst = s;
		b->lst = s;
		return;
	}
	b->lst->next = s;
	b->lst = s;
}

/* token */

static const str toknames[] = {
	[LNONE] = "NONE",

	[LEOF] = "EOF",
	[LEOL] = "EOL",

	[LID] = "ID",

	[LANEX] = "anex",
	[LBREAK] = "break",
	[LCASE] = "case",
	[LCONST] = "const",
	[LCONTINUE] = "continue",
	[LDEF] = "def",
	[LDEFER] = "defer",
	[LELSE] = "else",
	[LENUM] = "enum",
	[LFALL] = "fall",
	[LFOR] = "for",
	[LGOTO] = "goto",
	[LHOLE] = "hole",
	[LIF] = "if",
	[LOF] = "of",
	[LUNION] = "union",
	[LUSE] = "use",

	[LINTEGER] = "INTEGER",
	[LFLOAT] = "FLOAT",
	[LSTRING] = "STRING",
	[LRUNE] = "RUNE",

	[LADD] = "+",
	[LSUB] = "-",
	[LMUL] = "*",
	[LDIV] = "/",
	[LREM] = "%",
	[LAND] = "&",
	[LBOR] = "|",
	[LEOR] = "^",
	[LNOT] = "!",
	[LLAND] = "&&",
	[LLOR] = "||",
	[LEQL] = "==",
	[LNEQ] = "!=",
	[LGTR] = ">",
	[LLSS] = "<",
	[LGEQ] = ">=",
	[LLEQ] = "<=",
	[LDOT] = ".",

	[LCOMMA] = ",",
	[LCOLON] = ":",
	[LSEMI] = ";",
	[LLPAREN] = "(",
	[LLBRACKT] = "[",
	[LLBRACE] = "{",
	[LRPAREN] = ")",
	[LRBRACKT] = "]",
	[LRBRACE] = "}",

	[LASSIGN] = "=",
	[LRESULT] = "=>",
	[LTILDE] = "~",

	[LEND] = "END"
};

str
tokname(tokk t) {
	return toknames[t];
}
