/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

constexpr size TBUFSZ = 4;

/* Parser state.
 *
 * I decided to use pointers
 * to refer to tokens in tbuf
 * to avoid memory copy
 * everytime I need to update
 * it.
 */
LOCAL static struct {
	tok tbuf[TBUFSZ];	 // Token buffer.
} s;

/* Advances to the next token. */
static void
eat(u32 times) {
	while(times--) {
		memcpy(
			s.tbuf,
			&s.tbuf[1],
			sizeof s.tbuf[0] * 3
		);
		s.tbuf[3] = lex();
	}
}

static tokk
cur() {
	return s.tbuf[0].k;
}

static tokk
peek() {
	return s.tbuf[1].k;
}

/*
 * Gets the current token.
 */
static tok
curt() {
	return s.tbuf[0];
}

/*
 * Checks out if the current
 * token equals to t, returning
 * true and eating in this case.
 * Note that this function skips
 * every LEOL (end-of-line) token
 * if it differs from t.
 */
static bool
expect(tokk t) {
again:
	tokk c = cur();

	if(c != t) {
		if(c == LEOL) {
			eat(1);
			goto again;
		}
		return false;
	}

	eat(1);
	return true;
}

void
parserinit() {
	/* Initializes tbuf. */
	eat(TBUFSZ);
}

static bool
isdecl() {
	switch(cur()) {
	case LDEF:
		return true;
	case LID:
		switch(peek()) {
		case LLPAREN:
			return (s.tbuf[2].k == LRPAREN ||
						 s.tbuf[2].k == LID) &&
				s.tbuf[3].k == LCOLON;
		case LCOLON:
			return true;
		default:
		}
	/* fallthrough */
	default:
		return false;
	}
}

static bool
isstmt() {
	switch(cur()) {
	case LRESULT:
	case LIF:
	case LFOR:
	case LDEFER:
	case LHOLE:
	case LCASE:
		return true;
	case LID:
		if(peek() == LCOLON)
			return true;
	/* fallthrough */
	default:
		return false;
	}
}

static scope *pscope(tokk ter);
static list *plist(tokk ter);
static block *pblock(tokk ter);
static type *ptype();
static decl *pdecl();
static expr *pexpr(u32 prec);
static stmt *pstmt();

static type *
ptype() {
	tok t = curt();
	type *h = newtype(TNONE);

	switch(t.k) {
	case LID:
		/*
		 * Type expressions, syntax:
		 *   <expr>
		 *
		 * The expressions should
		 * result in a EDREF to
		 * DTYPE declaration.
		 */
		h->k = TEXPR;
		h->e = pexpr(0);
		break;
	case LAND:
		/*
		 * References, syntax;
		 *   & <hint>
		 */
		eat(1);
		h->k = TREF;
		h->ty = ptype();
		break;
	case LLPAREN:
		/*
		 * Functionals, syntax:
		 *   (<scope>) <hint>
		 */
		eat(1);
		h->k = TFUNC;
		h->s = pscope(LRPAREN);
		h->ty = ptype();
		break;
	case LUNION:
		eat(1);
		h->k = TUNION;
		if(!expect(LLBRACKT)) {
			lerro(t.l, "Expcted '['.");
			h->s = newscope();
			break;
		}
		h->s = pscope(LRBRACKT);
		if(h->s->dc == 0)
			lerro(t.l, "Empty union.");
		break;
	case LLBRACKT:
		eat(1);
		expect(LNONE);	 // Skips any LEOL.

		if(
			cur() == LRBRACKT ||
			isdecl()
		) {
			/*
			 * Structures, syntax:
			 *   [<scope>]
			 */
			h->k = TSTRUC;
			h->s = pscope(LRBRACKT);
			break;
		} else if(cur() == LTILDE) {

			/*
			 * Slices, syntax:
			 *   [~] <hint>
			 */
			h->k = TSLICE;
			eat(1);
			if(!expect(LRBRACKT)) {
				lerro(curt().l, "Expected ']'.");
				break;
			}
			h->ty = ptype();
			break;
		} else {
			/*
			 * Arrays, syntax:
			 *   [<expr>] <hint>
			 */
			h->k = TARRAY;
			h->e = pexpr(0);
			if(!expect(LRBRACKT)) {
				lerro(curt().l, "Expected ']'.");
				break;
			}
			h->ty = ptype();
			break;
		}
	default:
	}

	return h;
}

static decl *
pdecl() {
	decl *d;
	str id;

	tok t = curt();
	switch(cur()) {
	case LID:
		id = t.lit;

		switch(peek()) {
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
			d->ty = ptype();
			if(cur() == LASSIGN) {
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
			d->ty = newtype(TFUNC);
			eat(2);
			d->ty->s = pscope(LRPAREN);
			if(expect(LCOLON))
				d->ty->ty = ptype();
			else {
				t = curt();
				lerro(t.l, "Expected ':' and result type.");
				d->ty->ty = newtype(TNONE);
			}

			if(cur() == LASSIGN) {
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
		/*
		 * Types, syntax:
		 *   def <id>: <hint> = <expr>
		 */
		eat(1);
		d = newdecl(
			t.l,
			DDEF,
			curt().lit
		);
		eat(1);
		if(!expect(LCOLON)) {
			lerro(
				curt().l,
				"Expected ':'."
			);
			break;
		}
		d->ty = newtype(TTYPE);
		d->ty->ty = ptype();
		if(expect(LASSIGN))
			d->e = pexpr(0);
		break;
	default:
		lerro(
			t.l,
			"Expected declaration."
		);
		id = "!Invalid";
		goto inval;
	}

	return d;

inval:
	return newdecl(t.l, DNONE, id);
}

static expr *
pexpr(u32 prec) {
	expr *l;

	expect(LNONE);	 // Skips newlines.

	// Null Dennotation Precedence Table.
	constexpr u32 NPT[LEND] = {
		[LID] = 1,
		[LINTEGER] = 1,
		[LFLOAT] = 1,
		[LSTRING] = 1,
		[LRUNE] = 1,
		[LLPAREN] = 1,
		[LLBRACE] = 1,
		[LLBRACKT] = 1,

		[LADD] = 90,
		[LSUB] = 90,
		[LAND] = 90,
		[LMUL] = 90,
		[LNOT] = 90,
	};

	tok t = curt();
	expr *e = newexpr(t.l, ENONE);

	prec = NPT[t.k];
	if(prec == 0) {
		lerro(t.l, "Expected expression.");
		return e;
	}

	// Null denotation.
	switch(t.k) {
	case LADD:
		e->k = EPLUS;
		goto unop;
	case LSUB:
		e->k = EMINUS;
		goto unop;
	case LAND:
		e->k = EREF;
		goto unop;
	case LMUL:
		e->k = EIND;
		goto unop;
	case LNOT:
		e->k = ENOT;
		/* fallthrough */
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
		if(errno == ERANGE)
			lerro(t.l, "Integer too long.");
		eat(1);
		break;
	case LFLOAT:
		e->k = EFLOAT;
		e->flt = strtod(t.lit, nullptr);
		if(errno == ERANGE)
			lerro(t.l, "Float too long.");
		eat(1);
		break;
	case LLPAREN:
		eat(1);
		e->k = EPAREN;
		e->lhs = pexpr(prec);
		if(!expect(LRPAREN))
			lerro(t.l, "Expected ')'.");
		break;
	case LLBRACE:
		eat(1);
		e->k = EOPER;
		e->b = pblock(LRBRACE);
		break;
	case LLBRACKT:
		eat(1);
		expect(LNONE);	 // Skips newlines.
		if(isdecl()) {
			e->k = ESTRUC;
			e->s = pscope(LRBRACKT);
			break;
		}
		e->k = EARRAY;
		e->ls = plist(LRBRACKT);
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

		[LLPAREN] = 110,
		[LTILDE] = 110,
		[LDOT] = 110,
	};

led:
	t = curt();
	u32 nprec = LPT[t.k];
	if(nprec < prec)
		return e;

	prec = nprec;
	l = newexpr(t.l, ENONE);

	switch(t.k) {
	case LAND:
		l->k = EAND;
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
	case LLOR:
		l->k = ELOR;
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
		/* fallthrough */
binop:
		eat(1);
		l->lhs = e;
		l->rhs = pexpr(prec);
		break;
	case LDOT:
		eat(1);
		l->k = EDOT;
		l->lhs = e;
		if(cur() != LID) {
			lerro(curt().l, "Expected id.");
			l->str = "";
			break;
		}
		l->str = curt().lit;
		eat(1);
		break;
	case LLPAREN:
		eat(1);
		l->k = ECALL;
		l->lhs = e;
		l->ls = plist(LRPAREN);
		break;
	case LLBRACKT:
		eat(1);
		l->k = ESUBS;
		l->lhs = e;
		l->rhs = pexpr(0);
		if(!expect(LRBRACKT))
			lerro(curt().l, "Expected ']'.");
		break;
	case LTILDE:
		eat(1);
		l->k = EINST;
		l->lhs = e;
		if(!expect(LLBRACKT)) {
			lerro(curt().l, "Expected '['.");
			l->ls = newlist();
			break;
		}
		l->ls = plist(LRBRACKT);
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

static stmt *
pstmt() {
	stmt *s;
	expr *e;
	tok t = curt();

	switch(t.k) {
	case LRESULT:
		eat(1);
		s = newstmt(t.l, SRESULT);
		s->e = pexpr(0);
		break;
	case LID:
		if(peek() != LCOLON)
			goto exprs;

		/*
		 * Although stack statements
		 * looks like a object declaration,
		 * they are not. Stack stmts may
		 * be shadowed in the operation.
		 */
		s = newstmt(t.l, SLOCAL);
		s->id = t.lit;
		eat(2);	// eats LID LCOLON
		s->ty = ptype();
		if(cur() == LASSIGN) {
			eat(1);
			s->e = pexpr(0);
		}
		break;
	case LIF:
		eat(1);
		s = newstmt(t.l, SIF);
		s->e = pexpr(0);
		if(!expect(LLBRACE)) {
			lerro(curt().l, "Expected '{'.");
			break;
		}
		s->blk = pblock(LRBRACE);
		if(cur() == LELSE) {
			eat(1);
			if(!expect(LLBRACE)) {
				lerro(curt().l, "Expected '{'.");
				break;
			}
			s->els = pblock(LRBRACE);
		}
		break;
	case LFOR:
		eat(1);
		s = newstmt(t.l, SFOR);
		expect(LNONE);	 // Skips newlines.

		/* First field, initialization. */
		if(!expect(LCOMMA) && isstmt()) {
			s->init = pstmt();
			if(!expect(LCOMMA))
				goto forblk;
		}

		/* Second field, post-instruction. */
		if(isstmt()) {
			s->post = pstmt();
			if(!expect(LCOMMA))
				goto forblk;
		}

		/* Third field, condition. */
		expect(LNONE);	 // Skips newlines.
		if(cur() != LLBRACE)
			s->e = pexpr(0);

forblk:
		if(!expect(LLBRACE)) {
			lerro(curt().l, "Expected '{'.");
			break;
		}
		s->blk = pblock(LRBRACE);
		break;
	case LBREAK:
		s = newstmt(t.l, SBREAK);
		break;
	case LCONTINUE:
		s = newstmt(t.l, SCONTINUE);
		break;
	default:
exprs:
		e = pexpr(0);
		switch(cur()) {
		case LASSIGN:
			eat(1);
			s = newstmt(curt().l, SASSIGN);
			s->lhs = e;
			s->e = pexpr(0);
			break;
		default:
			s = newstmt(curt().l, SEXPR);
			s->e = e;
			break;
		}
	}

	return s;
}

static scope *
pscope(tokk ter) {
	decl *d;
	scope *s = newscope();

again:
	tok t = curt();
	if(t.k == ter) {
		eat(1);
		return s;
	}

	switch(t.k) {
	case LNONE:
		adeus("Received a LNONE.");
	case LEOF:
		lerro(t.l, "Unexpected EOF.");
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
	default:
		d = pdecl();
		declare(s, d);

		if(expect(LEOL) || expect(LCOMMA))
			goto again;
		if(expect(ter))
			return s;

		lerro(
			curt().l,
			"Expected a separator or '%s'.",
			tokname(ter)
		);

		/* The state is unknown here. */
		for(;;) {
			if(cur() == LEOF || expect(ter))
				return s;

			eat(1);
		}
		goto again;
	}
}

static list *
plist(tokk ter) {
	expr *e;
	list *l = newlist();

again:
	tok t = curt();
	if(t.k == ter) {
		eat(1);
		return l;
	}

	switch(t.k) {
	case LNONE:
		adeus("An LNONE.");
	case LEOF:
		lerro(t.l, "Unexpected EOF.");
		return l;
	case LCOMMA:
		/*
		 * Avoid this:
		 *   list := [
		 *      10,
		 *      , <- here, uncessary comma.
		 *      20,
		 *   ]
		 */
		lwarn(
			t.l,
			"Unecessary separator."
		);
		/* fallthrough */
	case LEOL:
		eat(1);
		goto again;
	default:
		e = pexpr(0);
		append(l, e);

		if(expect(LEOL) || expect(LCOMMA))
			goto again;
		if(expect(ter))
			return l;

		lerro(
			curt().l,
			"Expected a separator or '%s'.",
			tokname(ter)
		);

		/* The state is unknown here. */
		for(;;) {
			if(cur() == LEOF || expect(ter))
				return l;

			eat(1);
		}
		goto again;
	}
}

static block *
pblock(tokk ter) {
	stmt *s;
	block *b = newblock();

again:
	tok t = curt();
	if(t.k == ter) {
		eat(1);
		return b;
	}

	switch(t.k) {
	case LNONE:
		adeus("An LNONE.");
	case LEOF:
		lerro(t.l, "Unexpected EOF.");
		return b;
	case LCOMMA:
		lwarn(
			t.l,
			"Unecessary separator."
		);
		/* fallthrough */
	case LEOL:
		eat(1);
		goto again;
	default:
		s = pstmt();
		join(b, s);

		if(expect(LEOL) || expect(LCOMMA))
			goto again;
		if(expect(ter))
			return b;

		lerro(
			curt().l,
			"Expected a separator or '%s'.",
			tokname(ter)
		);

		/* The state is unknown here. */
		for(;;) {
			if(cur() == LEOF || expect(ter))
				return b;

			eat(1);
		}
		goto again;
	}
}

void
parse() {
	u->s = pscope(LEOF);
	if(u->s->dc == 0) {
		erro("Empty unit.");
		return;
	}
}
