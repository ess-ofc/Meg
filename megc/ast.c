/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

struct unit *newunit(str id) {
	struct unit *u;

	u = alloc(sizeof *u);
	*u = (struct unit){
		.id = id,
		.s = newscope()
	};
	return u;
}

struct type *newtype(
	struct loc l,
	enum typek k,
	enum qual q
) {
	struct type *t;

	t = alloc(sizeof *t);
	*t = (struct type){
		.l = l,
		.k = k,
		.q = q
	};
	return t;
}

struct decl *newdecl(
	struct loc l,
	enum declk k,
	str id
) {
	struct decl *d;

	d = alloc(sizeof *d);
	*d = (struct decl){
		.l = l,
		.k = k,
		.id = id
	};
	return d;
}

struct expr *newexpr(
	struct loc l,
	enum exprk k
) {
	struct expr *e;

	e = alloc(sizeof *e);
	*e = (struct expr){
		.l = l,
		.k = k,
		.ty = newtype(l, TNONE, QNONE)
	};
	return e;
}

struct stmt *newstmt(
	struct loc l,
	enum stmtk k
) {
	struct stmt *s = alloc(sizeof *s);
	*s = (struct stmt){
		.l = l,
		.k = k
	};
	return s;
}
