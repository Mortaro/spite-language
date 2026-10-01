# Self hosting

Spite's compiler is written in Spite and compiles itself. There is no other implementation.

## How it builds

`bootstrap/seed/spite_compiler.c` is the C that the compiler emits for its own sources. Compiling that file with
any C compiler gives you a working Spite compiler, which can then compile the sources again and produce the same
C: a fixpoint.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite -lm
./spite bootstrap --c-source --run=false    # writes bootstrap/bootstrap.c, equal to the seed when it is current
```

So the only thing needed to build Spite from nothing is a C compiler. `-lm` links the C library's maths, which
the compiler calls to fold maths on constants ([optimizations.md](../docs/optimizations.md#maths-on-constants-is-worked-out-while-compiling));
Linux and macOS keep it apart, and Windows needs no flag. The compiler it gives finds `launcher/` and
`library/` from its own executable, so the executable lives in the repository or a folder inside it. The seed is
committed, and `check.sh` says when it has drifted from the sources (`bash check.sh --update-seed` refreshes it
after an intended change). The compiler is a program like any other, named by its folder: `bootstrap/`, whose entry
is `bootstrap/bootstrap.spite` (class `Bootstrap`). Its C goes to the default place,
`.spite/build/bootstrap/bootstrap.c` (D283), because every `Build` field is a constant in what is built: a `--c-path` naming some other file would be written into the C, and the next
generation would differ ([compiler.md](../docs/compiler.md#outputs)).

## What proves it

`bash check.sh`, which needs nothing but a C compiler:

1. builds the seed compiler;
2. has it compile the compiler sources (generation 2), and has generation 2 compile them again (generation 3):
   the two must be **byte identical**, or one more generation must settle it;
3. runs every program in `conformance/` and `examples/` with `--debug-memory`, requiring its exact output and
   balanced allocations;
4. runs a program from another folder, which must open its relative paths there and be built into that folder's
   `.spite/` and never beside its source, builds a program that loads a local repository pinned to a commit, and
   runs the thread pool without `--debug-memory`;
5. reads the C of production builds: `examples/hello` must carry no library class it never uses, and the
   singletons a `Parallel` reaches must take their cheapest safe form ([optimizations.md](../docs/optimizations.md));
6. runs the test package in `tests/` ([testing.md](../docs/testing.md)), which must print nothing and balance;
7. requires the compiler to free everything it takes while compiling itself;
8. compiles every program in `diagnostics/`, which must fail with exactly the errors written beside it;
9. runs every titled program on these pages and in the README, replays every remote REPL session on them, and
   runs [repl.md](../docs/repl.md)'s live-reload session, editing a copy of its program while it runs;
10. prints two programs back out with `--final-classes` and requires the printed programs to run the same;
11. writes the compiler, a time-zone program and a file-watching program out for Windows, Linux and macOS,
    holding each operating system's library folder to compiling;
12. requires every `.spite` file outside `diagnostics/` to be formatted already;
13. says whether the committed seed is current (`--update-seed` replaces it with generation 2).

A generation that cannot reproduce itself is not a compiler for this language, so step 2 is the real test; the
rest is what keeps the language honest about what it says it does.

## Layout

```
bin/spite                        the command: builds the compiler from the seed, then runs it
launcher/launcher.spite          loads the standard library, the target system's folder, then the program
library/                         the standard library, in Spite: String, List, the numbers, File, JsonWriter, BinaryWriter, ...
library/spite/                   reflection: Spite.Class, Spite.Function and the rest
library/windows|linux|mac/       what each operating system does differently, as reopened classes
bootstrap/bootstrap.spite        the compiler's entry class, Bootstrap
bootstrap/source/syntax/         lexer, parser, syntax tree, formatter
bootstrap/source/discovery/      finding classes, following loads, merging reopened classes
bootstrap/source/analysis/       types, classes, functions, templates
bootstrap/source/generation/     C generation, and prelude.spite: the C the compiler supplies, beside its Spite
bootstrap/seed/spite_compiler.c  the committed fixpoint
conformance/ examples/ tests/    programs with their exact output, which must balance their allocations
diagnostics/                     programs that must fail, with their exact errors
scripts/docs_corpus/             writes every titled program of docs/ and the README out for check.sh
```

What stays C is only what the compiler emits itself: the object header, retain and release, `main`, which
constructs the launcher, and the members the compiler supplies to a few library classes as its own reopening,
listed in `prelude.spite`: `Console`, `Memory.Heap`, `Memory.Address` (its reads, writes and atomics, lowered
where they are called, D178), `TypedMemory`, `DynamicLibrary`, `HotReload`, `Concurrent`, `ThreadPool`,
`Scheduler`, the reflection members of `Spite.Attribute` and `Spite.Function`, and the numbers. `--final-classes`
prints them as declarations without a body ([compiler.md](../docs/compiler.md#inspect-merged-classes)). Everything a
program can name is Spite in `library/`, and the supplied members are meant to shrink to the machine's own
operations ([the rule](#self-hosting--partial)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Self hosting  **[partial]**

- The compiler is written in Spite and compiles itself; there is no other implementation.
  `bootstrap/seed/spite_compiler.c` is the C it emits for its own sources, committed, so a C compiler is all that
  is needed to build it.
- **The fixpoint is the test.** `bash check.sh` requires generation 2 (the seed compiling the sources) and
  generation 3 (generation 2 compiling them again) to be byte identical, or nothing else it checked is believed; when
  they differ, one more generation must settle it (a change to how the compiler compiles its own source), or the
  check fails with `FAILED: no fixpoint, generation 3 and 4 still emit different C`. A seed older than the sources
  is not a failure: the check ends `OK: fixpoint holds, but the compiler sources changed since the seed was
  written: run  bash check.sh --update-seed`.
- The compiler is held to what it asks of programs: compiling itself under `--debug-memory` it frees everything it
  takes, and every file of it is formatted.
- **What stays C** (D147, decided by Mortaro: zero hidden code, nothing that ties Spite to C) is only what the
  compiler emits itself and the members `prelude.spite` supplies; D178's primitives on `Memory.Address` are the
  part each backend is meant to keep. Moving the other supplied members into Spite is **not finished**.
- The language is larger than the part the compiler implements today; each page says what is decided but not
  built, `bootstrap/COMPILER_PLAN.md` is the compiler's progress log, and `conformance/` is the part that
  demonstrably works.
