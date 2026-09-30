/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

struct ptok {
	enum tok t;
	struct loc loc;
	str lit;
	u64 data;
};

/* Parser state.
 *
 * I decided to use pointers
 * to refer to tokens in tbuf
 * to avoid memory copy
 * everytime I need to update
 * it.
 */
LOCAL static struct {
	struct ptok
		tbuf[2],	 // Token buffer.
		*cur,		 // Ptr to the current tok in tbuf.
		*nxt;		 // Ptr to the next tok in tbuf.
} s;

/* Advances to the next token. */
static void eat(u32 times) {
	while (times--) {
		s.cur->t = lex();
		s.cur->loc = getloc();
		s.cur->lit = getlit();
		s.cur->data = getdata();

		/* Swap the tokens. */
		struct ptok *nxt = s.cur;
		s.cur = s.nxt;
		s.nxt = nxt;
	}
}

static enum tok cur() {
	return s.cur->t;
}

static enum tok peek() {
	return s.nxt->t;
}

/*
 * Gets the data of the
 * current token. Don't
 * store a this pointer
 * for a long time.
 */
static struct ptok *curd() {
	return s.cur;
}

/*
 * Checks out if the current
 * token equals to t, returning
 * true and eating in this case.
 * Note that this function skips
 * every LEOL (end-of-line) token
 * if it differs from t.
 */
static bool expect(enum tok t) {
	enum tok ct;

again:
	ct = cur();

	if (ct != t) {
		if (ct == LEOL) {
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

static struct scope *pscope(enum tok term);
static struct type *ptype();
static struct decl *pdecl();
static struct expr *pexpr(u32 prec);
static struct stmt *pstmt();

static struct type *ptype() {
	struct type *t;

	t = newtype(
		curd()->loc,
		TNONE,
		QNONE
	);

	switch (cur()) {
	case LMUT:
		t->q = QMUT;
		eat(1);
		break;
	case LCONST:
		t->q = QCONST;
		eat(1);
		break;
	default:
	}

	switch (cur()) {
	case LAND:
		/*
		 * References:
		 *
		 * &<type>
		 */
		t->k = TREF;
		eat(1);

		t->u.ref.ty = ptype();
		break;
	case LLBRACKT:
		eat(1);

		if (
			expect(LRBRACKT) ||
			peek() == LCOLON
		) {
			/*
			 * Structures:
			 *
			 * [<scope>]
			 *
			 * []
			 */
			t->k = TSTRUC;

			t->u.struc.s = pscope(LRBRACKT);
		} else {
			t->k = TARRAY;

			if (!expect(LTILDE))
				t->u.array.sz = pexpr(0);

			if (!expect(LRBRACKT))
				lerro(curd()->loc, "Expected ']'.");

			t->u.array.ty = ptype();
		}
		break;
	case LID:
		/*
		 * Declaration reference
		 * type:
		 *
		 * <dref expression>
		 */
		t->k = TDREF;
		t->u.dref.e = pexpr(0);
		break;
	default:
	}

	return t;
}

static struct decl *pdecl() {
	struct ptok *pt;
	struct decl *d;
	str id;

	pt = curd();
	switch (cur()) {
	case LID:
		id = pt->lit;

		switch (peek()) {
		case LCOLON:
			d = newdecl(pt->loc, DOBJ, pt->lit);
			eat(2);

			d->u.obj.ty = ptype();
			break;
		case LLPAREN:
			d = newdecl(pt->loc, DFUNC, pt->lit);
			eat(2);

			if (!expect(LRPAREN))
				d->u.func.s = pscope(LRPAREN);

			if (expect(LCOLON))
				d->u.func.ty = ptype();
			else {
				lerro(curd()->loc, "Expected ':' and result type.");
				d->u.func.ty = newtype(
					curd()->loc,
					TNONE,
					QNONE
				);
			}

			if (expect(LASSIGN))
				d->u.func.e = pexpr(0);
			break;
		default:
			lerro(
				pt->loc,
				"Expected function "
				" or object declaration."
			);
			goto inval;
		}
		break;
	case LDEF:
		adeus("Type definitions unsupported.");
	case LALIAS:
		adeus("Type aliases unsupported.");
	default:
		lerro(pt->loc, "Expected declaration.");
		id = "!Invalid";
		goto inval;
	}

	return d;

inval:
	return newdecl(
		pt->loc,
		DNONE,
		id
	);
}

static struct expr *pexpr(u32 prec) {
	struct expr *e, *l;

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

	e = newexpr(curd()->loc, ENONE);

	prec = NPT[cur()];
	if (prec == 0) {
		lerro(curd()->loc, "Expected expression.");
		return e;
	}

	// Null denotation.
	switch (cur()) {
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
		e->u.un.e = pexpr(prec);
		break;
	case LID:
		e->k = EDREF;
		e->u.dref.id = curd()->lit;
		eat(1);
		break;
	case LSTRING:
		e->k = ESTRING;
		e->u.str.s = curd()->lit;
		e->u.str.n = curd()->data;
		eat(1);
		break;
	case LRUNE:
		e->k = ERUNE;
		e->u.rune = curd()->data;
		eat(1);
		break;
	case LINTEGER:
		e->k = EINTEGER;
		e->u.integer = curd()->data;
		eat(1);
		break;
	case LFLOAT:
		adeus("Floats are unspported.");
	case LLPAREN:
		eat(1);
		e->k = EPAREN;
		e->u.un.e = pexpr(prec);
		if (!expect(LRPAREN))
			lerro(curd()->loc, "Expected ')'.");
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
	u32 nprec = LPT[cur()];
	if (nprec < prec)
		return e;

	prec = nprec;
	l = newexpr(curd()->loc, ENONE);

	switch (cur()) {
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
		l->u.bin.lhs = e;
		l->u.bin.rhs = pexpr(prec);
		break;
	case LLPAREN:
		eat(1);
		l->k = EPAREN;
		l->u.call.func = e;
		if (!expect(LRPAREN)) {
			struct expr *lst;	 // Last arg.

			lst = pexpr(0);
			l->u.call.args = lst;
			while (expect(LCOMMA)) {
				lst->next = pexpr(0);
				lst = lst->next;
			}

			if (!expect(LRPAREN))
				lerro(curd()->loc, "Expected ')'.");
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
static struct stmt *pstmt() {
	adeus("Statements are not done.");
}

static struct scope *pscope(
	enum tok ter  // Terminator.
) {
	struct scope *s;
	struct decl *d;

	s = newscope();
again:
	if (expect(ter))
		return s;

	switch (cur()) {
	case LNONE:
		adeus("parser - 'An LNONE'.");
	case LEOF:
		return s;
	case LCOMMA:
		lerro(
			curd()->loc,
			"Unexpected separator."
		);
		/* fallthrough */
	case LEOL:
		eat(1);
		break;
	case LID:
	case LDEF:
	case LALIAS:
		d = pdecl();
		declare(s, d);

		if (expect(LEOL) || expect(LCOMMA))
			break;
		if (expect(ter))
			return s;

		lerro(
			curd()->loc,
			"Expected a separator or '%s'.",
			tokname(ter)
		);
		/* Attempts to continue. */
		if (d->k != DNONE)
			break;

		/* The state is unknown here. */
		for (;;) {
			if (expect(LEOL) || expect(LCOMMA))
				break;
			else if (cur() == ter)
				return s;

			eat(1);
		}
		break;
	default:
		return s;
	}
	goto again;
}

void parse(struct unit *u) {
	u->s = pscope(LEOF);
	if (u->s->dc == 0) {
		erro("Empty unit.");
		return;
	}
}