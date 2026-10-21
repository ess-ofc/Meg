/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

enum : u08 {
	NOTOK = 0,
	BREAKOK = 1 << 1,
	CONTOK = 1 << 2,
	FALLOK = 1 << 3
};

typedef struct ctx ctx;
struct ctx {
	expr *oper;	 // Operation.
	stmt *cas;	 // Case, if inside one.
	u08 f;		 // Flags.
};

LOCAL type
	*tnone = &(type){},
	*ti08 = &(type){.k = TINT, .sz = 1, .sign = true},
	*ti16 = &(type){.k = TINT, .sz = 2, .sign = true},
	*ti32 = &(type){.k = TINT, .sz = 4, .sign = true},
	*ti64 = &(type){.k = TINT, .sz = 8, .sign = true},
	*tu08 = &(type){.k = TINT, .sz = 1, .sign = false},
	*tu16 = &(type){.k = TINT, .sz = 2, .sign = false},
	*tu32 = &(type){.k = TINT, .sz = 4, .sign = false},
	*tu64 = &(type){.k = TINT, .sz = 8, .sign = false},
	*tf32 = &(type){.k = TFLOAT, .sz = 4},
	*tf64 = &(type){.k = TFLOAT, .sz = 8},
	*tbool = &(type){.k = TBOOL, .sz = 1},
	/* Aliases sut up in checkinit() */
		*astr = &(type){};

LOCAL static decl
	*ctrue = &(decl){.k = DCONST, .id = "true", .e = &(expr){.k = EINTEGER, .lint = 1}},
	*cfalse = &(decl){.k = DCONST, .id = "false", .e = &(expr){.k = EINTEGER, .lint = 1}};

LOCAL static scope *stk;  // Scope stack, linked list.
LOCAL static size stksz;
LOCAL static scope *mags;	// Magic Scope.

/*
 * Helps to declare type in
 * the magic scope.
 */
static void
dtype(str id, type *ty) {
	decl *d = newdecl((loc){}, DDEF, id);
	d->ck = 1;
	d->ty = newtype(TTYPE);
	d->ty->ty = ty;
	d->ty->d = d;
	declare(mags, d);
}

static void
inscope(scope *s) {
	s->next = stk;
	stk = s;
	stksz++;
}

static void
outscope() {
	stk = stk->next;
	if(stksz <= 1)
		adeus("Scope stack error.");
	stksz--;
}

void
checkinit() {
	/* Init aliases. */
	astr->k = TSLICE;
	astr->ty = tu08;

	/* Init decls. */
	ctrue->ty = tbool;
	cfalse->ty = tbool;

	/* Initializes the magic scope. */
	mags = newscope();
	inscope(mags);
	/* Types. */
	dtype("i08", ti08);
	dtype("i16", ti16);
	dtype("i32", ti32);
	dtype("i64", ti64);
	dtype("u08", tu08);
	dtype("u16", tu16);
	dtype("u32", tu32);
	dtype("u64", tu64);
	dtype("f32", tf32);
	dtype("f64", tf64);
	dtype("bool", tbool);
	/* Aliases. */
	dtype("str", astr);
	dtype("rune", tu32);
	/* Declarations. */
	declare(mags, ctrue);
	declare(mags, cfalse);
}

[[nodiscard]]
static type *aply(loc l, type *x, type *y);
[[nodiscard]]
static type *ctype(type *t);
static void cexpr(expr *e);
static void cstmt(stmt *s, ctx c);
static void cdecl(decl *d);
static void cscope(scope *s);
static void clist(list *l);
static void cblock(block *b, ctx c);

static type *
aply(loc l, type *x, type *y) {
	if(x->k && x->k != y->k)
		goto diff;

	switch(x->k) {
		dbuk *b;

	case TNONE:
		x = y;
		break;
	case TEXPR:
		adeus("aply() received TEXPR.");
	case TINT:
	case TFLOAT:
	case TBOOL:
	case TTYPE:
		break;
	case TFUNC:
		x->ty = aply(
			l,
			x->ty,
			y->ty
		);
		/* falltrhough */
	case TSTRUC:
	case TUNION:
		if(x->s->maps != y->s->maps)
			goto diff;
		b = x->s->fst;
		while(b) {
			cdecl(b->d);
			b = b->next;
		}
		break;
	case TARRAY:
		if(x->len != y->len)
			goto diff;
	case TREF:
	case TSLICE:
		x->ty = aply(
			l,
			x->ty,
			y->ty
		);
		break;
	}

	internty(x);
	return x;

diff:
	lerro(l, "Aplying different types.");
	return tnone;
}

static type *
ctype(type *t) {
	switch(t->k) {
		dbuk *b;
		loc l;

	case TNONE:
	case TINT:
	case TFLOAT:
	case TBOOL:
		break;
	case TEXPR:
		cexpr(t->e);
		l = t->e->l;
		t = t->e->ty;
		if(t->k != TTYPE) {
			lerro(l, "Not a type.");
			t = newtype(TNONE);
			break;
		}
		t = t->ty;
		break;
	case TREF:
		t->ty = ctype(t->ty);
		/*
		 * References inherits
		 * the type methods.
		 */
		t->s = t->ty->s;
		break;
	case TFUNC:
		t->ty = ctype(t->ty);
		/* fallthrough */
	case TSTRUC:
		cscope(t->s);
		break;
	case TARRAY:
		cexpr(t->e);
		l = t->e->l;
		if(t->e->k != EINTEGER) {
			lerro(l, "Expected integer constant.");
			break;
		}

		t->len = t->e->lint;
		/* fallthrough */
	case TSLICE:
	case TTYPE:
		t->ty = ctype(t->ty);
		break;
	case TUNION:
		cscope(t->s);
		b = t->s->fst;
		while(b) {
			/* TODO: Eliminate these loops. */
			dbuk *b2 = t->s->fst;
			while(b2) {
				if(b == b2)
					break;
				if(b->d->ty == b2->d->ty)
					lerro(b->d->l, "Duplicated permutation.");
				b = b->next;
			}
			b = b->next;
		}
		break;
	}

	return internty(t);
}

static void
cexpr(expr *e) {
	if(e->ck++)
		return;

	/* Type switch. */
	switch(e->k) {
		scope *s;
		dbuk *b;
		expr *ei;
		type *ty;

	case ENONE:
		break;
	case EADD:
	case ESUB:
	case EMUL:
	case EDIV:
		cexpr(e->lhs);
		cexpr(e->rhs);
		e->ty = aply(
			e->l,
			e->lhs->ty,
			e->rhs->ty
		);
		switch(e->ty->k) {
		case TINT:
		case TFLOAT:
			break;
		default:
			lerro(
				e->l,
				"Expected integer or"
				" float operands."
			);
			goto none;
		}
		break;
	case EREM:
	case EAND:
	case EBOR:
	case EEOR:
		cexpr(e->lhs);
		cexpr(e->rhs);
		e->ty = aply(
			e->l,
			e->lhs->ty,
			e->rhs->ty
		);
		if(e->ty->k != TINT) {
			lerro(
				e->l,
				"Expected integer operands."
			);
			goto none;
		}
		break;
	case ELAND:
	case ELOR:
	case EEQL:
	case ENEQ:
	case EGTR:
	case ELSS:
	case EGEQ:
	case ELEQ:
		e->ty = tbool;
		cexpr(e->lhs);
		cexpr(e->rhs);
		(void) aply(
			e->l,
			e->lhs->ty,
			e->rhs->ty
		);
		switch(e->lhs->ty->k) {
		case TINT:
		case TFLOAT:
		case TBOOL:
		case TREF:
		case TUNION:
			break;
		default:
			lerro(
				e->l,
				"Type is not"
				" comparable."
			);
		case TNONE:
			goto none;
		}
		break;
	case EKEY:
		e->ty = tbool;
		cexpr(e->lhs);
		cexpr(e->rhs);
		e->ty = aply(
			e->l,
			e->ty,
			e->rhs->ty
		);
		break;
	case EDOT:
		cexpr(e->lhs);
		s = e->lhs->ty->s;
		if(!s) {
			lerro(
				e->lhs->l,
				"Not a scope."
			);
			goto none;
		}
		e->d = getdecl(s, e->str);
		if(!e->d) {
			lerro(e->l, "'%s' is not a member.");
			goto none;
		}
		if(!e->d->ty->k || !e->d->ck) {
			cdecl(e->d);
			if(!e->d->ty->k || e->d->ck > 1) {
				lerro(e->l, "Can't infer type.");
				goto none;
			}
		}

		e->ty = e->d->ty;
		break;
	case EREF:
		cexpr(e->lhs);
		e->ty = newtype(TREF);
		e->ty->ty = e->lhs->ty;
		e->ty = ctype(e->ty);
		break;
	case EIND:
		cexpr(e->lhs);
		ty = e->lhs->ty;
		if(ty->k != TREF) {
			lerro(
				e->lhs->l,
				"Expected reference."
			);
			break;
		}
		e->ty = ty->ty;
		break;
	case ENOT:
	case EPLUS:
	case EMINUS:
		cexpr(e->lhs);
		e->ty = e->lhs->ty;
		break;
	case ECALL:
		clist(e->ls);

		/* Enters in the method scope. */
		if(e->ls->ec > 0) {
			inscope(e->ls->fst->ty->s);
			cexpr(e->lhs);
			outscope();
		} else {
			cexpr(e->lhs);
		}

		/* Checks functional type and aplies.  */
		ty = e->lhs->ty;
		if(!ty->k)
			goto none;
		if(ty->k != TFUNC) {
			lerro(e->lhs->l, "Not a functional.");
			goto none;
		}
		e->ty = ty->ty;

		/* Check the arguments. */
		if(ty->s->dc != e->ls->ec) {
			lerro(
				e->l,
				"Argument count mismatch."
			);
			break;
		}
		ei = e->ls->fst;
		b = ty->s->fst;
		while(ei && b) {
			(void) aply(
				ei->l,
				b->d->ty,
				ei->ty
			);
			ei = ei->next;
			b = b->next;
		}
		break;
	case ESUBS:
		cexpr(e->lhs);
		cexpr(e->rhs);
		ty = e->lhs->ty;

		switch(ty->k) {
		case TARRAY:
		case TSLICE:
			break;
		default:
			lerro(
				e->lhs->l,
				"Expected array or slice."
			);
			break;
		}
		if(e->rhs->ty->k != TINT) {
			lerro(
				e->rhs->l,
				"Expected integer."
			);
		}
		e->ty = ty->ty;
		break;
	case EINST:
		cexpr(e->lhs);
		ty = e->lhs->ty;
		if(ty->k != TSTRUC) {
			lerro(
				e->lhs->l,
				"Expected struct."
			);
			goto none;
		}
		e->ty = ty;

		/* The expressions must be EKEY. */
		ei = e->ls->fst;
		while(ei) {
			cexpr(ei);
			if(ei->k != EKEY) {
				lerro(ei->l, "Expected key.");
				break;
			}
			if(ei->lhs->k != EDREF) {
				lerro(
					ei->lhs->l,
					"Expected field"
					" name"
				);
				break;
			}
			decl *d = getdecl(
				ty->s,
				ei->rhs->str
			);

			cexpr(ei->rhs);  // Only the value is checked.
			(void) aply(
				ei->l,
				d->ty,
				ei->ty
			);
			ei = ei->next;
		}
		/*
		 * Fields not initialised
		 * will be monitored.
		 */
		break;
	case EDREF:
		for(scope *s = stk; s; s = s->next) {
			decl *x = getdecl(s, e->str);
			if(x) {
				if(!x->ty->k || !x->ck) {
					cdecl(x);
					if(!x->ty->k || x->ck > 1) {
						lerro(e->l, "Can't infer type.");
						goto none;
					}
				}
				e->d = x;
				e->ty = x->ty;
				goto ret;
			}
		}
		lerro(
			e->l,
			"'%s' is undeclared.",
			e->str
		);
		goto none;
	case EPAREN:
		cexpr(e->lhs);
		e->ty = e->lhs->ty;
		break;
	case EOPER:
		cblock(
			e->b,
			(ctx){
				.oper = e,
			}
		);
		e->ty = tnone;
		break;
	case ESTRUC:
		cscope(e->s);
		e->ty = newtype(TSTRUC);
		e->ty->s = e->s;
		break;
	case EARRAY:
		e->ty = newtype(TARRAY);
		e->ty->len = e->ls->ec;
		e->ty->ty = e->ls->fst->ty;
		ei = e->ls->fst;
		while(ei) {
			cexpr(ei);
			e->ty->ty = aply(
				e->l,
				e->ty->ty,
				ei->ty
			);
			ei = ei->next;
		}
		break;
	case EINTEGER:
		e->ty = ti32;
		break;
	case EFLOAT:
		e->ty = tf32;
		break;
	case ERUNE:
		e->ty = tu32;
		break;
	case ESTRING:
		e->ty = astr;
		break;
	}

ret:
	return;

none:
	e->ty = tnone;
}

static void
cstmt(stmt *s, ctx c) {
	assert(c.oper->k == EOPER);

	if(s->ck++)
		return;

	/* Update the context. */
	c.f ^= FALLOK;

	switch(s->k) {
		decl *d;
		scope *scp;

	case SNONE:
		break;
	case SRESULT:
		cexpr(s->e);
		c.oper->ty = aply(
			s->l,
			c.oper->ty,
			s->e->ty
		);
		/*
		 * If the next there is a
		 * statement after this,
		 * it will be unreachable.
		 */
		if(s->next)
			lwarn(s->next->l, "Unreachable.");
		break;
	case SLOCAL:
		d = newdecl(s->l, DOBJ, s->id);
		d->l = s->l;
		d->ty = s->ty;
		d->e = s->e;
		cdecl(d);
		declare(stk, d);
		break;
	case SIF:
		if(s->init) {
			scp = newscope();
			inscope(scp);
			cstmt(s->init, c);
		}
		cexpr(s->e);
		cblock(s->blk, c);
		if(s->els)
			cblock(s->els, c);
		if(s->init)
			outscope();
		break;
	case SFOR:
		if(s->init) {
			scp = newscope();
			inscope(scp);
			cstmt(s->init, c);
		}
		if(s->post)
			cstmt(s->post, c);
		if(s->e)
			cexpr(s->e);
		c.f |= BREAKOK | CONTOK;
		cblock(s->blk, c);
		if(s->init)
			outscope();
		break;
	case SCASE:
		if(s->init) {
			scp = newscope();
			inscope(scp);
			cstmt(s->init, c);
		}
		cexpr(s->e);
		c.f |= BREAKOK | FALLOK;
		cblock(s->blk, c);
		if(s->init)
			outscope();
		break;
	case SOF:
		if(!c.cas) {
			lerro(
				s->l,
				"of label outside case."
			);
			break;
		}
		cexpr(s->e);
		if(c.cas->e) {
			(void) aply(
				s->e->l,
				c.cas->e->ty,
				s->e->ty
			);
			break;
		}
		if(s->e->ty->k != TBOOL) {
			lerro(
				s->e->l,
				"Expected boolean."
			);
		}
		break;
	case SBREAK:
		if(c.f ^ BREAKOK) {
			lerro(
				s->l,
				"break statement outside"
				" for or case."
			);
			break;
		}
	case SCONTINUE:
		if(c.f ^ CONTOK) {
			lerro(
				s->l,
				"continue statement"
				" outside for loop."
			);
		}
		break;
	case SFALL:
		if(c.f ^ FALLOK) {
			lerro(
				s->l,
				"fall statement outside case."
			);
			break;
		}
		if(s->next && s->next->k != SOF)
			lerro(s->l, "Illegal fall.");
		break;
	case SHOLE:
		cblock(s->blk, c);
		break;
	case SASSIGN:
		cexpr(s->lhs);
		cexpr(s->e);
		switch(s->lhs->k) {
		default:
			lerro(s->lhs->l, "Not assignable.");
			break;
		case EDREF:
		case EIND:
		case ESUBS:
			(void) aply(
				s->e->l,
				s->lhs->ty,
				s->e->ty
			);
		}
		break;
	case SEXPR:
		cexpr(s->e);
		break;
	}
}

static void
cdecl(decl *d) {
	if(d->ck++)
		return;

	switch(d->k) {
	case DNONE:
		break;
	case DFUNC:
		d->ty = ctype(d->ty);
		if(d->e) {
			inscope(d->ty->s);
			cexpr(d->e);
			d->ty->ty = aply(
				d->l,
				d->ty->ty,
				d->e->ty
			);
			outscope();
			break;
		}
		if(!d->ty->ty->k) {
			lerro(
				d->l,
				"Can't infer result type."
			);
		}
		break;
	case DOBJ:
		d->ty = ctype(d->ty);
		if(d->e) {
			cexpr(d->e);
			d->ty = aply(
				d->e->l,
				d->ty,
				d->e->ty
			);
			break;
		}
		if(!d->ty->k) {
			lerro(
				d->l,
				"Can't infer object type."
			);
		}
		break;
	case DCONST:
		d->ty = ctype(d->ty);
		if(!d->e) {
			lerro(
				d->l,
				"Unitialised constant."
			);
			break;
		}
		cexpr(d->e);
		d->ty = aply(
			d->l,
			d->ty,
			d->e->ty
		);
		break;
	case DDEF:
		d->ty = ctype(d->ty);
		if(d->e)
			d->ty->e = d->e;
		/*
		 * We should avoid this
		 * type to be interned.
		 * We can't use the id
		 * cause may exist more
		 * than one type with
		 * the same in a scope,
		 * so I decided to use
		 * the declaration addr.
		 */
		d->ty->d = d;
		break;
	}
}

static void
cscope(scope *s) {
	inscope(s);
	dbuk *b = s->fst;
	while(b) {
		cdecl(b->d);
		b = b->next;
	}
	outscope();
}

static void
clist(list *l) {
	expr *e = l->fst;
	while(e) {
		cexpr(e);
		e = e->next;
	}
}

static void
cblock(block *b, ctx c) {
	inscope(newscope());
	stmt *s = b->fst;
	while(s) {
		cstmt(s, c);
		s = s->next;
	}
	outscope();
}

void
check() {
	cscope(u->s);
}
