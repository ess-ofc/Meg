/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

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
	}
}