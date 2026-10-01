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

#define LOCAL _Thread_local

typedef struct loc loc;
typedef struct scope scope;
typedef struct unit unit;
typedef struct type type;
typedef struct hint hint;
typedef struct decl decl;
typedef struct expr expr;
typedef struct stmt stmt;

/* Source location. */

struct loc {
	u32 line, col;
};

/* The Unit. */

struct unit {
	str id;
	scope *s;
};

/* Scopes. */

typedef struct dbuk dbuk;
struct dbuk {
	u64 h;  // Hash.
	size len;
	struct dbuk *next;
	decl *d;
};

/*
 * Scopes contains declarations
 * as a hash map. We need the
 * order they were inserted
 * when printing AST. `struct
 * scope` isn't an AST node.
 */
struct scope {
	/* Map. */
	dbuk
		*map,			 // Map.
		*fst, *lst;	 // First and last bucket.
	size maps;		 // Map size.
	size dc;			 // Decl count.
};

/* Types. */

enum typek : u08 {
	TNONE,
	TINT,
	TFLOAT,
	TBOOL,
	TRUNE,
	TREF,
	TFUNC,
	TSTRUC,
	TARRAY
};
typedef enum typek typek;

struct type {
	typek k;
	loc l;
	size sz;
	bool sign;
	bool move;	// Type may not be copied.

	type *ty;  // Secondary type, may be null.
	scope *s;  // Parameters or fields.

	/* TARRAY */
	i64 asz;			// -1 for slices.
	expr *szexpr;	// Constexpr.
};

/* Hints. */

enum hintk {
	HNONE,
	HEXPR,
	HREF,
	HFUNC,
	HSTRUC,
	HARRAY
};
typedef enum hintk hintk;

struct hint {
	hintk k;
	loc l;
	hint *ty;  // Secondary types.
	expr *e;	  // HEXPR or array size.
	scope *s;  // Parameters or fields.
};

/* Declarations. */

enum declk : u08 {
	DNONE,
	DFUNC,
	DOBJ,
	DTYPE	 // Primitives.
};
typedef enum declk declk;

struct decl {
	declk k;
	loc l;
	str id;
	type *ty;  // After analysis.
	hint *h;

	expr *e;

	/* DTYPE */
	scope *s;  // Type scope.
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
	EBOR,
	ELAND,
	ELOR,
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
	ENOT,
	EPLUS,
	EMINUS,
	EOPER,
	ESELECT,

	EPAREN,
	EDREF,
	EINTEGER,
	EFLOAT,
	EBOOL,
	ERUNE,
	ESTRING,
	ESTRUC,
	EARRAY
};
typedef enum exprk exprk;

struct expr {
	exprk k;
	loc l;
	expr *next;
	type *ty;

	/* Most nodes. */
	expr *lhs;
	expr *rhs;
	str str;
	size n;

	/* EDREF */
	decl *d;

	/* EOPER */
	stmt *oper;

	/* EINTEGER, ERUNE, EBOOL */
	u64 lint;

	/* EFLOAT */
	double flt;
};

/* Statements. */

enum stmtk {
	SNONE,
	SRESULT,
	SSTACK,
	SIF,
	SFOR,
	SBREAK,
	SCONTINUE,
	SEXPR
};
typedef enum stmtk stmtk;

struct stmt {
	stmtk k;
	loc l;
	stmt *next;

	expr *e;
	stmt *doblk;  //  if, for block.

	/* SSTACK (local variable) */
	str id;
	type *ty;

	/* SIF */
	stmt *orel;	 // or, else block.
};

/* tok.c */

enum tokk {
	LNONE,

	LEOF,
	LEOL,

	LID,

	LDEF,
	LALIAS,
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
typedef enum tokk tokk;

typedef struct tok tok;
struct tok {
	tokk k;
	loc l;
	u64 data;
	str lit;
};

str tokname(tokk);

/* main.c */

extern LOCAL str file;

extern str progname;
extern bool	 // Flags.
	fwerror,
	fdump;

/* type.c */

type *newtype(loc l, typek k);
bool tyeql(type *x, type *y);

/* ast.c */

unit *newunit(str id);
hint *newhint(loc l, hintk k);
decl *newdecl(loc l, declk k, str id);
expr *newexpr(loc l, exprk k);
stmt *newstmt(loc l, stmtk k);

/* scope.c */

scope *newscope();
void declare(scope *s, decl *d);
decl *getdecl(scope *s, str id);
bool scopeql(scope *x, scope *y);

/* diag.c */

void erro(str msg, ...);
void warn(str msg, ...);
void note(str msg, ...);
void info(str msg, ...);

void lerro(loc l, str msg, ...);
void lwarn(loc l, str msg, ...);
void lnote(loc l, str msg, ...);
void linfo(loc l, str msg, ...);

[[noreturn]]
void adeus(str msg, ...);

u64 errcount();

/* lex.c */

error lexinit();
void lexdnit();
tok lex();
str intern(str s, size n);

/* parser.c */

void parserinit();
void parse(unit *u);

/* analyser.c */

void analyserinit();
void analyse(unit *u);

/* utis.c */

void meminit();
void memdnit();
void *alloc(size n);
