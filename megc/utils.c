/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

/* Large includes. */
#include <xxh3.h>

struct mem {
	struct mem *next;
	char data[];
};

/* List of memory blocks. */
LOCAL static struct mem *mlist;
/* Allocation count. */
LOCAL static int allc;
/* Bytes allocated. */
LOCAL static size bytes;

void meminit() {
	/* Nothing for now. */
}

void memdnit() {
	if (fdump) {
		info(
			"--Memory Information--\n"
			"  |Blocks allocated: %d\n"
			"  |Bytes allocated: %zu",
			allc,
			bytes
		);
	}

	while (mlist) {
		struct mem *next = mlist->next;
		free(mlist);

		allc--;
		mlist = next;
	}

	if (allc != 0)
		adeus("Allocator error.");
}

void *alloc(size n) {
	struct mem *mem = malloc(n + 8);
	mem->next = mlist;
	mlist = mem;

	bytes += n;
	allc++;
	return mem->data;
}

/* scope */

scope *newscope() {
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

static void scopecheck(scope *s) {
	if ((float) s->dc / s->maps < 0.80)
		return;

	auto bukp = s->fst;

	s->lst = nullptr;
	s->dc = 0;
	s->maps *= 4;

	size bytes = sizeof *s->map * s->maps;
	s->map = alloc(bytes);
	memset(s->map, 0, bytes);

	while (bukp) {
		declare(s, bukp->d);
		bukp = bukp->next;
	}
}

void declare(scope *s, decl *d) {
	scopecheck(s);

	size len = strlen(d->id);
	u64 h = XXH3_64bits(d->id, len);

	size pos = h & (s->maps - 1);
	for (;;) {
		auto p = &s->map[pos];

		if (p->h == h && p->len == len) {
			lerro(d->l, "Redeclaration of '%s'.", d->id);
			lnote(p->d->l, "Previous declaration is here.");
			return;
		}

		if (!p->d) {
			p->d = d;
			p->h = h;
			p->len = len;

			if (s->lst)
				s->lst->next = p;
			else
				s->fst = p;
			s->lst = p;
			s->dc++;
			return;
		}

		pos = (pos + 1) & (s->maps - 1);
	}
}

decl *getdecl(scope *s, str id) {
	size len = strlen(id);
	u64 h = XXH3_64bits(id, len);

	size pos = h & (s->maps - 1);
	for (;;) {
		auto p = &s->map[pos];

		if (p->h == h && p->len == len)
			return p->d;

		if (!p->d)
			return nullptr;

		pos = (pos + 1) & (s->maps - 1);
	}
}

bool scopeql(scope *x, scope *y) {
	if (x->dc != y->dc)
		return false;

	auto xb = x->fst;
	auto yb = y->fst;
	while (xb && yb) {
		if (!tyeql(xb->d->ty, yb->d->ty))
			return false;
		xb = xb->next;
		yb = yb->next;
	}

	return true;
}

/* list */

list *newlist() {
	list *l = alloc(sizeof *l);
	*l = (list){};
	return l;
}

void append(list *l, expr *e) {
	l->ec++;
	e->next = nullptr;
	if (!l->fst) {
		l->fst = e;
		l->lst = e;
		return;
	}
	l->lst->next = e;
	l->lst = e;
}

bool listeql(list *x, list *y) {
	if (x->ec != y->ec)
		return false;
	expr
		*xe = x->fst,
		*ye = y->fst;
	while (xe) {
		if (!tyeql(xe->ty, ye->ty))
			return false;
	}
	return true;
}

/* type */

type *newtype(loc l, typek k) {
	type *t = alloc(sizeof *t);
	*t = (struct type){
		.l = l,
		.k = k
	};
	return t;
}

bool tyeql(type *x, type *y) {
	if (x->k != y->k)
		return false;

	switch (x->k) {
	case TNONE:
	case TBOOL:
	case TRUNE:
		return true;
	case TINT:
		return x->sz == y->sz &&
			x->sign == y->sign;
	case TFLOAT:
		return x->sz == y->sz;
	case TREF:
		return tyeql(x->ty, y->ty);
	case TFUNC:
		return scopeql(x->s, y->s) &&
			tyeql(x->ty, y->ty);
	case TSTRUC:
		return scopeql(x->s, y->s);
	case TARRAY:
		return x->asz == y->asz &&
			tyeql(x->ty, y->ty);
	case TTYPE:
		return tyeql(x->ty, y->ty);
	}
}

/* block */

block *newblock() {
	block *ret = alloc(sizeof *ret);
	*ret = (block){};
	return ret;
}

void join(block *b, stmt *s) {
	b->sc++;
	s->next = nullptr;
	if (!b->fst) {
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

	[LDEF] = "def",
	[LALIAS] = "alias",
	[LMUT] = "mut",
	[LCONST] = "const",
	[LIF] = "if",
	[LELSE] = "else",
	[LDEFER] = "defer",
	[LHOLE] = "hole",
	[LCASE] = "case",
	[LOF] = "of",
	[LFOR] = "for",
	[LBREAK] = "break",
	[LCONTINUE] = "continue",

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
	[LNEG] = "!",
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
	[LDOLLAR] = "$",

	[LEND] = "END"
};

str tokname(tokk t) {
	return toknames[t];
}
