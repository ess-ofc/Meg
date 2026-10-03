/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

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