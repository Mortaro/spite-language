# Spite

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans: one way to do
each thing, no macros, metaprogramming and a real standard library instead of loops. It compiles to C, and the
compiler is written in Spite and compiles itself. The compiler is also its own formatter and linter -- there is
no separate style guide to follow, it rewrites your file to the one true style and refuses a naming problem
outright instead of silently accepting it.

A file is a class named after it, and a program is a folder: `arena/arena.spite` is the entry, and its constructor
runs the program.

```gdscript title=arena/monster.spite
var name = ""
var health = 0

func Monster(new_name: String, new_health: Integer) {
    name = new_name
    health = new_health
}

func alive(): Boolean {
    return health > 0
}
```
```gdscript title=arena/arena.spite entry
var console = Console()

func Arena() {
    var monsters = [Monster("slime", 12), Monster("ghost", 0), Monster("troll", 30)]
    var standing = monsters.filter_alive().sum_health()
    console.print("health still standing:", standing)
    var troll = monsters.last()
    crash troll
    show_attributes(troll)
}

func show_attribute(attribute: Symbol<Monster>, monster: Monster) {
    console.print(attribute.name, "=", monster.attributes[attribute])
}
```
```output
health still standing: 42
name = troll
health = 30
```

Nobody wrote `filter_alive` or `sum_health`. `List` has templates, `filter_<member>()` and `sum_<member>()`, and
the compiler writes the two this program calls for `Monster`, then fuses the chain into one loop with no list in
between. `show_attributes` is the same idea turned on a class: `show_attribute` takes a `Symbol<Monster>`, so the
plural calls it once for every attribute of `Monster`, each call a typed function the compiler wrote. Nothing is
looked up while the program runs, and whatever it does not call is not in the executable
([docs/collections.md](docs/collections.md), [docs/metaprogramming.md](docs/metaprogramming.md)).

## Reading order

[`docs/`](docs/README.md) is both the tutorial and the definition of the language: each page teaches one part and
ends with its rules in full, and every titled program on it is compiled and run by `bash check.sh`. Read it in
this order, from a first program to the compiler's internals:

1. [Getting started](docs/getting_started.md) -- build the compiler, run hello world, a program of two classes.
2. [Classes and files](docs/classes_and_files.md) -- a file is a class; what it holds and in what order;
   constructors and singletons.
3. [Programs](docs/programs.md) -- the entry file, the launcher, `Arguments`, `Environment` and `Build`.
4. [Values and types](docs/values_and_types.md) -- numbers and the casting rule, `String`, enums, unions, `type`
   shapes.
5. [Nullable values and failure](docs/failure.md) -- `T?` and narrowing, `assert`, `crash`, and nothing else.
6. [Functions and operators](docs/functions_and_operators.md) -- function values, variadic arguments, operators
   as functions.
7. [Control flow](docs/control_flow.md) -- `if`, `while` (the only loop), `switch`.
8. [Style](docs/style.md) -- the compiler is the formatter and the linter: names, comments, nothing unused.
9. [Memory](docs/memory.md) -- reference counting, `copy`, `drop`, `Memory.Address`, `Memory.Heap` and choosing
   an allocator.
10. [Metaprogramming](docs/metaprogramming.md) -- Symbol codegen, generics and codegen values, tree shaking.
11. [Reflection](docs/reflection.md) -- `Spite.Class`, `Spite.Function`, namespaces, instances.
12. [Packages](docs/packages.md) -- `load`, namespaces, reopening classes (mods).
13. [Concurrency](docs/concurrency.md) -- `Concurrent` and `Parallel`, without `async`/`await`.
14. [Standard library](docs/standard_library.md) -- `String`, files, folders, processes, the console, sockets.
15. [Collections](docs/collections.md) -- `List`, `Dictionary` and member templates.
16. [JSON and binary](docs/json.md) and [Time](docs/time.md).
17. [Foreign libraries](docs/foreign_libraries.md) -- `DynamicLibrary` and one folder per operating system.
18. [Targets](docs/targets.md) -- planned: the web and isomorphic classes.
19. [The compiler](docs/compiler.md), [the REPL and live reload](docs/repl.md), [testing](docs/testing.md),
    [optimizations](docs/optimizations.md) and [self hosting](docs/self_hosting.md).
20. [Decisions](docs/decisions.md), [open questions](docs/open_questions.md) and
    [known issues](docs/KNOWN_ISSUES.md) -- why each rule is what it is, and what is not settled or not built.

Writing Spite with an AI? Paste [docs/for_ai_writers.md](docs/for_ai_writers.md), the whole language on one
dense page, into its context first.

## Status

The docs say, heading by heading, what is implemented, partial or planned; in short:

| Area | Status |
|---|---|
| Files as classes, `singleton` and `generic $name` header lines, enforced file order | implemented |
| Values, numbers as classes (`this`, `from_type` casts), `T?` narrowing, `assert` and `crash` | implemented |
| Functions as values, variadic `...args: List<T>`, operators as functions | implemented |
| Enums, unions, shapes (`type`), `value == Class` tests | implemented |
| Metaprogramming: `Symbol` templates, `Symbol<Class>`, compile-time type tests, fused member-template chains | implemented |
| Reflection (`Spite.Class`, `Spite.Attribute`, `Spite.Function`, `Spite.Namespace`), read-only | implemented |
| Memory: reference counting, the `Memory` namespace (`Memory.Address`, `Memory.Heap`, `Memory.Arena`), an allocator per object, `TypedMemory`, tree-shaken output | implemented; `Vector<T>` and `Memory.Frame` planned |
| `Build` (compile time) and `Environment` (run time), the visible launcher | implemented |
| Standard library in Spite: `String`, `List`, `Dictionary`, `JsonWriter`/`JsonReader`, `BinaryWriter`/`BinaryReader`, `File`, `Directory`, `Watcher`, `Process`, `Program`, `Console`, `Socket`, time (`Instant`, `Date`, `TimeZones`) | implemented |
| Concurrency: `Concurrent` (compile-time state machines, hidden async IO) and `Parallel` (the thread pool), singletons made safe by the compiler | implemented on Windows; checking what a `Parallel` function reaches is not built |
| Foreign libraries (`DynamicLibrary`), one folder per operating system | implemented; Linux and macOS folders compile but have never run |
| REPL: `--repl`, `--repl-port`, `spite connect`; live reload (`--hot-reload`) | implemented; live reload runs on Windows ([docs/repl.md](docs/repl.md)) |
| Self hosting | done: the compiler is Spite, and the only hand-written C is `bootstrap/source/generation/prelude.spite` |
| Web target, isomorphic classes | planned -- see [docs/targets.md](docs/targets.md) |

Decisions waiting on Mortaro are collected in [`mortaros_missing_decisions.md`](mortaros_missing_decisions.md).

## Build and run

Spite compiles itself. `bootstrap/seed/spite_compiler.c` is the committed fixpoint C, so building the compiler
needs nothing but a C compiler.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
./spite examples/hello
```

Check the compiler: `check.sh` builds the seed, requires generation 2 and generation 3 to be byte identical,
runs every program in `conformance/` requiring exact output and balanced allocations, and reports whether the
committed seed is current (`bash check.sh --update-seed` refreshes it). It uses the first of `cc`, `clang` or
`gcc` it finds, or whatever `CC` names.

```
bash check.sh
```

## Write your own

Build the compiler once, then point it at a program's folder:

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
export CC=cc                     # the compiler shells out to this to build the C it emits
./spite path/to/folder
```

That builds the executable beside the program, `path/to/folder/folder.exe`, and runs it. The compiler reads the
whole program first and then produces every output asked for: `--c-source` also writes `folder.c` beside it,
`--run=false` runs nothing, and `--executable-path=` and `--c-path=` put either somewhere else
([docs/compiler.md](docs/compiler.md)).

On Windows the C compiler usually lives inside Visual Studio rather than on `PATH`, and its path
contains spaces, so use the short form:

```
CL="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang.exe"
"$CL" -O2 -Wno-parentheses-equality -Wno-deprecated-declarations bootstrap/seed/spite_compiler.c -o spite.exe
export CC="$(cygpath -d "$CL") -Wno-deprecated-declarations"
./spite.exe path/to/thing
```

**Put each program in its own folder.** `spite arena` loads the whole `arena` folder ([docs/packages.md](docs/packages.md)), so
every `.spite` file in it is part of the same program.

**Save as UTF-8 without a byte order mark.** The lexer rejects a file that starts with one, and PowerShell's
`Set-Content -Encoding utf8` writes one by default: use `-Encoding utf8NoBOM`, or an editor set to UTF-8
without BOM.

**`docs/` is the ground truth, for what works today and for what the language is**: every titled program in it,
and every program in `conformance/` and `examples/`, is compiled and checked by `bash check.sh`, and each page's
rules in full also record what is decided but not built yet, marked as such.

## Where to look

- [`AGENTS.md`](AGENTS.md) -- how to work in this repository: gitmoji commits, how decisions are recorded,
  and the conventions for shared files when more than one agent is running.
- [`SPITE.md`](SPITE.md) -- things that cause Mortaro spite, with what to do instead. Read it before proposing
  a language feature or a way of working; it is the point of the project.
- [`docs/`](docs/README.md) -- the language, normative: one page per topic that teaches it and then states its
  rules in full, the [decision log](docs/decisions.md), and a dense cheat sheet meant to be pasted into an AI's
  context (`docs/for_ai_writers.md`). When anything else disagrees with the docs, the docs win. Every titled
  Spite code block in `docs/` is compiled and checked as part of `bash check.sh`.
- [`PLAN.md`](PLAN.md) -- implementation milestones, decisions made where the docs were silent, and what is
  left.
- [`bootstrap/COMPILER_PLAN.md`](bootstrap/COMPILER_PLAN.md) -- the compiler's own plan and progress log:
  what it implements today, and what it does not.
- [`examples/`](examples/) -- idiomatic sample programs the end-to-end test suite also runs.
