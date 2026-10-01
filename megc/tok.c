/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

static const str toknames[] = {
	[LNONE] = "NONE",

	[LEOF] = "EOL",
	[LEOL] = "EOL",

	[LID] = "ID",

	[LDEF] = "def",
	[LALIAS] = "alias",
	[LMUT] = "mut",
	[LCONST] = "const",
	[LIF] = "if",
	[LOR] = "or",
	[LELSE] = "else",
	[LDEFER] = "defer",
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

	[LRESULT] = "=>",
	[LTILDE] = "~",
	[LDOLLAR] = "$",

	[LEND] = nullptr
};

str tokname(tokk t) {
	return toknames[t];
}
