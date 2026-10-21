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
typedef float f32;
typedef double f64;
typedef size_t size;
typedef const char *str;
typedef u32 rune;
typedef u08 error;

#define LOCAL _Thread_local

typedef struct loc loc;
typedef struct scope scope;
typedef struct list list;
typedef struct block block;
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
	scope *next;
	/* Map. */
	dbuk
		*map,			 // Map.
		*fst, *lst;	 // First and last bucket.
	size maps;		 // Map size.
	size dc;			 // Decl count.
};

/* Lists. */

/*
 * Lists contains a linked
 * list of expressions and
 * a pointer to the last
 * expression.
 */
struct list {
	expr *fst, *lst;
	size ec;
};

/* Blocks. */

/*
 * Blocks contains a linked
 * list of statements and
 * a pointer to the last
 * statement.
 */
struct block {
	stmt *fst, *lst;
	size sc;
	scope *s;  // Used by the analyser.
};

/* Types. */

enum typek : u08 {
	TNONE,
	TEXPR,  // Expr like mod.type.
	TINT,
	TFLOAT,
	TBOOL,
	TREF,
	TFUNC,
	TSTRUC,
	TARRAY,
	TSLICE,
	TUNION,
	TTYPE
};
typedef enum typek typek;

struct type {
	typek k;
	size sz;
	decl *d;	 // When is derived from a def.
	bool sign;
	bool move;	 // Type may not be copied.
	expr *init;	 // Initialiser.
	expr *e;		 // TEXPR or array length.

	type *ty;	 // Secondary type, may be null.
	scope *s;	 // Parameters, permutations or fields.
	scope *met;	 // Associated functions.

	i64 len;
};

/* Declarations. */

enum declk : u08 {
	DNONE,
	DFUNC,
	DOBJ,
	DCONST,
	DDEF
};
typedef enum declk declk;

struct decl {
	declk k;
	i32 ck;
	loc l;
	str id;
	type *ty;

	size i;	// Struct or parameter index, set up by declare().
	expr *e;

	/* DTYPE */
	scope *s;  // Type scope.
};

/* Expressions. */

enum exprk : u08 {
	ENONE,

	/* Binary operators. */
	EADD,
	ESUB,
	EMUL,
	EDIV,
	EREM,
	EAND,
	EBOR,
	EEOR,
	ELAND,
	ELOR,
	EEQL,
	ENEQ,
	EGTR,
	ELSS,
	EGEQ,
	ELEQ,
	EKEY,	 // Key-value.
	EDOT,

	/* Unary operators. */
	EREF,
	EIND,
	ENOT,
	EPLUS,
	EMINUS,

	/* Specialised operators. */
	ECALL,
	ESUBS,  // array[i]
	EINST,  // StructType~[field = val]

	/* Articulations. */
	EDREF,
	EPAREN,
	EOPER,
	ESTRUC,
	EARRAY,

	/* Literals. */
	EINTEGER,
	EFLOAT,
	ERUNE,
	ESTRING
};
typedef enum exprk exprk;

struct expr {
	exprk k;
	i32 ck;	// Avoids reccursion.
	loc l;
	expr *next;
	type *ty;

	/* Most nodes. */
	expr *lhs;
	expr *rhs;
	decl *d;
	str str;

	/* ESTRING */
	size n;

	/* ECALL, EARRAY, EBASE */
	list *ls;

	/* ESTRUC */
	scope *s;

	/* EOPER */
	block *b;

	/* EINTEGER, ERUNE, EBOOL */
	u64 lint;

	/* EFLOAT */
	double flt;
};

/* Statements. */

enum stmtk {
	SNONE,
	SRESULT,
	SLOCAL,
	SIF,
	SFOR,
	SCASE,
	SOF,
	SBREAK,
	SCONTINUE,
	SFALL,
	SHOLE,
	SASSIGN,
	SEXPR
};
typedef enum stmtk stmtk;

struct stmt {
	stmtk k;
	i32 ck;	// Avoids reccursion.
	loc l;
	stmt *next;

	expr *e;
	block *blk;	 // SIF, SFOR block.
	stmt *init;	 // SIF, SFOR initialization.
	str id;
	decl *d;
	type *ty;

	/* SFOR */
	stmt *post;	 // Post-instruction stmt.

	/* SIF */
	block *els;

	/* SASSIGN */
	expr *lhs;
};

/* Tokens. */

enum tokk {
	LNONE,

	LEOF,
	LEOL,

	LID,

	LANEX,
	LBREAK,
	LCASE,
	LCONST,
	LCONTINUE,
	LDEF,
	LDEFER,
	LELSE,
	LENUM,
	LFALL,
	LFOR,
	LGOTO,
	LHOLE,
	LIF,
	LOF,
	LUNION,
	LUSE,

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
	LNOT,
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

/* main.c */

extern LOCAL str file;
extern LOCAL unit *u;

extern str progname;
extern bool	 // Flags.
	fwerror,
	fdump;

/* ast.c */

void newunit(str id);
type *newtype(typek k);
decl *newdecl(loc l, declk k, str id);
expr *newexpr(loc l, exprk k);
stmt *newstmt(loc l, stmtk k);

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
void parse();

/* check.c */

LOCAL extern type
	*tnone,	// For untyped things.
	*ti08,
	*ti16,
	*ti32,
	*ti64,
	*tu08,
	*tu16,
	*tu32,
	*tu64,
	*tf32,
	*tf64,
	*tbool,
	*astr;

void checkinit();
void check();

/* utis.c */

void meminit();
void memdnit();
void *alloc(size n);
scope *newscope();
decl *getdecl(scope *s, str id);
void declare(scope *s, decl *d);
list *newlist();
void append(list *l, expr *e);
block *newblock();
void join(block *b, stmt *s);
type *internty(type *tp);
str tokname(tokk);
