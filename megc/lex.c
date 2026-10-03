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
	loc l = {line, col};

	if (roff >= busz) {
		/* Not an error, it's just EOF. */
		ch = 0;
		return 0;
	}

	size w = runesz(buf[roff]);
	if (w == 0) {
		lerro(l, "Invalid unicode character.");
		return 1;
	}

	if (w > busz - roff) {
		lerro(l, "Invalid rune size.");
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
	loc l = {line, col};

	switch (ch) {
	case '\0':
	case '\n':
		lerro(l, "Incomplete scape.");
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
		lerro(l, "Unknown scape character: '%lc'.", ch);
		next();
		return 0;
	}
}

static tok getrune() {
	tok ret = {
		.k = LRUNE,
		.l = {line, col},
	};

	assert(ch == '\'');
	next();

	switch (ch) {
	case '\0':
	case '\n':
		lerro(ret.l, "Incomplete rune literal.");
		return ret;
	case '\\':
		next();
		ret.data = getscape();
		break;
	default:
		ret.data = ch;
		next();
	}

	if (ch != '\'') {
		while (ch && ch != '\n' && ch != '\'')
			next();
		if (ch != '\'')
			lerro(ret.l, "Unterminated rune literal.");
		else
			lerro(ret.l, "Multicharacter rune literal.");
	} else {
		next();
	}

	return ret;
}

static tok getstr() {
	tok ret = {
		.k = LSTRING,
		.l = {line, col}
	};

	assert(ch == '"');

	char s[16384];
	size len = 0;

	size roff = off;
	next();
	for (;;) {
		switch (ch) {
		case '\0':
		case '\n':
			lerro(ret.l, "Unterminated string literal.");
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
	ret.lit = intern(s, len);
	ret.data = len;
	return ret;
}

static tok getkw(loc l, str id, size n) {
	tok ret = {
		.k = LID,
		.l = l,
		.data = n,
		.lit = id
	};

	constexpr size maxk = 4;

	const struct {
		str id;
		tokk t;
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
			{"alias", LALIAS},
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
		return ret;

	auto kg = tab[n];
	for (size i = 0; i < maxk; i++) {
		if (!kg[i].id)
			break;

		if (!memcmp(kg[i].id, id, n)) {
			ret.k = kg[i].t;
			break;
		}
	}

	return ret;
}

static tok getid() {
	loc l = {line, col};

	assert(isridstart(ch));

	prev();
	size boff = off;	// Begin offset.
	str beg = (str) &buf[boff];

	/* Get the raw ID, from buf. */
	size eoff;	// End offset.
	while (isridcontinue(ch)) {
		eoff = off;
		next();
	}

	size len = eoff - boff;
	return getkw(
		l,
		intern(beg, len),
		len
	);
}

static tok getnum() {
	assert(isrdigit(ch));

	tok ret = {
		.k = LINTEGER,
		.l = {line, col}
	};

	char buf[16384];
	size len = 0;
	bool f = false;		// Is float?
	bool issep = false;	// Previous rune is separator.

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
		issep = true;
		next();
		next();
	}

analyze:
	loc rl;
	i32 val;

	for (;;) {
		rl = ret.l;

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
		case 'f':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
			val = ch - 'a' + 10;
			if (val >= 16 || base != 16)
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
			f = true;
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

		if (ch == ';') {
			if (issep)
				lerro(rl, "Consecutive separators.");

			issep = true;
			next();
			continue;
		}

		break;
	}

end:
	if (issep)
		lerro(rl, "Unnecessary separator.");
	if (f)
		ret.k = LFLOAT;
	ret.lit = intern(buf, len);
	ret.data = base;
	return ret;
}

tok lex() {
	/* Skips every space. */
	while (isrspace(ch))
		next();

	tok ret = {
		.l = {line, col}
	};

	if (ch == '\n') {
		next();
		ret.k = LEOL;
		return ret;
	}

	tokk k = LNONE;
	switch (ch) {
	case '\0':
		k = LEOF;
		break;
	case '\\':
		next();
		while (ch != '\\') {
			if (ch == '\0') {
				lerro(ret.l, "Unterminated comment.");
				break;
			}
			next();
		}
		next();
		return lex();
	case '\'':
		return getrune();
	case '"':
		return getstr();
	case '+':
		next();
		k = LADD;
		break;
	case '-':
		next();
		k = LSUB;
		break;
	case '*':
		next();
		k = LMUL;
		break;
	case '/':
		next();
		k = LDIV;
		break;
	case '%':
		next();
		k = LREM;
		break;
	case '&':
		next();
		k = LAND;
		if (ch == '&') {
			next();
			k = LLAND;
			break;
		}
		break;
	case '|':
		next();
		k = LBOR;
		if (ch == '|') {
			next();
			k = LLOR;
			break;
		}
		break;
	case '^':
		next();
		k = LEOR;
		break;
	case '!':
		next();
		k = LNEG;
		if (ch == '=') {
			next();
			k = LNEQ;
			break;
		}
		break;
	case '=':
		next();
		k = LASSIGN;
		switch (ch) {
		case '=':
			next();
			k = LEQL;
			break;
		case '>':
			next();
			k = LRESULT;
			break;
		}
		break;
	case '>':
		next();
		k = LGTR;
		if (ch == '=') {
			next();
			k = LGEQ;
			break;
		}
		break;
	case '<':
		next();
		k = LLSS;
		if (ch == '=') {
			next();
			k = LLEQ;
			break;
		}
		break;
	case '.':
		next();
		k = LDOT;
		break;
	case ',':
		next();
		k = LCOMMA;
		break;
	case ':':
		next();
		k = LCOLON;
		break;
	case '(':
		next();
		k = LLPAREN;
		break;
	case '[':
		next();
		k = LLBRACKT;
		break;
	case '{':
		next();
		k = LLBRACE;
		break;
	case ')':
		next();
		k = LRPAREN;
		break;
	case ']':
		next();
		k = LRBRACKT;
		break;
	case '}':
		next();
		k = LRBRACE;
		break;
	case '~':
		next();
		k = LTILDE;
		break;
	case '$':
		next();
		k = LDOLLAR;
		break;
	default:
		if (isridstart(ch))
			return getid();

		if (isrdigit(ch))
			return getnum();

		lerro(ret.l, "Unknown character U+%X.", ch);
		ret.k = LNONE;
	}

	ret.k = k;
	return ret;
}
