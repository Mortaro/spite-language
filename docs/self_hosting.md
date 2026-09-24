# Self hosting

Spite's compiler is written in Spite and compiles itself. There is no other implementation.

## How it builds

`bootstrap/seed/spite_compiler.c` is the C that the compiler emits for its own sources. Compiling that file with
any C compiler gives you a working Spite compiler, which can then compile the sources again and produce the same
C -- a fixpoint.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
./spite bootstrap/spite_compiler.spite --mode=c > next.c    # equal to the committed seed
```

So the only thing needed to build Spite from nothing is a C compiler. The seed is committed, and `check.sh` says
when it has drifted from the sources (`bash check.sh --update-seed` refreshes it after an intended change). The
compiler's entry is named by its file, `bootstrap/spite_compiler.spite`, since its folder is not named after it.

## What proves it

`bash check.sh`, which needs nothing but a C compiler:

1. builds the seed compiler;
2. has it compile the compiler sources (generation 2), and has generation 2 compile them again (generation 3):
   the two must be **byte identical**, or one more generation must settle it;
3. runs every program in `conformance/` and `examples/`, requiring its exact output and balanced allocations;
4. runs the test package in `tests/` ([testing.md](testing.md)), which must print nothing and balance;
5. requires the compiler to free everything it takes while compiling itself;
6. compiles every program in `diagnostics/`, which must fail with exactly the errors written beside it;
7. runs every titled program on these pages, and replays every remote REPL session on them;
8. prints two programs back out with `--final_classes` and requires the printed programs to run the same;
9. writes the compiler out for Windows, Linux and macOS, holding each operating system's library folder to
   compiling;
10. requires every `.spite` file outside `diagnostics/` to be formatted already.

A generation that cannot reproduce itself is not a compiler for this language, so step 2 is the real test; the
rest is what keeps the language honest about what it says it does.

## Layout

```
launcher/launcher.spite          loads the standard library, the target system's folder, then the program
library/                         the standard library, in Spite: String, List, the numbers, File, Json, ...
library/spite/                   reflection: Spite.Class, Spite.Function and the rest
library/windows|linux|mac/       what each operating system does differently, as reopened classes
bootstrap/spite_compiler.spite   the compiler's entry class
bootstrap/source/syntax/         lexer, parser, syntax tree, formatter
bootstrap/source/discovery/      finding classes, following loads, merging reopened classes
bootstrap/source/analysis/       types, classes, functions, templates
bootstrap/source/generation/     C generation, and prelude.spite: the C the compiler supplies, beside its Spite
bootstrap/seed/spite_compiler.c  the committed fixpoint
```

What stays C is only what the compiler emits itself: the object header, retain and release, the few members of
`Memory`, `DynamicLibrary` and the numbers declared without a body, and `main`, which constructs the launcher.
Everything a program can name is Spite in `library/`.

## What the compiler does not do yet

The language is larger than what the compiler implements. Each page here says in a line what is decided but not
built, and links the [manual](../manual.md) section; `bootstrap/COMPILER_PLAN.md` is the compiler's own progress
log.
