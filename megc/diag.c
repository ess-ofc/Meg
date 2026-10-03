/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define CIAN "\033[1;36m"
#define GREEN "\033[1;32m"

static u64 errc = 0;

u64 errcount() {
	return errc;
}

static void diag(
	str pfx,
	str msg,
	va_list va
) {
	printf("%s\033[0m: ", pfx);
	vprintf(msg, va);
	puts("");
}

void erro(str msg, ...) {
	va_list va;
	va_start(va);
	errc++;
	diag(RED "error", msg, va);
	va_end(va);
}

void warn(str msg, ...) {
	va_list va;
	va_start(va);
	if (fwerror) {
		errc++;
		diag(RED "error", msg, va);
	} else {
		diag(YELLOW "warning", msg, va);
	}
	va_end(va);
}

void note(str msg, ...) {
	va_list va;
	va_start(va);
	diag(CIAN "note", msg, va);
	va_end(va);
}

void info(str msg, ...) {
	va_list va;
	va_start(va);
	diag(GREEN "info", msg, va);
	va_end(va);
}

static void ldiag(
	loc l,
	str pfx,
	str msg,
	va_list va
) {
	printf(
		"%s - %u,%u | %s\033[0m:\n\t",
		file,
		l.line,
		l.col,
		pfx
	);
	vprintf(msg, va);
	puts("");
}

void lerro(loc l, str msg, ...) {
	va_list va;
	va_start(va);
	errc++;
	ldiag(l, RED "error", msg, va);
	va_end(va);
}

void lwarn(loc l, str msg, ...) {
	va_list va;
	va_start(va);
	if (fwerror) {
		errc++;
		ldiag(l, RED "error", msg, va);
	} else {
		ldiag(l, YELLOW "warning", msg, va);
	}
	va_end(va);
}

void lnote(loc l, str msg, ...) {
	va_list va;
	va_start(va);
	ldiag(l, CIAN "note", msg, va);
	va_end(va);
}

void linfo(loc l, str msg, ...) {
	va_list va;
	va_start(va);
	ldiag(l, GREEN "info", msg, va);
	va_end(va);
}

[[noreturn]]
void adeus(str msg, ...) {
	va_list va;
	va_start(va);
	diag(RED "Panic", msg, va);
	va_end(va);
	abort();
}
