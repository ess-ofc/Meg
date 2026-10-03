/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

unit *newunit(str id) {
	unit *u = alloc(sizeof *u);
	*u = (unit){
		.id = id,
		.s = newscope()
	};
	return u;
}

hint *newhint(loc l, hintk k) {
	hint *h = alloc(sizeof *h);
	*h = (hint){
		.l = l,
		.k = k
	};
	return h;
}

decl *newdecl(loc l, declk k, str id) {
	decl *d = alloc(sizeof *d);
	*d = (decl){
		.l = l,
		.k = k,
		.id = id
	};
	return d;
}

expr *newexpr(loc l, exprk k) {
	expr *e = alloc(sizeof *e);
	*e = (expr){
		.l = l,
		.k = k,
		.ty = newtype(l, TNONE)
	};
	return e;
}

stmt *newstmt(
	loc l,
	stmtk k
) {
	stmt *s = alloc(sizeof *s);
	*s = (stmt){
		.l = l,
		.k = k
	};
	return s;
}
