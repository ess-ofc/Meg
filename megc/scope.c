/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

/* Large includes. */
#include <xxh3.h>

struct scope *newscope() {
	struct scope *s;

	s = alloc(sizeof *s);
	s->fst = nullptr;
	s->lst = nullptr;
	s->maps = 4;
	s->dc = 0;

	size bytes = sizeof *s->map * s->maps;
	s->map = alloc(bytes);
	memset(s->map, 0, bytes);
	return s;
}

static void check(
	struct scope *s
) {
	if ((float) s->dc / s->maps < 0.80)
		return;

	auto bukp = s->fst;

	s->fst = nullptr;
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

void declare(
	struct scope *s,
	struct decl *d
) {
	check(s);

	size len = strlen(d->id);
	u64 h = XXH3_64bits(d->id, len);

	size pos = h & (s->maps - 1);
	for (;;) {
		auto p = &s->map[pos];

		if (p->h == h && p->len == len)
			return;

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

struct decl *getdecl(
	struct scope *s,
	str id
) {
	size len = strlen(id);
	u64 h = XXH3_64bits(id, len);

	size pos = h & (s->maps - 1);
	for (;;) {
		auto p = &s->map[pos];

		if (p->h == h && p->len == len)
			return p->d;

		if (!p->d) {
			return nullptr;
		}

		pos = (pos + 1) & (s->maps - 1);
	}
}