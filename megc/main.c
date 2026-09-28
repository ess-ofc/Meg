/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"
#include <stdio.h>

str file;
bool werror;

int main(int c, char **v) {
	setlocale(LC_ALL, "");

	if (c != 2) {
		erro("Expected only the file name.");
		return 1;
	}

	file = v[1];
	if (lexinit())
		return 1;
	enum tok t = lex();
	while (t) {
		fputs(tokname(t), stdout);
		if (t == LEOL)
			puts("");
		else if (t == LEOF)
			break;
		else if (t == LID)
			printf("(%s) ", getlit());
		else if (t == LINTEGER)
			printf("(%s) ", getlit());
		else if (t == LFLOAT)
			printf("(%s) ", getlit());
		else
			putc(' ', stdout);
		t = lex();
	}
	puts("");
	return 0;
}
