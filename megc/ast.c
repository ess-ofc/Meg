/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

struct unit *newunit(str id) {
	struct unit *u = alloc(sizeof *u);
	u->id = id;
	u->s = newscope();
	return u;
}

struct type *newtype(
	struct loc l,
	enum typek k,
	enum qual q
) {
	struct type *t = alloc(sizeof *t);
	t->l = l;
	t->k = k;
	t->q = q;
	return t;
}

struct decl *newdecl(
	struct loc l,
	enum declk k,
	str id
) {
	struct decl *d = alloc(sizeof *d);
	d->l = l;
	d->k = k;
	d->id = id;
	return d;
}

struct expr *newexpr(
	struct loc l,
	enum exprk k,
	struct type *ty
) {
	struct expr *e = alloc(sizeof *e);
	e->l = l;
	e->k = k;
	e->ty = ty;
	return e;
}

struct stmt *newstmt(
	struct loc l,
	enum stmtk k
) {
	struct stmt *s = alloc(sizeof *s);
	s->l = l;
	s->k = k;
	s->next = nullptr;
	return s;
}