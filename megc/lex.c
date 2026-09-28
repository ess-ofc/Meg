/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

/* Large includes. */
#include "runetable.h"
#include <string.h>
#include <xxh3.h>

LOCAL static u08 *buf;
LOCAL static size off, busz;
LOCAL static u32 line = 1, col;

LOCAL static str lit;
LOCAL static u64 data;
LOCAL static struct loc loc;
LOCAL static rune ch;

static struct {
	struct strbucket {
		u64 hash;
		size len;
		str s;
	} *map;
	size maps;
	size bukc;
} pool;

static void poolck() {
	if ((float) pool.bukc / pool.maps < 0.80)
		return;

	size olds = pool.maps;
	auto oldm = pool.map;
	pool.maps *= 4;
	pool.bukc = 0;
	pool.map = calloc(
		pool.maps,
		sizeof *pool.map
	);

	for (size i = 0; i < olds; i++)
		if (oldm[i].s)
			intern(oldm[i].s, oldm[i].len);

	free(oldm);
}

str intern(str s, size n) {
	poolck();

	u64 hash = XXH3_64bits(s, n);
	size pos = hash & (pool.maps - 1);

	for (;;) {
		auto b = &pool.map[pos];

		if (
			b->hash == hash &&
			b->len == n
		) {
			return b->s;
		}

		if (b->s == nullptr) {
			char *buf = alloc(n + 1);
			memcpy(buf, s, n);
			buf[n] = 0;

			b->hash = hash;
			b->len = n;
			b->s = buf;
			pool.bukc++;
			return buf;
		}

		pos = (pos + 1) & (pool.maps - 1);
	}
}

str getlit() {
	return lit;
}

u64 getdata() {
	return data;
}

static size runesz(u08 byte) {
	if ((byte & 0x80) == 0x00)
		return 1;
	if ((byte & 0xe0) == 0xc0)
		return 2;
	if ((byte & 0xf0) == 0xe0)
		return 3;
	if ((byte & 0xf8) == 0xf0)
		return 4;

	return 0;
}

static size utf8tor(size roff, rune *r) {
	/* In case error, return 1 to avoid loops. */

	if (roff >= busz) {
		/* Not an error, it's just EOF. */
		ch = 0;
		return 0;
	}

	size w = runesz(buf[roff]);
	if (w == 0) {
		lerro(loc, "Invalid unicode character.");
		return 1;
	}

	if (w > busz - roff) {
		lerro(loc, "Invalid rune size.");
		return 1;
	}

	if (roff >= busz) {
		*r = 0;
		return 1;
	}

	*r = 0;
	u08 *p = &buf[roff];
	switch (w) {
	case 1:
		*r = *p;
		break;
	case 2:
		*r = ((u32) p[0] & 0x1f) << 6 |
			((u32) p[1] & 0x3f);
		break;
	case 3:
		*r = ((u32) p[0] & 0x0f) << 12 |
			((u32) p[1] & 0x3f) << 6 |
			((u32) p[2] & 0x3f);
		break;
	case 4:
		*r = ((u32) p[0] & 0x07) << 18 |
			((u32) p[1] & 0x3f) << 12 |
			((u32) p[2] & 0x3f) << 6 |
			((u32) p[3] & 0x3f);
		break;
	}

	return w;
}

static void next() {
	off += utf8tor(off, &ch);
	if (ch == '\n') {
		line++;
		col = 0;
		return;
	}
	col++;
}

static void prev() {
	if (ch == '\n' || col < 1)
		adeus("Attempted to go one line back.");

	while (off)
		if (runesz(buf[--off]) != 0)
			break;

	if (runesz(buf[off]) == 0)
		adeus("Is '%s' a valid UTF-8 file?", file);

	utf8tor(off, &ch);
	col--;
}

static rune peek() {
	rune r = 0;
	utf8tor(off, &r);
	return r;
}

error lexinit() {
	FILE *f = fopen(file, "r");
	if (!f) {
		erro(
			"Couldn't open the file"
			"'%s'; system: '%s'.",
			file,
			strerror(errno)
		);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	busz = ftell(f);
	if (busz == 0) {
		erro("'%s' is empty.", file);
		fclose(f);
		return 1;
	}

	buf = alloc(busz + 1);
	fseek(f, 0, SEEK_SET);
	fread(buf, busz, 1, f);
	buf[busz] = '\0';

	fclose(f);

	/* Initializes the string poool. */
	pool.maps = 64;
	pool.map = calloc(
		pool.maps,
		sizeof(*pool.map)
	);

	next();	// Get the first rune.
	return 0;
}

void lexdnit() {
	free(pool.map);
	pool.map = nullptr;
	pool.bukc = 0;
	pool.maps = 0;
}

static rune getscape() {
	switch (ch) {
	case '\0':
	case '\n':
		lerro(loc, "Incomplete scape.");
		return 0;
	case '0':
		next();
		return '\0';
	case 'n':
		next();
		return '\n';
	case 't':
		next();
		return '\t';
	case 'a':
		next();
		return '\a';
	case 'b':
		next();
		return '\b';
	case 'r':
		next();
		return '\r';
	case 'v':
		next();
		return '\v';
	case 'f':
		next();
		return '\f';
	case '\\':
		next();
		return '\\';
	case '"':
		next();
		return '"';
	case '\'':
		next();
		return '\'';
	case '?':
		next();
		return '\?';

	default:
		lerro(loc, "Unknown scape character: '%lc'.", ch);
		next();
		return 0;
	}
}

static enum tok getrune() {
	assert(ch == '\'');
	next();

	switch (ch) {
	case '\0':
	case '\n':
		lerro(loc, "Incomplete rune literal.");
		return LRUNE;
	case '\\':
		next();
		data = getscape();
		break;
	default:
		data = ch;
		next();
	}

	if (ch != '\'') {
		while (ch && ch != '\n' && ch != '\'')
			next();
		if (ch != '\'')
			lerro(loc, "Unterminated rune literal.");
		else
			lerro(loc, "Multicharacter rune literal.");
	} else {
		next();
	}

	return LRUNE;
}

static enum tok getstr() {
	assert(ch == '"');

	loc.line = line;
	loc.col = col;

	char s[16384];
	size len = 0;

	size roff = off;
	next();
	for (;;) {
		switch (ch) {
		case '\0':
		case '\n':
			lerro(loc, "Unterminated string literal.");
			goto end;
		case '"':
			next();
			goto end;
		case '\\':
			next();
			roff = off;
			s[len++] = getscape();
			continue;
		default:
			while (roff < off)
				s[len++] = buf[roff++];
			next();
			continue;
		}
	}

end:
	lit = intern(s, len);
	data = len;
	return LSTRING;
}

static enum tok getkw(
	str id,
	size n
) {
	constexpr size maxk = 4;

	const struct {
		str id;
		enum tok t;
	} tab[][maxk] = {
		[2] = {
			{"if", LIF},
			{"or", LOR},
		},
		[3] = {
			{"def", LDEF},
			{"mut", LMUT},
			{"for", LFOR},
		},
		[4] = {
			{"else", LELSE},
		},
		[5] = {
			{"const", LCONST},
			{"defer", LDEFER},
			{"break", LBREAK},
		},
		[8] = {
			{"continue", LCONTINUE},
		}
	};

	size ts = sizeof tab / sizeof tab[0];
	if (n >= ts)
		return LID;

	auto kg = tab[n];
	for (size i = 0; i < maxk; i++) {
		if (!kg[i].id)
			break;

		if (!memcmp(kg[i].id, id, n))
			return kg[i].t;
	}

	return LID;
}

static enum tok getid() {
	assert(isridstart(ch));
	loc.line = line;
	loc.col = col;

	prev();
	size boff = off;	// Begin offset.
	str beg = (str) &buf[boff];

	/* Get the raw ID, from buf. */
	size eoff;	// End offset.
	while (isridcontinue(ch)) {
		eoff = off;
		next();
	}

	size rawlen = eoff - boff;
	lit = intern(beg, rawlen);
	data = rawlen;
	return getkw(lit, rawlen);
}

static enum tok getnum() {
	assert(isrdigit(ch));

	loc.line = line;
	loc.col = col;
	struct loc rl;	 // Auxiliar location.

	char buf[16384];
	size len = 0;
	bool f = false;  // Is float?

	i32 base = 10;
	str bname = "decimal";
	if (ch == '0') {
		switch (peek()) {
		case 'x':
			base = 16;
			bname = "hexadecimal";
			break;
		case 'o':
			base = 8;
			bname = "octal";
			break;
		case 'b':
			base = 2;
			bname = "binary";
			break;
		default:
			goto analyze;
		}
		next();
		next();
	}

	if (!isrdigit(ch)) {
		rl.line = line;
		rl.col = col;
		lerro(rl, "Empty literal.");

		buf[len++] = '0';
		goto end;
	}

analyze:
	i32 val;
	bool issep = false;	// Previous rune is separator.

	for (;;) {
		rl = loc;

		val = getdigitval(ch);
		if (val != -1) {
			if (val >= base) {
				lerro(rl, "Invalid digit in %s literal.", bname);
				break;
			}

			buf[len++] = '0' + val;
			goto next;
		}

		switch (ch) {
		case 'i':
		case 'u':
			buf[len++] = ch;
			next();
			goto casep;
		case 'f':
			if (f) {
				buf[len++] = ch;
				next();
				goto casep;
			}
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
			val = ch - 'a' + 10;
			if (val >= 16)
				lerro(rl, "Invalid digit in %s literal.", bname);

			buf[len++] = ch;
			break;
		case '.':
			if (f || base != 10)
				lerro(rl, "Unexpected dot.");
			f = true;
			buf[len++] = ch;
			/* fallthrough. */
		case ';':
			if (issep)
				lwarn(rl, "Consecutive separators.");
			issep = true;
			next();
			continue;
		case 'E':
			if (base != 10)
				lerro(rl, "Unexpected E-notation.");
			if (issep)
				lwarn(rl, "Redundant separators.");
			buf[len++] = ch;
			issep = true;
			next();
			goto casee;
		default:
			goto end;
		}

next:
		issep = false;
		next();
	}
	goto end;

casee:
	rl.line = line;
	rl.col = col;

	if (ch == '+' || ch == '-') {
		buf[len++] = ch;
		next();
	}

	if (!isrdigit(ch)) {
		lerro(rl, "Empty exponent.");
		goto end;
	}

	for (;;) {
		rl.col = col;

		if (isrdigit(ch)) {
			val = getdigitval(ch);
			buf[len++] = '0' + val;

			issep = false;
			next();
			continue;
		}

		switch (ch) {
		case 'u':
		case 'i':
		case 'f':
			buf[len++] = ch;
			next();
			goto casep;
		case ';':
			if (issep) {
				lerro(rl, "Consecutive separators.");
			}
			buf[len++] = ch;

			issep = true;
			next();
			continue;
		}

		goto end;
	}

casep:  // casep.
	if (!isrdigit(ch))
		goto end;

	switch (ch) {
	case '0':
		if (peek() == '8')
			break;
		goto end;
	case '1':
		if (peek() == '6')
			break;
		goto end;
	case '3':
		if (peek() == '2')
			break;
		goto end;
	case '6':
		if (peek() == '4')
			break;
		goto end;
	default:
		goto end;
	}

	buf[len++] = ch;
	next();

	buf[len++] = ch;
	next();

end:
	lit = intern(buf, len);
	data = len;
	return f ? LFLOAT : LINTEGER;
}

enum tok lex() {
	data = 0;
	lit = nullptr;

	/* Skips every space. */
	while (isrspace(ch))
		next();

	if (ch == '\n') {
		next();
		loc.col++;
		return LEOL;
	}

	loc.line = line;
	loc.col = col;

	switch (ch) {
	case '\0':
		return LEOF;
	case '\'':
		return getrune();
	case '"':
		return getstr();
	case '+':
		next();
		return LADD;
	case '-':
		next();
		return LSUB;
	case '*':
		next();
		return LMUL;
	case '/':
		next();
		return LDIV;
	case '%':
		next();
		return LREM;
	case '&':
		next();
		if (ch == '&') {
			next();
			return LLAND;
		}
		return LAND;
	case '|':
		next();
		if (ch == '|') {
			next();
			return LLOR;
		}
		return LBOR;
	case '^':
		next();
		return LEOR;
	case '!':
		next();
		if (ch == '=') {
			next();
			return LNEQ;
		}
		return LNEG;
	case '=':
		next();
		switch (ch) {
		case '=':
			next();
			return LEQL;
		case '>':
			next();
			return LRESULT;
		}
		return LASSIGN;
	case '>':
		next();
		if (ch == '=') {
			next();
			return LGEQ;
		}
		return LGTR;
	case '<':
		next();
		if (ch == '=') {
			next();
			return LLEQ;
		}
		return LLSS;
	case '.':
		next();
		return LDOT;
	case ',':
		next();
		return LCOMMA;
	case ':':
		next();
		return LCOLON;
	case '(':
		next();
		return LLPAREN;
	case '[':
		next();
		return LLBRACKT;
	case '{':
		next();
		return LLBRACE;
	case ')':
		next();
		return LRPAREN;
	case ']':
		next();
		return LRBRACKT;
	case '}':
		next();
		return LRBRACE;
	case '~':
		next();
		return LTILDE;
	case '$':
		next();
		return LDOLLAR;
	default:
		if (isridstart(ch))
			return getid();

		if (isrdigit(ch))
			return getnum();

		next();
		lerro(loc, "Unknown character U+%04X.", ch);
		return LNONE;
	}
}
