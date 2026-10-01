/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

/* Parser state.
 *
 * I decided to use pointers
 * to refer to tokens in tbuf
 * to avoid memory copy
 * everytime I need to update
 * it.
 */
LOCAL static struct {
	tok tbuf[2],  // Token buffer.
		*cur,		  // Ptr to the current tok in tbuf.
		*nxt;		  // Ptr to the next tok in tbuf.
} s;

/* Advances to the next token. */
static void eat(u32 times) {
	while (times--) {
		*s.cur = lex();

		/* Swap the tokens. */
		tok *nxt = s.cur;
		s.cur = s.nxt;
		s.nxt = nxt;
	}
}

static tokk cur() {
	return s.cur->k;
}

static tokk peek() {
	return s.nxt->k;
}

/*
 * Gets the current token.
 */
static tok curt() {
	return *s.cur;
}

/*
 * Checks out if the current
 * token equals to t, returning
 * true and eating in this case.
 * Note that this function skips
 * every LEOL (end-of-line) token
 * if it differs from t.
 */
static bool expect(tokk t) {
again:
	tokk c = cur();

	if (c != t) {
		if (c == LEOL) {
			eat(1);
			goto again;
		}
		return false;
	}

	eat(1);
	return true;
}

void parserinit() {
	/* Setup tok pointers. */
	s.cur = &s.tbuf[0];
	s.nxt = &s.tbuf[1];

	/* Initializes tbuf. */
	eat(2);
}

static scope *pscope(tokk term);
static hint *phint();
static decl *pdecl();
static expr *pexpr(u32 prec);
static stmt *pstmt();

static hint *phint() {
	tok t = curt();
	hint *h = newhint(t.l, HNONE);

	switch (t.k) {
	case LID:
		/*
		 * Type expressions, syntax:
		 *   <expr>
		 *
		 * The expressions should
		 * result in a EDREF to
		 * DTYPE declaration.
		 */
		h->k = HEXPR;
		h->e = pexpr(0);
		break;
	case LAND:
		/*
		 * References, syntax;
		 *   & <hint>
		 */
		eat(1);
		h->k = HREF;
		h->ty = phint();
		break;
	case LLPAREN:
		/*
		 * Functionals, syntax:
		 *   (<scope>) <hint>
		 */
		eat(1);
		h->k = HFUNC;
		h->s = pscope(LRPAREN);
		h->ty = phint();
		break;
	case LLBRACKT:
		eat(1);
		expect(LNONE);	 // Skips any LEOL.

		if (
			cur() == LRBRACKT ||
			peek() == LCOLON
		) {
			/*
			 * Structures, syntax:
			 *   [<scope>]
			 */
			h->k = HSTRUC;
			h->s = pscope(LRBRACKT);
			break;
		} else {
			/*
			 * Arrays, syntax:
			 * 1, slice.
			 *   [~] <hint>
			 * 2, sized array.
			 *   [<constexpr>] <hint>
			 */
			h->k = HARRAY;
			if (!expect(LTILDE))
				h->e = pexpr(0);
			if (!expect(LRBRACKT))
				lerro(curt().l, "Expected ']'.");
			h->ty = phint();
			break;
		}
	default:
	}

	return h;
}

static decl *pdecl() {
	decl *d;
	str id;

	tok t = curt();
	switch (cur()) {
	case LID:
		id = t.lit;

		switch (peek()) {
		case LCOLON:
			/*
			 * Objects, syntax:
			 * 1, normal.
			 *   <id>: <hint>
			 * 2, with initializer.
			 *   <id>: <hint> = <expr>
			 */
			d = newdecl(t.l, DOBJ, t.lit);
			eat(2);
			d->h = phint();
			if (cur() == LASSIGN) {
				eat(1);
				d->e = pexpr(0);
			}
			break;
		case LLPAREN:
			/*
			 * Functions, syntax:
			 * 1, declatation.
			 *   <id>(<scope>): <hint>
			 * 2, definition.
			 *   <id>(<scope>): <hint> = <expr>
			 */
			d = newdecl(t.l, DFUNC, t.lit);
			eat(2);
			d->s = pscope(LRPAREN);
			if (expect(LCOLON))
				d->h = phint();
			else {
				t = curt();
				lerro(t.l, "Expected ':' and result type.");
				d->h = newhint(t.l, HNONE);
			}

			if (cur() == LASSIGN) {
				eat(1);
				d->e = pexpr(0);
			}
			break;
		default:
			lerro(
				t.l,
				"Expected function "
				" or object declaration."
			);
			goto inval;
		}
		break;
	case LDEF:
		adeus("Type definitions unsupported.");
	case LALIAS:
		/*
		 * Aliases, syntax:
		 *   alias <id>: <hint>
		 */
		eat(1);
		d = newdecl(t.l, DTYPE, curt().lit);
		eat(1);
		if (!expect(LCOLON)) {
			lerro(curt().l, "Expected ':'.");
			break;
		}
		d->h = phint();
		break;
	default:
		lerro(t.l, "Expected declaration.");
		id = "!Invalid";
		goto inval;
	}

	return d;

inval:
	return newdecl(t.l, DNONE, id);
}

static struct expr *pexpr(u32 prec) {
	expr *l;

	expect(LNONE);	 // Skips newlines.

	// Null Dennotation Precedence Table.
	constexpr u32 NPT[LEND] = {
		[LID] = 1,
		[LINTEGER] = 1,
		[LFLOAT] = 1,
		[LSTRING] = 1,
		[LRUNE] = 1,

		[LADD] = 90,
		[LSUB] = 90,
		[LAND] = 90,
		[LNEG] = 90,

		[LLPAREN] = 100,
		[LLBRACE] = 100
	};

	tok t = curt();
	expr *e = newexpr(t.l, ENONE);

	prec = NPT[t.k];
	if (prec == 0) {
		lerro(t.l, "Expected expression.");
		return e;
	}

	// Null denotation.
	switch (t.k) {
	case LADD:
		e->k = EPLUS;
		goto unop;
	case LSUB:
		e->k = EMINUS;
		goto unop;
	case LAND:
		e->k = EREF;
		goto unop;
	case LNEG:
		e->k = ENEG;
unop:
		eat(1);
		e->lhs = pexpr(prec);
		break;
	case LID:
		e->k = EDREF;
		e->str = t.lit;
		eat(1);
		break;
	case LSTRING:
		e->k = ESTRING;
		e->str = t.lit;
		e->n = t.data;
		eat(1);
		break;
	case LRUNE:
		e->k = ERUNE;
		e->lint = t.data;
		eat(1);
		break;
	case LINTEGER:
		e->k = EINTEGER;
		e->lint = strtoll(
			t.lit,
			nullptr,
			t.data
		);
		if (errno == ERANGE)
			lerro(t.l, "Integer too long.");
		eat(1);
		break;
	case LFLOAT:
		e->k = EFLOAT;
		e->flt = strtod(t.lit, nullptr);
		if (errno == ERANGE)
			lerro(t.l, "Float too long.");
		eat(1);
		break;
	case LLPAREN:
		eat(1);
		e->k = EPAREN;
		e->lhs = pexpr(prec);
		if (!expect(LRPAREN))
			lerro(t.l, "Expected ')'.");
		break;
	case LLBRACE:
		e->k = EOPER;
		adeus("Operations are not supported.");
		break;
	default:
		adeus("Unsupported expression.");
		return e;
	}

	// Left Dennotation Precedence Table.
	constexpr u32 LPT[LEND] = {
		[LLOR] = 10,

		[LLAND] = 20,

		[LBOR] = 30,

		[LEOR] = 40,

		[LAND] = 50,

		[LEQL] = 60,
		[LNEQ] = 60,
		[LGTR] = 60,
		[LLSS] = 60,
		[LGEQ] = 60,
		[LLEQ] = 60,

		[LADD] = 70,
		[LSUB] = 70,

		[LMUL] = 80,
		[LDIV] = 80,
		[LREM] = 80,

		[LLPAREN] = 100
	};

led:
	t = curt();
	u32 nprec = LPT[t.k];
	if (nprec < prec)
		return e;

	prec = nprec;
	l = newexpr(t.l, ENONE);

	switch (t.k) {
	case LLOR:
		l->k = ELOR;
		goto binop;
	case LBOR:
		l->k = EBOR;
		goto binop;
	case LEOR:
		l->k = EEOR;
		goto binop;
	case LLAND:
		l->k = ELAND;
		goto binop;
	case LEQL:
		l->k = EEQL;
		goto binop;
	case LNEQ:
		l->k = ENEQ;
		goto binop;
	case LGTR:
		l->k = EGTR;
		goto binop;
	case LLSS:
		l->k = ELSS;
		goto binop;
	case LGEQ:
		l->k = EGEQ;
		goto binop;
	case LLEQ:
		l->k = ELEQ;
		goto binop;
	case LADD:
		l->k = EADD;
		goto binop;
	case LSUB:
		l->k = ESUB;
		goto binop;
	case LMUL:
		l->k = EMUL;
		goto binop;
	case LDIV:
		l->k = EDIV;
		goto binop;
	case LREM:
		l->k = EREM;
binop:
		eat(1);
		l->lhs = e;
		l->rhs = pexpr(prec);
		break;
	case LLPAREN:
		eat(1);
		l->k = ECALL;
		l->lhs = e;
		if (!expect(LRPAREN)) {
			expr *lst = pexpr(0);

			l->rhs = lst;
			l->n = 1;
			while (expect(LCOMMA)) {
				lst->next = pexpr(0);
				lst = lst->next;
				l->n++;
			}

			t = curt();
			if (!expect(LRPAREN))
				lerro(t.l, "Expected ')'.");
		}
		break;
	default:
		/*
		 * The operator may be in
		 * the LPT, but if the
		 * CPU is here, we didn't
		 * add the support to
		 * this operator.
		 */
		adeus(
			"Token `%s` not supported"
			" as an operator.",
			tokname(cur())
		);
	}

	/* e should be used by l here. */
	e = l;
	goto led;
}

[[maybe_unused]]
static stmt *pstmt() {
	adeus("Statements are not done.");
}

static scope *pscope(tokk ter) {
	decl *d;

	scope *s = newscope();
again:
	tok t = curt();
	if (cur() == ter) {
		eat(1);
		return s;
	}

	switch (t.k) {
	case LNONE:
		adeus("Received a LNONE.");
	case LEOF:
		return s;
	case LCOMMA:
		lwarn(
			t.l,
			"Unecessary separator."
		);
		/* fallthrough */
	case LEOL:
		eat(1);
		goto again;
	case LID:
	case LDEF:
	case LALIAS:
		d = pdecl();
		declare(s, d);

		if (expect(LEOL) || expect(LCOMMA))
			goto again;
		if (expect(ter))
			return s;

		lerro(
			t.l,
			"Expected a separator or '%s'.",
			tokname(ter),
			tokname(cur()),
			curt().lit
		);
		/* Attempts to continue. */
		if (d->k != DNONE)
			goto again;

		/* The state is unknown here. */
		for (;;) {
			if (expect(LEOL) || expect(LCOMMA))
				goto again;
			else if (cur() == ter)
				return s;

			eat(1);
		}
		goto again;
	default:
		return s;
	}
}

void parse(unit *u) {
	u->s = pscope(LEOF);
	if (u->s->dc == 0) {
		erro("Empty unit.");
		return;
	}
}
