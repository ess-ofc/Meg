/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#pragma once

/* IWYU pragma: begin_exports */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
/* IWYU pragma: end_exports */

/* Defs. */
typedef int8_t i08;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u08;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t size;
typedef const char *str;
typedef u32 rune;
typedef u08 error;

#define LOCAL thread_local

/* Source location. */

struct loc {
	u32 line, col;
};

/* The Unit. */

struct unit {
	str id;
	struct scope *s;
};

/* Scopes. */

struct scope {
	/* Map. */
	struct dbuk {
		u64 h;  // Hash.
		size len;
		struct dbuk *next;
		struct decl *d;
	} *map,			 // Map.
		*fst, *lst;	 // First and last bucket.
	size maps;		 // Map size.
	size dc;			 // Decl count.
};

/* Types. */

enum typek : u08 {
	TNONE,
	TDREF,
	TREF,
	TFUNC,
	TSTRUC,
	TARRAY,
	TSLICE
};

/* Qualifiers. */
enum qual : u08 {
	QNONE,
	QCONST,
	QMUT
};

struct type {
	struct loc l;
	union {
		struct {
			struct expr *d;  // dref to a type.
		} dref;

		struct {
			struct type *ty;
		} ref;

		struct {
			struct scope *s;
			struct type *ty;
		} func;

		struct {
			struct scope *s;
		} struc;

		struct {
			struct expr *sz;
			struct type *ty;
		} array;

		struct {
			struct type *ty;
		} slice;
	} u;
	enum qual q;
	enum typek k;
};

/* Declarations. */

enum declk : u08 {
	DNONE,
	DFUNC,
	DOBJ,
	DTYPE,  // Primitives.
	DDEF,
	DALIAS
};

/* Type category. */
enum typec : u08 {
	CNONE,
	CINT,
	CUINT,
	CFLOAT,
	CBOOL,
	CRUNE
};

struct decl {
	struct loc l;
	str id;
	union {
		struct {
			struct scope *s;
			struct type *ty;
		} func;

		struct {
			struct expr *init;
			struct type *ty;
		} obj;

		struct {
			size size;
			bool sign;
			enum typec c;
		} type;

		struct {
			struct scope *s;
			struct type *ty;
		} def;

		struct {
			struct type *ty;
		} alias;
	} u;
	enum declk k;
};

/* Expressions. */

enum exprk : u08 {
	ENONE,
	EADD,
	ESUB,
	EMUL,
	EDIV,
	EREM,

	EAND,
	EEOR,
	EOR,
	ELAND,
	ELOR,
	ENOT,
	EEQL,
	ENEQ,
	EGTR,
	ELSS,
	EGEQ,
	ELEQ,
	EDOT,

	EREF,
	ECALL,
	ENEG,
	EPLUS,
	EMINUS,
	ESELECT,

	EPAREN,
	EDREF,
	ELIT,
	ESTR,
	ESTRUC,
	EARRAY
};

struct expr {
	struct loc l;
	struct expr *next;
	struct type *ty;
	union {
		struct {
			struct expr *lhs;
			struct expr *rhs;
		} bin;

		struct {
			struct expr *e;
		} un;

		struct {
			struct expr *func;  // DREF;
			struct expr *args;
		} call;

		struct {
			str id;
			struct decl *d;
		} dref;

		struct {
			uint64_t num;
		} lit;

		struct {
			str s;
		} str;

		struct {
			struct scope *s;
		} struc;

		struct {
			struct expr *list;
		} array;
	} u;
	enum exprk k;
};

/* Statements. */

enum stmtk {
	SNONE,
	SRESULT,
	SSTACK,
	SASSIGN,
	SIF,
	SFOR,
	SBREAK,
	SCONTINUE,
	SEXPR
};

struct stmt {
	struct loc l;
	struct stmt *next;
	union {
		struct {
			str id;
			struct type *ty;
			struct expr *e;
		} stack;

		struct {
			struct expr *var;	 // Any assinable expr.
			struct expr *e;
		} assign;

		struct {
			struct expr *cond;
			/* Both stmt list. */
			struct stmt *then;
			struct stmt *orel;
		} ifs;

		struct {
			struct expr *cond;  // Maybe null.
			struct stmt *does;  // Stmt list.
		} fors;

		/* ERESULT and SEXPR. */
		struct expr *expr;
	} u;
	enum stmtk k;
};

/* Tokens. */

enum tok {
	LNONE,

	LEOF,
	LEOL,

	LID,

	LDEF,
	LMUT,
	LCONST,
	LIF,
	LOR,
	LELSE,
	LDEFER,
	LFOR,
	LBREAK,
	LCONTINUE,

	LINTEGER,
	LFLOAT,
	LSTRING,
	LRUNE,

	LADD,
	LSUB,
	LMUL,
	LDIV,
	LREM,
	LAND,
	LBOR,
	LEOR,
	LNEG,
	LLAND,
	LLOR,
	LEQL,
	LNEQ,
	LGTR,
	LLSS,
	LGEQ,
	LLEQ,
	LDOT,

	LCOMMA,
	LCOLON,
	LSEMI,
	LLPAREN,
	LLBRACKT,
	LLBRACE,
	LRPAREN,
	LRBRACKT,
	LRBRACE,

	LASSIGN,
	LRESULT,
	LTILDE,
	LDOLLAR,

	LEND
};

str tokname(enum tok);

/* main.c */

extern LOCAL str file;

extern str progname;
extern bool	 // Flags.
	fwerror,
	fdump;

/* ast.c */

struct unit *newunit(str id);
struct type *newtype(
	struct loc l,
	enum typek k,
	enum qual q
);
struct decl *newdecl(
	struct loc l,
	enum declk k,
	str id
);
struct expr *newexpr(
	struct loc l,
	enum exprk k,
	struct type *ty
);
struct stmt *newstmt(
	struct loc l,
	enum stmtk k
);

/* scope.c */

struct scope *newscope();
void declare(struct scope *s, struct decl *d);
struct decl *getdecl(struct scope *s, str id);

/* diag.c */

void erro(str msg, ...);
void warn(str msg, ...);
void note(str msg, ...);
void info(str msg, ...);

void lerro(struct loc l, str msg, ...);
void lwarn(struct loc l, str msg, ...);
void lnote(struct loc l, str msg, ...);
void linfo(struct loc l, str msg, ...);

[[noreturn]]
void adeus(str msg, ...);

u64 errcount();

/* lex.c */

error lexinit();
void lexdnit();
enum tok lex();
str getlit();
uint64_t getdata();
str intern(str s, size n);

/* parser.c */

void parse(struct unit *u);

/* utis.c */

void meminit();
void memdnit();
void *alloc(size n);
