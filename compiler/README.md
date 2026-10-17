# The Spite compiler

The compiler is written in Spite and compiles itself. This page is for people who want to work on it: the path a
program takes from its source to an executable, what each step is responsible for, and what it must leave to the
others. It describes the compiler in its final form. The language itself is taught in [docs/](../docs/README.md)
and specified in [specs/](../specs/README.md); the rules for every change (style, commits, documentation, what may be
pushed) are in [AGENTS.md](../AGENTS.md).

- [The flow](#the-flow)
- [The steps](#the-steps)
- [What every step keeps to](#what-every-step-keeps-to)
- [Where the code is](#where-the-code-is)
- [The two intermediate representations](#the-two-intermediate-representations)

## The flow

```
command line
  -> discovery          which files make the program, and its build settings
  -> syntax             each file's text as a syntax tree
  -> declarations       every class, function, field, enum and union the program declares
  -> lowering           the high IR, with every name resolved and every check made while it is built
  -> high passes        every decision about the program: proofs, memory, layout, threads, what is reached
  -> lowering to low    the low IR: flat, explicit memory and control flow
  -> code generation    machine code and our own linker, or C for a system without a backend
  -> the executable     run, tested, reloaded or inspected
```

Each step takes the output of the step before it and nothing else. Nothing reads its own output back, no step reads
C or machine code to learn about the program, and a decision is made in exactly one step.

## The steps

### Command line

`compiler/compiler.spite`. Reads the arguments (`spite <program> [--flags]`), the build settings a program gives in
its `Build` fields, and decides what to do: build and run, build only, check only, format, test, start a REPL, or
reload a running program. It owns the errors about the command line itself and nothing about the program.

### Discovery

`compiler/source/discovery/`. Starting from the program's folder, finds every file that makes the program: a file
is a class ([classes_and_files](../specs/classes_and_files.md)), a `load` line brings in a folder or a package
([packages](../specs/packages.md)), and the standard library is loaded through the launcher
(`launcher/launcher.spite`) with the folder of the system being built for. It owns the errors about a program's
shape: names of files and folders, what may be loaded, what is internal to the standard library.

### Syntax

`compiler/source/syntax/`. The lexer and the parser turn each file into a syntax tree. Spite has one layout, so the
formatter rewrites a file into it and the compiler says so ([style](../specs/style.md)). It owns the errors of
spelling and layout.

### Declarations

`compiler/source/analysis/`. Collects what every file declares: classes with their fields and functions, enums,
unions, templates and their parameters. It answers questions about declarations and decides nothing about bodies.

### Lowering: the high IR

`compiler/source/intermediate/` (the lowering, `model/` and `checks/`). Walks every function the program reaches,
starting from its entry class, and builds the [high IR](high_ir.md):

- **Names and types.** Every name is resolved and every expression typed here, by the rules of the
  [specification](../specs/README.md). Generic classes and templates are instantiated for the types they are used
  with, so everything in the high IR is concrete.
- **Checks.** Every compile error is raised here, at the place the IR for that code is built, with the exact text its
  spec page gives; the first error stops the compile. A rule that needs a fact only a pass establishes (a lock, a
  thread, a condition decided while compiling) is raised by that pass, with the same exactness.
- **Desugaring.** Every operator, index, interpolation, `switch` and `assert` becomes the call or branch it means.
  What the language can say as a call of a function stays that call; the passes decide what it becomes.
- **Compile-time work.** Reflection over what is known while compiling, templates and values folded while compiling
  are worked out here, so the program carries only the result.

The high IR is held for the whole program and cached per function, so a later build lowers again only what changed
([D569](../DECISIONS.md)).

### High passes

`compiler/source/intermediate/` (the pass folders). Every decision about how the program runs is made here, as a
rewrite of the high IR, in a fixed order: inlining, proofs, ownership, placement, arrangement, concurrency, folding,
and reach (the tree shaker). A verifier checks the IR after every pass. What each pass decides and when it does not
apply is in [high_ir.md](high_ir.md#the-passes); what a user can observe of it is documented in
[optimizations](../docs/optimizations.md) and [proofs](../docs/proofs.md).

### Lowering to the low IR

Turns the high IR, after its passes, into the [low IR](low_ir.md): classes become sizes and offsets, a count, a
frame object, a check or a wait becomes the loads, stores, branches and calls it is made of, and every value has a
machine type. This step decides nothing about the program: every choice it carries out was made by a high pass.

### Code generation

From the low IR, for each system Spite has a backend for: instruction selection, register allocation, the encoder
(`compiler/source/backend/x86/` for x86-64), our own object format, our own linker, and the writer of the system's
executable (Windows, Linux, macOS). A development build favours the speed of the build; an optimised build uses the
facts the passes proved. For a system without a backend, the low IR is printed as C and compiled by a C compiler,
which is also how Spite builds itself from source the first time ([self_hosting](../design/self_hosting.md)).

### The executable

A built program is run, its tests are run while compiling it ([testing](../specs/testing.md)), and a development build
can be inspected and reloaded while it runs ([repl](../specs/repl.md)). A crash reports from the table of sites the
lowering recorded, so it names its cause wherever the check ended up after inlining and folding.

## What every step keeps to

- **One decision, one place.** A step that finds it needs a fact another step owns asks for it; it never works it out
  again.
- **Zero runtime.** Whatever can be worked out while compiling is; the program carries no interpreter, scheduler or
  registry, and a feature a program does not use leaves nothing in it ([SPITE.md](../SPITE.md)).
- **Loud failure.** A construct a step cannot handle is refused by name, never passed on as something else
  ([D244](../DECISIONS.md)). A wrong instruction is worse than a slow build.
- **Nothing only for a human to read.** The IRs have no pretty forms beyond what a test or a diagnostic needs; what a
  crash report or an error must name stays ([D561](../DECISIONS.md)).

## Where the code is

| Folder | Step |
|---|---|
| `compiler/compiler.spite`, `compiler/build.spite`, `compiler/environment.spite` | command line, settings |
| `compiler/source/discovery/` | discovery |
| `compiler/source/syntax/` | syntax and formatting |
| `compiler/source/analysis/` | declarations |
| `compiler/source/intermediate/` | lowering, the high IR and its passes |
| `compiler/source/backend/` | the low IR, code generation, objects, linking, executables |
| `compiler/source/translation/` | splitting C into units and caching them, for the C path |
| `compiler/seed/` | the C the compiler writes for itself, to build it from source |
| `compiler/proof/` | what proves the compiler builds itself |
| `launcher/` | the program that loads the standard library and the user's program |
| `library/` | the standard library, written in Spite |

## The two intermediate representations

- [The high IR](high_ir.md): the whole program as Spite's own operations, concrete and desugared, where every
  optimisation and proof is made.
- [The low IR](low_ir.md): flat functions of machine-typed instructions on virtual registers, explicit about memory
  and control flow, which every code generator reads.
