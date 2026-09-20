# Self hosting

Spite's compiler is written in Spite and compiles itself. There is no other implementation.

## How it builds

`bootstrap/seed/spite_compiler.c` is the C that the compiler emits for its own sources. Compiling that file with
any C compiler gives you a working Spite compiler, which can then compile the sources again and produce the same
C -- a fixpoint.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
./spite --file=bootstrap/spite_compiler.spite --mode=c > next.c    # equal to the committed seed
```

So the only thing needed to build Spite from nothing is a C compiler. The seed is committed, and `check.sh`
refuses to pass if it has drifted from the sources (`bash check.sh --update-seed` refreshes it after an
intended change).

## What proves it

`bash check.sh`:

1. builds the seed compiler,
2. has it compile the compiler sources (generation 2),
3. has generation 2 compile them again (generation 3) and requires the two to be **byte identical**,
4. runs every program in `conformance/`, requiring exact output and balanced allocations.

A generation that cannot reproduce itself is not a compiler for this language, so step 3 is the real test.

## Layout

```
bootstrap/spite_compiler.spite   the entry class
bootstrap/source/syntax/         lexer, parser, AST
bootstrap/source/discovery/      finding classes, loading roots, merging reopened classes
bootstrap/source/analysis/       types, classes, functions, templates
bootstrap/source/generation/     C generation
bootstrap/seed/spite_compiler.c  the committed fixpoint
library/                         Spite.Class, Spite.Attribute and the rest, as ordinary Spite source
runtime/                         the C runtime the generated code includes
```

`library/` is the beginning of the standard library becoming Spite rather than hand-written C -- see
manual.md's "Pure Spite: dissolving the runtime" for where that ends.

## What the compiler does not do yet

The language is larger than the subset the compiler currently implements. `bootstrap/COMPILER_PLAN.md` is the
progress log and says exactly what is missing; `conformance/` says exactly what works, because every program in
it must pass.
