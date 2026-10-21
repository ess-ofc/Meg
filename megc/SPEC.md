# The Meg Programming Language Specification.

The Meg Programming is designed to be
simple a simple programming language, its
syntax is clear and will help you to
develop. This specification clears out how
the language works and will serve as base
for the `megc` develoment.

| *Don't forget Meg, 2020 - 10.07.2025*

## The Base.

The Language is based in four concepts:
declarations, expressions, statements and
type hints. Every syntatical construction
in the language are based in these
concepts.

## Keywords.

There are 15 keywords in the language, as 
follows:
```Meg
anex      def    goto  fall
break     defer  hole
case      else   if
const     enum   of
continue  for    union
```
## Magic Scope.

The Magic Scope is where the compiler
declares default declarations of the
language. The Magic Scope will be
automatically created while checking a
compilation unit, it must contain the
following declarations:

- `i08` signed 8-bit integer type.
- `i16` signed 16-bit integer type.
- `i32` signed 32-bit integer type.
- `i64` signed 64-bit integer type.
- `u08` unsigned 8-bit integer type.
- `u16` unsigned 16-bit integer type.
- `u32` unsigned 32-bit integer type.
- `u64` unsigned 64-bit integer type.
- `f32` 32-bit floating point type.
- `f64` 64-bit floating point type.
- `bool` boolean type.
- `rune` a 32 integer alias type.
- `str` an unsigned byte slice alias type.
- `self` the unit declarations.

## The Syntax.

As said above, every syntatical
construction are based in those four
concepts. Each concept serves for a
specific purpose, except for the type
concept, which have a semantic and logic
function in the source analysis.

### 1. Declarations.

All declarations must have an identifer,
type and might, optionally, have an
initialiser expression. Every declaration
must be present in the Intermediate Object,
as an object, function, type or dependency
metadata for annexed modules or packages.

#### 1.1 Annex Declaration.

The syntax is
<pre>
<b>anex</b> <i>identifier expression</i>
</pre>

EXAMPLE
```Meg
anex meg "meg"
anex sys "libsystem"
anex mod "module.meg"
```
In the example, the package 'meg' is 
annexed to this module as *meg*, the
package 'libsystem" as *sys* and the
module 'module.meg' as *mod*.

The expression must be constant and of type
`str` (`[~]u08`), this string will be
evaluated as the annex name. If the annex
name appears as the name of any package
dependency, it will be used in the current
module; otherwise, the compiler will treat
the name as a path to a module relative to 
this module folder.

When the annex can't be solved, the
compiler should dispay an error explaining
why. When the annexed package/module
depends on this package/module, the
compiler should display an error of ciclic
dependency.

An annex declaration will contain all types
and public functions of the annexed package
or module declared inside.

#### 1.2 Function Declaration.

The syntaxes are:
1. External function declaration.
<pre>
<i>identifier</i>(<i>declaration-list</i>): <i>type</i>
</pre>
2. Function definition.
<pre>
<i>identifier</i>(<i>declaration-list</i>): <i>type</i> = <i>expression</i>
</pre>

External function declarations must be
defined in other module in this package or
package dependency, the linker will 
display an error otherwise. Function
definitions must have an expression, which
will be executed at run-time and result in
a value of the same type that is specified
in *type* in the syntax 2.

Functions may have parameters declared in
the delaration list inside parenthesis,
they will be declared in a proper scope,
which is the same scope in where the
expression will be executed.

#### 1.3 Object Declaration.

The syntaxes are:
1. Object without initialiser.
<pre>
<i>identifier</i>: <i>type</i>
</pre>
1. Object with initialiser.
<pre>
<i>identifier</i>: <i>type</i> = <i>expression</i>
</pre>

An object is always a memory region where
the size corresponds to the type size with
alignment.

It will store a value of its type and may
be uninitialised as long as
its state is known:
```Meg
newObj() := {
   obj: Obj \ Uninitialised, but ok. \
   => obj \ Uninitialised value escape error. \
}
```

When an object hash no initialiser, it will
be initialised with the type initialiser,
if any:
```Meg
def Integer: i32 = 0 \ Initialiser is 0. \

newInt() := {
   int: Integer \ Is automatically 0. \
   => int \ Returning 0. \
}
```

#### 1.4 Defined Types.

The syntaxes are:
1. Type without initialiser.
<pre>
<b>def</b> <i>identifier</i>: <i>type</i>
</pre>
2. Type with initialiser.
<pre>
<b>def</b> <i>identifier</i>: <i>type</i> = <i>expression</i>
</pre>

A type initialiser redefines *type* as an
inequivalent type under a new identifier.
When an initialiser is provided, it must be
a constant expression of the type *type*.
