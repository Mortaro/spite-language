# Compiler command line

`spite` compiles one program. A program is a folder, and its entry file is the one named after the folder:
`spite game` compiles `game/game.spite`, whose class `Game` is constructed to start the program. The compiler
first reads the **whole** program -- the launcher, the standard library, the program's folder, its `build.spite`
and every folder it `load`s -- and only then decides what to produce, from the program's
[`Build`](programs.md#compile-time-settings-build). Every option is a field of `Build`, given as `--name=value`
(a `Boolean` option may be given bare, `--optimized`). A flag is written in kebab-case and sets the `snake_case` field
of the same name: `--repl-port=4000` sets `Build.repl_port`, and a field a program declares in its own
`build.spite`, `worker_stack_size`, is given as `--worker-stack-size=256` ([D188](#flags-and-settings)). By default
the compiler builds the executable into `.spite/` in the folder you run it from, and runs it: nothing is written
beside the program's source unless a path says so ([D283](#where-the-outputs-go)).

```
spite game                          build .spite/build/game/game.exe, and run it
spite game --debug-memory           the same, counting allocations and frees
spite game --c-source               also write .spite/build/game/game.c
spite game --run=false              only check that it compiles
spite game --repl-port=4000         serve a REPL on 127.0.0.1:4000 while it runs
spite game -- --name=production     run it with a setting its Environment declares
spite format game                   format files without compiling them
spite connect 4000                  talk to a program running with --repl-port=4000
```

Every command and flag, with exactly what it does, is listed under [Command line](#command-line). Flags mix
freely -- a production build may keep the REPL -- and the outputs combine: one compile can build the executable,
write its C and write its final classes.

`bin/spite` (and `bin/spite.cmd`, which runs it from a Windows prompt) is the command itself: it builds the
compiler from `bootstrap/seed/spite_compiler.c` into `.spite/spite.exe` the first time, and again whenever the
seed is newer, finds a C compiler, makes the folder and path arguments absolute, and passes everything else
through. What follows `--` reaches the program exactly as it was typed, from bash or from PowerShell or `cmd`
through `spite.cmd`: `spite tool -- --prefixes=/Game/Legacy/` gives the program `--prefixes=/Game/Legacy/`, not
the path Git for Windows' bash would make of it ([the rule](#the-launcher-passes-the-programs-arguments-untouched)).

## Name the program

```bash
spite game
spite game/
```

The folder supplies the program's other classes, and the entry class's constructor takes no arguments: the
program reads its command line through `Environment()`, what its build decided through `Build()`, and the raw
arguments through `Arguments()`. A program is named only by its folder (D130): `spite game/game.spite` is an
error that answers with the folder form, `spite game`, and a folder with no entry file named after it is an error
naming the file it looked for ([the texts](#naming-a-program)). The compiler compiles itself the same way,
`spite bootstrap`, whose entry is `bootstrap/bootstrap.spite` ([self_hosting.md](self_hosting.md)).

## How a program is loaded

The compiler reads `launcher/launcher.spite` first -- a Spite class whose constructor loads `library/`, then the
target operating system's folder of it, then the program's folder -- and follows its `load` lines in order.
[programs.md](programs.md#how-a-program-is-loaded) walks through it.

## Where it runs

The compiler finds `launcher/` and `library/` from its own executable, looking in each folder above it, and
never from the working directory. So `spite` runs, and the program it builds runs, in the folder you ran it from:
a relative path the program opens -- `File("save.txt")`, `Directory("levels")`, a cache folder -- is relative to
that folder, not to the language's repository. A foreign library's header named by a relative path
(`DynamicLibrary`'s third argument) is looked for in the working directory as well.

## Choose the outputs

The compiler reads the program first, and decides what to produce only after, from `Build` -- so a program's own
`build.spite` can choose its outputs like any other option ([programs.md](programs.md#compile-time-settings-build)).
There are four, and every one that is on is produced by the same compile: `run` (on by default: build the
executable, then run it with everything after `--`), `executable` (build it without running it), `c_source` (write
the generated C) and `final_classes` (write the program back out as Spite into a folder,
[below](#inspect-merged-classes)); their defaults are in [Build options](#build-options).

Running a program runs its executable, so `run` builds it too, in the same place `executable` would. With every
output off (`--run=false`), the compiler still reads and compiles the whole program and reports its errors, and
writes nothing but the formatting, which is not an output: every compile does it first ([below](#formatting)):

```bash
spite game                                      # build .spite/build/game/game.exe and run it
spite game --c-source                           # the same, and write .spite/build/game/game.c to read
spite game --executable --c-source --run=false  # build the executable and write the C, run nothing
spite game --run=false                          # only check that it compiles
```

Checking writes no executable, so one an earlier build left in `.spite/build/` is still there and still runs the
old code: to build without running, turn `executable` on, `spite game --executable --run=false`. Whether
`--run=false` alone should build instead is waiting on Mortaro (`mortaros_missing_decisions.md` item 212).

Whole-program steps -- tree shaking, the constants `Build` folds, which templates are made -- run on the complete
program before any output is written, so the C written with an executable is the C that executable was built
from.

In that C, every local, parameter and attribute has a `_` after its Spite name (`var near = 3` is
`int32_t near_ = 3;`, an attribute `pascal` is `self->pascal_`), and a function is its class's name joined to its
own (`Map_far`). No snake_case name ends in `_`, so no Spite name can meet a C keyword or a macro from a system
header (Windows defines `near`, `far` and `pascal`): C reserves nothing in Spite (D168). Reflection,
`--final-classes`, crash reports and error messages keep the Spite names.

## Where the outputs go

**Nothing is written beside the program's source unless a path says so** (D283). Each output has its own path
option, and without one it goes into `.spite/`, a folder inside the folder you ran `spite` from -- the folder a
relative path the program opens is read from ([above](#where-it-runs)). `.spite/` holds only what the compiler
writes, and can be deleted whenever you like:

| Output | Path option | Default |
|---|---|---|
| the executable | `executable_path` | `.spite/build/game/game.exe` (no `.exe` on Linux and macOS) |
| the C | `c_path` | `.spite/build/game/game.c` |

The folder under `.spite/build/` is the program folder's own path from the folder you ran `spite` in, so
`spite examples/hello` builds `.spite/build/examples/hello/hello.exe`, and two programs named alike never share an
executable. A program outside that folder (`spite ../other/game`, or an absolute path somewhere else) is built into
`.spite/elsewhere/game_<number>/game.exe`, the number worked out from the program folder's whole path the way the
C's is below. `spite game` builds and runs it from there, so where it lands matters only when you want to keep it:

```bash
spite game --executable --run=false --executable-path=build/game.exe
spite game --c-source --run=false --c-path=build/game.c
```

`.spite/git/` holds the checkouts of the repositories a program's `load`s pin to a commit
([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)). In the language's own repository, `.spite/`
also holds the compiler `bin/spite` builds, the intermediate C and objects below, and `check.sh`'s work folders. No
`.spite/` folder is ever read as part of a program, and neither is `.spite-cache/`, its name before D283, which can
be deleted.

A folder the path names is created. A path option given for an output that is off is an error saying which flag
is missing (`--c-path` without `--c-source`, [the texts](#outputs)), so a path never passes silently. Every `Build`
field is a constant in the built program, the paths included.

What a build writes **beside its executable**, wherever that is (`.spite/build/game/` unless `--executable-path`
moves it): `game.crashes`, the map from a crash report's
site ids to their lines ([failure.md](failure.md)), and, for a `--hot-reload` build, `game.reload_host`,
`game.reload_files` and each reload's `game_reload_1.dll` with its C ([repl.md](repl.md#live-reload---hot-reload)).
The one intermediate that is not an output is the C the C compiler reads when `--c-source` is off: it goes to the
language repository's `.spite/game_<number>.c`, the number worked out from the executable's whole path, so
two builds of programs named alike in different folders at once never compile each other's C; it is overwritten
by the next build of the same executable (D260). A C compiler that reports success without writing the
executable is an error, `the C compiler reported success but '<path>' is not there: nothing was built, so nothing
runs`, never a build that ends with nothing to run. A program built from [translation units](#translation-units-the-c-compiled-in-parallel-and-cached) writes its units,
their header and their objects to `.spite/objects` instead, each named by the hash of its content.

`spite reload <folder> ... --executable-path=<running executable>` is what a `--hot-reload` program runs to
rebuild itself: given the options it was built with, it compiles only the classes whose files changed into a
library beside the executable and prints what it rebuilt, or its errors, on standard output, where the running
program reads them ([repl.md](repl.md#live-reload---hot-reload)). You never type it.

## Build options

Every option is a `Build` field with a literal default, declared in `library/build.spite`:

| Option | Default | What it does |
|---|---|---|
| `run` | `true` | builds the executable and runs it |
| `executable` | `false` | builds the executable |
| `executable_path` | `""` | where the executable goes; `""` is `.spite/build/<program>/` in the working folder ([above](#where-the-outputs-go)) |
| `c_source` | `false` | writes the generated C |
| `c_path` | `""` | where the C goes; `""` is `.spite/build/<program>/` in the working folder |
| `final_classes` | `""` | writes the merged classes to this folder |
| `optimized` | `false` | a release build: `-O3` instead of `-O0`, and link-time optimisation across the [translation units](#translation-units-the-c-compiled-in-parallel-and-cached) |
| `tune_for_this_machine` | `false` | lets the C compiler use every instruction this machine has (`-march=native`, or `-mcpu=native` on ARM); the executable may not run on an older processor ([below](#release-builds)) |
| `translation_units` | `0` (chosen) | how many C files the executable is compiled from: `0` is one file in a default build and, in an `--optimized` one, a number chosen by the size of the C and the processors; `1` is one file ([below](#translation-units-the-c-compiled-in-parallel-and-cached)) |
| `development` | `false` | an [inspectable build](#development-builds-and-tree-shaking): nothing tree-shaken, internals as ordinary objects |
| `repl` | `false` | runs the program, then opens a REPL on it at the terminal ([repl.md](repl.md)) |
| `repl_port` | `0` (off) | serves the remote REPL on `127.0.0.1` at this port while the program runs ([repl.md](repl.md)) |
| `hot_reload` | `false` | swaps changed classes into the running program; implies `development` ([repl.md](repl.md#live-reload---hot-reload)) |
| `debug_memory` | `false` | counts allocations and frees and prints them when the program ends ([below](#counting-memory---debug-memory)) |
| `trace_asserts` | `false` | writes each failed `assert` of the program to the error stream as it fails, not only in a crash's report ([failure.md](failure.md#what-a-crash-reports)) |
| `operating_system` | the compiling machine | cannot be given: it is the system doing the compiling |
| `target_operating_system` | `operating_system` | the system the program is compiled for ([below](#compile-for-another-system)) |
| `program` | the folder named | cannot be given: it is the folder on the command line, which the launcher loads |

```bash
spite game --optimized
spite game --debug-memory
spite game --executable --run=false --executable-path=build/game.exe --debug-memory
```

A program may give an option a different default by reopening `Build` in its own `build.spite`
(`var optimized = true`, or `var c_source = true` to always write its C), and add options of its own the same
way. No option is read before the program is, so every one of them, the outputs included, can be given a default
there -- except `target_operating_system`, which comes from the flag alone ([why](#outputs)). Every `Build` field is
a constant in the built program, so with `var build = Build()` beside the attributes, `if build.debug_memory { }`
keeps one branch.

None of this is in the program that is built (D177): choosing, building and writing outputs is the compiler's own
work, and `Build` is a folded singleton that holds nothing at run time. Only five options add code to the program,
and only to a build that asks for them: `--debug-memory` its allocation table, `--repl` and `--repl-port` the REPL
and the reflection it walks, `--hot-reload` a slot per function and a file watcher, and `--trace-asserts` a write
in each failed `assert`. Each one's cost is on its
page.

The compiler hands the C it writes to the command in the `CC` environment variable when it is set; otherwise to the
first of `cc`, `clang` and `gcc` that runs. `bin/spite` keeps a `CC` that is already set, and uses it to build
the compiler from the seed too; otherwise it sets `CC` to `SPITE_CC` when that is set (one executable, spaces in
its path allowed, which wins over `CC`), else to the first of `cc`, `clang` and `gcc` on the `PATH`, or the clang
inside Visual Studio on Windows:

```bash
SPITE_CC=clang spite game --optimized
```

## Release builds

`--optimized` is the release build: the C compiler is asked for `-O3` instead of the default build's `-O0`, and
when the executable is built from several [translation units](#translation-units-the-c-compiled-in-parallel-and-cached),
for link-time optimisation too (`-flto=thin` with clang, linked by `lld` except on macOS; `-flto=auto` with gcc), so
a function in one unit is still inlined into another. The default build stays at `-O0`: it is the one you rebuild
all day, and the one the corpus runs.

`--tune-for-this-machine` (proposed by Claude, unconfirmed: the name) lets the C compiler use every instruction the
compiling machine has -- `-march=native`, or `-mcpu=native` on ARM -- in whatever build it is given to. The
executable may then stop on an older processor with an illegal instruction, so it is off by default, and a build
meant for other machines leaves it off:

```bash
spite game --optimized                          # release: -O3 and link-time optimisation
spite game --optimized --tune-for-this-machine  # the same, for this processor only
```

How fast each build is, against C written by hand, is `bash benchmarks/versus_c/run.sh`
([benchmarks/README.md](../benchmarks/README.md#spite-against-c)).

## Translation units: the C compiled in parallel and cached

A release build (`--optimized`) of a big program is built from several C files, compiled at the same time and
remembered (proposed by Claude, unconfirmed). The generated C is split into one header -- every type, macro and prototype, and an `extern` line for
each variable at file level -- and a number of units that include it. Each function goes into the unit a hash of
its name picks, so editing a function leaves every other function where it was; the variables are defined in the
first unit; the small `static inline` helpers stay in the header, a copy for each unit. The C compiler compiles as
many units at once as the machine has processors, and the objects are linked.

Each unit's object is kept in the language repository's `.spite/objects`, named by a hash of everything it
was compiled from: the C compiler's command and flags, the header and the unit. A build finds the objects it
already has and compiles only the rest, so building a program again after changing nothing only links it, and
after changing one function's body compiles one unit. What the cache cannot save: an edit that adds or removes a
function, changes a type, or adds or removes a piece of constant text changes the header (the texts are
file-level variables numbered in order), and every unit is compiled again.

How many units: `--translation-units` (`Build.translation_units`, proposed by Claude, unconfirmed) gives the
number, in any build. `0`, the default, is one file in a default build: at `-O0` the C compiler spends its time
reading, and every unit reads the whole header again, so splitting the compiler's own C made a cold `-O0` build
slower, not faster ([the numbers](../benchmarks/README.md#compile-time-at-scale)). In an `--optimized` build `0`
chooses the largest power of two that is no more than one unit per 768 KiB of C, no more than the number of
processors, and at most 64: eight units for the compiler's 7 MB of C. A program under 1.5 MB of C stays one file,
compiled as before and not cached. A `--hot-reload` build, and a program whose C
includes a foreign library's header (a `DynamicLibrary` given one), are always one file. `--c-source` still
writes the program's C as one file, which is what `check.sh` compares to prove the compiler reproduces itself.

An `--optimized` build, in seconds of the whole `spite` command on Mortaro's machine (32 logical processors,
clang 19.1.5): from one file as before, and from units with the cache empty (cold), full (warm) and after one
function's body changed:

| program | C | one file | cold | warm | one edit |
|---|---|---|---|---|---|
| the compiler | 7.2 MB | 37.7 | 14.5 | 11.4 | 13.3 |
| SlopEngine's `kal_character` | 14.1 MB | 80.1 | 45.7 | 30.7 | 67.2 |
| a generated 209 206-line program | 20.5 MB | 181.2 | 50.0 | 34.3 | 37.3 |

A warm build is the Spite compile plus a ThinLTO link, which optimises the whole program again each time; the
SlopEngine edit was in a generic class, which changes every instantiation of it. The default build of the same
three took 11.3, 26.1 and 16.5 s from one file. How they were measured, and the unit counts tried, are in
[benchmarks/README.md](../benchmarks/README.md#compile-time-at-scale); `bash benchmarks/build_times.sh` measures
them again.

## Compile for another system

`--target-operating-system=linux` (or `windows`, or `mac`) compiles for that system from any machine: it loads
`library/linux/` instead of this machine's folder, and `build.target_operating_system` is `"linux"` in the
program. `check.sh` uses it to hold the folders it cannot run to compiling:

```bash
spite bootstrap --c-source --run=false --c-path=compiler_linux.c --target-operating-system=linux
```

## Pass settings to the program

Everything after the first bare `--` belongs to the program, not the compiler. A program reads its run-time
settings from the `Environment` singleton, whose fields it declares by reopening `Environment` in its own
`environment.spite` (see [programs.md](programs.md#run-time-settings-environment)):

```bash
spite server -- --name=production
```

Before the `--`, a `--name=value` sets a `Build` field -- the compiler's own options, or one the program declares
in its `build.spite` -- and is written into the build as a constant:

```bash
spite server --serve=true -- --name=production
```

A flag naming a field of `Environment` is an error that says to pass it after `--`, and a flag naming no field
at all is an error listing the fields `Build` has, so typos never pass silently.

## Formatting

**The compiler is the formatter, and every compile formats first** (D190). Once the whole program has been read,
each of the program's own files whose formatted text differs from what is on disk is rewritten, printing
`formatted <path>`, and the program is read again, so what is compiled, and every line an error names, is the
formatted file. Nothing turns this off, and a file the formatter refuses stops the compile: a program is never
compiled from text that is not in the one style ([the rules](#formatting-before-compiling-and-spite-format)).

**A file edited during the compile is never overwritten.** Just before writing a formatted file, the compiler reads it
again; if it no longer holds the text the compile read -- another editor or agent changed it meanwhile -- it is left
as it is now, `<path> changed while it was being compiled` is printed, and the program is read again from disk. A
file whose formatting does not change is never written, so its modification time never moves. (`spite format`
refuses such a file the same way: format it again.)

```bash
spite format game library/list.spite    # format a folder's files and one file, compiling nothing
spite format --check game                # rewrite nothing: list every file that would change, and fail
```

`spite format` runs the same formatter on files rather than on a program, so a file that does not compile yet
can still be formatted. What the formatter rewrites, and what it refuses to, is [style.md](style.md).

## Development builds and tree shaking

A normal build keeps only what the program uses: a condition on a codegen value or a `Build` field keeps one
branch, a template exists only for the names called, and reflection only where it is read
([metaprogramming.md](metaprogramming.md#tree-shaking)).

An **inspectable build** -- `--development`, `--hot-reload`, `--repl` or `--repl-port` -- keeps every generated
function instead, so live reload has all of them to swap and the REPL can reach every internal, and the
singletons that hold nothing, such as `Build`, are ordinary objects that `.instances` lists. Every other build,
ordinary or `--optimized`, is a production build ([the rule](#inspectable-and-production-builds)). Conditions on
codegen values and `Build` fields fold in both: they are facts of the build, not values the running program could
change. Which optimisation applies in which build is in [optimizations.md](optimizations.md).

## Counting memory: `--debug-memory`

`--debug-memory` builds the program with an allocation table and prints `allocations: N frees: N` when it ends.
The two numbers differ only when something leaked, and then the report names the classes whose instances are
still alive, which is how a leaked cycle shows up ([memory.md](memory.md#cycles-leak)). Every program in
`conformance/`, `examples/` and these pages is run this way by `check.sh`, and must balance. The table costs a
lookup under a lock on every allocation and free, and exists only in a `--debug-memory` build: any other build
calls the allocator directly.

## Compile time

Compiling grows linearly with the program: every whole-program step -- reading, formatting, analysis, template
instances, call effects, tree shaking -- works on each class a fixed number of times, and anything looked up by
name is found through a table, never by walking every class again. The measure is a data-heavy program: a
`Symbol<Item>` walk over a folder of small record classes, each one `fill(item)` of 10 to 30 assignments, with
`Filler<record.class>` made for each, and the items kept in a `Dictionary` keyed by number. CPU seconds of the
compiler alone (`--c-source --run=false`, so no C compiler), on Windows with clang:

| records | before | now | C written |
| ---: | ---: | ---: | ---: |
| 525 | 2.8 s | 0.75 s | 57k lines |
| 1,049 | 9.1 s | 1.2 s | 111k lines |
| 2,097 | 33.6 s | 2.2 s | 218k lines |
| 6,991 | 514 s | 7.3 s | 722k lines |

About 1 ms and 104 lines of C per record. Before, each `Filler<record.class>` looked for its record by listing
every class of the namespace, sorting them and working out each one's member name, so the walk was quadratic;
the namespace's classes, their names and where each name is are now worked out once and kept, grown only when a
class is added (`namespace_walk` in the generator). A program whose dictionary keys are learned while compiling
(D224) is generated twice; the first pass stops once it has learned them, before the C is assembled and tree
shaken, since the second pass writes it again. Every file is read once: the formatter formats the text the
compile read. The compiler compiling itself, which has neither, takes 1.7 s of CPU, as before.

Two more steps that were quadratic, found building SlopTheseus with `--hot-reload` (about 460 files; 880,000 lines
of C for its server, 1.3 million for its client): which functions can reach a wait (D209) was worked out by
sweeping every call the program makes until nothing changed, again for every question asked, and whether a
function not yet written is called through a shape looked through every union for every such function on every
sweep of the pending functions. A wait now spreads from the functions that wait to their callers once, through a
table of who calls whom that only grows, and the shape question first asks a table of the names any shape calls.
The server's whole compile went from 159 to 72 seconds (one generation pass from 48 to 17), the client's from
268 to 110 (the C compiler's share included). A reload ([repl.md](repl.md#live-reload---hot-reload))
skips the C of the whole program, which it never uses, and cuts the program's C into functions once rather than
three times.

Where a record's 104 lines go: 13 are its `fill` body; about 47 are the allocate, default, release and init
functions of the record class and of its `Filler<...>` instance, 18 of them `#ifdef` blocks for instance tracking
and weak references that the C preprocessor removes; 11 are prototypes, 7 the walk's step for that record, and
the rest structs and typedefs.

## Inspect merged classes

`--final-classes=folder` writes the discovered, merged classes as readable `.spite` files -- one per class, under
the namespace folders it belongs to. It is useful after a `load` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program, which `check.sh` proves on every run. `Build` is printed with the defaults it
was declared with (the program's own `build.spite` included), not the values this build folded, so running the
printed program takes its flags again. A repository loaded at two commits is printed once per version, each in
a folder named by the repository and the commit (`slop_engine_6c7dca9/`), since the versions are two libraries
([packages.md](packages.md#two-versions-of-one-repository)).

What is written is what the program **ends up with**, not what was written down: the classes come from the
generator after it has run, so a class the generator never made is not there, a generic template is not there,
and each of its instantiations is. The tree shaking of a production build's C runs after that, so a library class
the program never uses (`Watcher`, `Socket`) may still be printed although its C was dropped.

Every class the program names comes from a file in `library/`, including `Memory.Heap`, `DynamicLibrary`,
`String` and the numbers (`Integer`, `Long`, `Double`, `Memory.Address`, ...), so every one of them is printed like a
class of your own. A function whose body the compiler supplies -- the floor that stays C, such as
`Memory.Heap.allocate` -- is added to its class as the compiler's own reopening, and is printed as a declaration
without a body. The printed `memory/heap.spite` is:

```gdscript
singleton

func allocate(bytes: Long): Memory.Address
func resize(address: Memory.Address, bytes: Long): Memory.Address
func free(address: Memory.Address)
func live_allocations(): Integer
```

A declaration without a body is what the compiler reads back, so the printed program still compiles: the
printed `memory/heap.spite` reopens `Memory.Heap` with the same members, and the compiler supplies the same bodies
again. Anywhere else, a `func` with no body is an error, because only the compiler can supply one.

`instantiated/` holds one file per generic instantiation, named for the values it was given
(`weapon_integer_true.spite`, class `WeaponIntegerTrue`), written as a `type` with the members that instantiation has.
`Launcher` is printed with the rest.

Which root supplied each declaration is **not** shown yet. It cannot be a comment, since a comment is only ever
a link to a markdown heading ([style.md](style.md#comments-are-links)), so it needs a form of its own
([open question 10](open_questions.md#open-questions)).

The folder has no default: anything inside the program's own folder would be read back as part of the program,
so the folder is always named. Like every output, it combines with the others; `--run=false` writes only it:

```bash
spite game --final-classes=.spite/final --run=false
```

## Errors and usage

With no program, the compiler prints its usage text and exits unsuccessfully. A `.spite` file named instead of
its folder, a folder with no entry file, an entry constructor that takes arguments, a path option for an output
that is off, a flag with an underscore, and a `--name=value` before `--` that names no `Build` field are errors
too, each naming the fix; their texts are [below](#naming-a-program).

A program's own errors come first. When a package it loads is broken too -- the engine halfway through a
migration, say -- its errors are listed after the program's, so they never hide the program's own; how many of each
are listed is [below](#how-many-errors-are-listed).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Command line

Every command and flag the compiler has, as built. `program` is a folder; every flag before the first bare `--`
is a field of `Build` ([Build options](#build-options), [programs.md](programs.md#build-settings-build--implemented)).

```
spite program                           build .spite/build/program/program.exe and run it
spite program --run=false               compile the whole program and write nothing but its formatting
spite program --executable --run=false  build the executable without running it
spite program --executable-path=path    put the executable at path (with --executable, or with run on)
spite program --c-source                also write .spite/build/program/program.c
spite program --c-path=path             put the C at path (with --c-source)
spite program --final-classes=folder    also write the final classes into folder (inspect merged classes)
spite program --optimized               a release build: -O3, and link-time optimisation across translation units
spite program --tune-for-this-machine   use every instruction this machine has (-march=native); not portable
spite program --translation-units=1     compile the executable from one C file (0, the default, chooses)
spite program --development             an inspectable build: no tree shaking, internals as ordinary objects (D143)
spite program --debug-memory            count allocations and frees, print the balance when the program ends
spite program --trace-asserts           print each failed assert as it fails, as well as in a crash's report
spite program --repl                    run it, then answer REPL commands at the terminal (repl.md)
spite program --repl-port=4000          serve REPL commands on 127.0.0.1:4000 while it runs (repl.md)
spite program --hot-reload              swap changed classes into the running program (implies --development)
spite program --target-operating-system=linux   compile for another system: windows, linux or mac
spite program --serve=true              set a Build field the program declares in its build.spite (programs.md)
spite program -- ada --player=x         run it, passing everything after -- to the program (Arguments, Environment)
spite format file_or_folder ...         format files without compiling them (a folder with every folder inside it)
spite format --check file_or_folder ... rewrite nothing; list every file that would change, and exit 1 if any would
spite connect port                      talk to a program running with --repl-port=port (repl.md)
spite connect port --command="..."      send one REPL command, print its raw JSON answer line, and exit
spite reload program ... --executable-path=running   what a --hot-reload program runs to rebuild itself (repl.md)
```

A `Boolean` flag may be given bare: `--optimized` is `--optimized=true`, and `--optimized=false` stays allowed
(D120). Flags mix freely, and every output that is on comes from the same compile: an `--optimized` build may keep
the REPL (it is then inspectable), and `--executable --c-source --final-classes=folder --run=false` writes all
three. Building and running are the same command. There is no `--mode`, `--output` or `--format`: the outputs are
the four fields above, formatting is not an option, and `spite format`, `spite connect` and `spite reload` are
commands of the compiler, not options.

### Naming a program

A program is named only by its folder (D89, D130), and its entry file is the one named after the folder. The
compiler's own entry is `bootstrap/bootstrap.spite`, class `Bootstrap`, so it compiles itself as `spite bootstrap`.
With no folder named, the compiler prints its usage to the error output and exits 1.

- A `.spite` path: `error: 'game/game.spite' is a file, and a program is named by its folder: spite game`.
- A folder without its entry: `error: 'game' holds no program: a program is a folder holding an entry file named
  after it, 'game/game.spite'`.
- An entry constructor with parameters is an error at its line ([programs.md](programs.md#constructors-and-the-entrypoint);
  `diagnostics/entry_constructor_arguments`).

### Outputs

**The whole program first, then the outputs** (D128, D129; the spellings proposed by Claude, unconfirmed). The
compiler reads the launcher, the library, the program's folder, its `build.spite` and every `load` before it reads
any option, and then decides what to produce from the program's resolved `Build`, so a program's own `build.spite`
can set every option. The one exception is `target_operating_system`, which the launcher needs to know which
folder of `library/` belongs to the program, so it is read from the flag alone: a `var target_operating_system`
in a program's `build.spite` is an error naming the flag ([programs.md](programs.md#build-settings-build--implemented)).
The outputs are `Boolean` fields -- `run` (default `true`: build the executable and run it), `executable` (build it
without running), `c_source` (write the C) -- and `final_classes`, which stays the folder to write to (`""` is
off), since any folder inside the program would be read back as part of it. With every output off the compiler
still compiles the whole program and reports its errors. Tree shaking and every other whole-program step run
before any output is written.

- **Where outputs go** (D129, D283): `executable_path` and `c_path`, each `""` by default, which means the
  working folder's `.spite/`, never beside the program's source (D283, decided by Mortaro: "the compiler shouldn't
  write intermediate files beside the source"). The layout is proposed by Claude, unconfirmed: a program folder
  inside the working folder is built into `.spite/build/<its path from the working folder>/`, so `spite game` writes
  `.spite/build/game/game.exe` (`game` when the target is Linux or macOS) and `.spite/build/game/game.c`, and the
  program folder that is the working folder itself is `.spite/build/<name>/`; any other is built into
  `.spite/elsewhere/<name>_<number>/`, the number `(n * 31 + code) mod 1 000 000 007` over the codes of the program
  folder's absolute path from 7. The default paths are relative to the working folder, which is where the program
  runs. A folder a path names is created. A path option given on the command line for an output that is off is an error naming the missing
  flag: `error: '--c-path' says where the C goes, and this build writes no C: give --c-source too`, and
  `error: '--executable-path' says where the executable goes, and this build makes none: give --executable too`
  (an executable is made when `executable` or `run` is on). What describes an executable stays beside it wherever
  it goes: the `.crashes` map ([Failure: three outcomes and no others](failure.md#failure-three-outcomes-and-no-others--partial))
  and a `--hot-reload` build's `.reload_host`, `.reload_files` and `_reload_<n>` libraries. The one intermediate
  is the C the C compiler reads when `c_source` is off, written to the language repository's
  `.spite/<executable name>_<number>.c`, the number `(n * 31 + code) mod 1 000 000 007` over the codes of
  the executable's whole path from 7 (D260, decided by Claude under D244: two sessions building programs named
  alike at once used to share `<name>.c`, so one could compile the other's program), or, for a build from several
  translation units, its units, header and objects, written to `.spite/objects` under the hash of their
  content (the rules below).
- **A build that ends with no executable is an error** (D260): when the C compiler (or the link of the units)
  reports success and the executable is not at its path, the build stops with `error: the C compiler reported
  success but '<path>' is not there: nothing was built, so nothing runs (another build writing the same
  executable, or a virus scanner, may have removed it)` and exit status 1, instead of a status of 0 with nothing to
  run.
- **The compiler's own C goes to its default path**, `.spite/build/bootstrap/bootstrap.c` from the language's
  repository: every `Build` field is a constant in what is built, so a `--c-path` naming a different file each run
  would be written into the C and the fixpoint would never hold ([self_hosting.md](self_hosting.md)).
- **`.spite/` is never part of a program** (D283; proposed by Claude, unconfirmed): walking a program's folder or a
  loaded root, the compiler skips every folder named `.spite`, `.spite-cache` (the name before D283) or `.git`, so
  running `spite .` inside a program, whose outputs and checkouts land in its own `.spite/`, reads none of them
  back, and `spite format` skips `.spite/` and `.spite-cache/` the same way.
- **Final classes** are described in full [above](#inspect-merged-classes): a program, not a report.
- **The executable is built from translation units** (proposed by Claude, unconfirmed: the behaviour, the name
  `translation_units` and its rule). The number is `translation_units` when it is above `0`; at `0` it is 1 unless
  `optimized` is on, and then the largest power of two that is at most the C's size divided by 768 KiB, at most the
  processors (`NUMBER_OF_PROCESSORS` on Windows, `getconf _NPROCESSORS_ONLN` elsewhere, else 4) and at most 64. One unit is the one C file as before.
  A `--hot-reload` build and C holding `#include "` (a foreign library's header) are one file whatever the number.
  The split keeps what the C means: every preprocessor line, type and prototype goes into the header in its order;
  a function at file level loses `static` and goes to the unit `FNV-1a(name) mod units` picks, its prototype into
  the header where it stood; a `static inline` function stays in the header; a variable at file level loses `static`,
  is defined in unit 0 and declared `extern` in the header where it stood; a function or variable inside a
  preprocessor condition goes to unit 0 inside the same conditions. Each unit, the header and the list of objects
  are named by the FNV-1a hash of their text (a unit's hash covers the C compiler's command and flags too), written
  once, and compiled only when their object is missing; the C compiler runs once per missing unit, as many at once
  as there are processors, then links the objects (`@` a file listing them). Nothing removes old objects.
- **`optimized` is `-O3`, and link-time optimisation across units** (proposed by Claude, unconfirmed; the default
  build's `-O0` is Mortaro's). A build from several units also passes `-flto=thin` (with `-fuse-ld=lld` except on
  macOS) when `CC --version` names clang, `-flto=auto` when it names gcc, and nothing for another compiler.
  `tune_for_this_machine` adds `-mcpu=native` when `CC -dumpmachine` starts with `aarch64` or `arm`, else
  `-march=native`, to compiling and linking in any build.
- The C compiler is the command in `CC` (it may carry arguments), or else the first of `cc`, `clang` and `gcc`
  that runs; none found is `error: no C compiler found. Set the CC environment variable to the command that
  compiles C (for example CC=clang), or install cc, clang or gcc`. `bin/spite` keeps a `CC` already set, and
  otherwise exports it from `SPITE_CC` or the first compiler it finds; `SPITE_CC` wins over `CC`.

### Flags and settings

- **Flags are kebab-case, fields stay snake_case** (D188, decided by Mortaro): `--repl-port=4000` sets
  `Build.repl_port`, `--hot-reload` sets `hot_reload`, and a program's own field `worker_stack_size` is
  `--worker-stack-size=256` (`conformance/stage6/build_settings`). An underscore in a flag's name is an error,
  checked before anything is read, naming the hyphen form: `error: '--repl_port' is written '--repl-port': a flag
  is kebab-case, and it sets the Build field 'repl_port'` (`diagnostics/underscore_flag`); the value after `=` is
  never touched. The readings below are **(proposed by Claude, unconfirmed)**: the rule covers the compiler's flags
  before the `--`, and every message that names a flag names the kebab form; a program's run-time settings after
  the `--`, read by `Environment` when the program runs, keep their field's own spelling for now
  (`-- --player_name=x`), since translating them is a question of its own (`mortaros_missing_decisions.md`), and
  the kebab form of a declared setting stops the program naming the field's spelling rather than being ignored
  ([programs.md](programs.md)).
- Before the `--`, a `--name=value` sets the `Build` field of that name, and is written into the build as a
  constant. One that names no field is an error that shows both places a setting can belong
  (`diagnostics/unknown_compiler_flag`): `error: '--serve' is not a compiler option, and this program's Build
  declares no setting 'serve' (it declares run, executable, ...). To decide it when compiling, reopen Build in the
  program's build.spite with 'var serve = ...'; to read it when the program runs, declare it in environment.spite
  and give it after '--': spite program -- --serve=value`. One that names a field of `Environment` says to give it
  after the `--` (`diagnostics/environment_setting_to_compiler`).
- A value that does not fit the field is an error naming the flag and the forms it takes, and no file position,
  since the mistake is on the command line: `error: '--optimized=maybe' was given to the compiler, but
  'Build.optimized' is a Boolean: --optimized, --optimized=true or --optimized=false` (`diagnostics/flag_value`).
  `--repl-port` takes 1 to 65535: `error: --repl-port takes a port number from 1 to 65535: spite program
  --repl-port=4000`. `--target-operating-system` takes `windows`, `linux` or `mac`: `error:
  '--target-operating-system=beos' names no operating system the library has a folder for: windows, linux or mac`.
- `--operating-system` and `--program` cannot be given: `error: '--operating-system' cannot be given:
  Build.operating_system is the system doing the compiling (windows). To write the program for another system, give
  --target-operating-system=linux (or windows, or mac)`, and `error: '--program' cannot be given: the program is
  the folder named on the command line, 'spite folder', and Build.program is set from it`.
- **Where the compiler's flags end** (proposed by Claude, unconfirmed): at the first bare `--`. Everything after it
  reaches the program verbatim -- `arguments.get(0)` is the first, `arguments.player` reads `--player=...`, and
  `Environment` reads the settings it declares -- and none of it is read as a compiler flag or a folder to compile
  (`conformance/stage6/program_arguments`). A program's own `Arguments` stops at `--` the same way, which is the
  ordinary meaning of `--`.
- Every setting is decided while compiling and costs nothing at run time (D177); only the builds that ask for it
  carry `--debug-memory`'s table, the REPL (`--repl`, `--repl-port`) or live reload (`--hot-reload`).

### Inspectable and production builds

(D143, decided by Mortaro; the readings below proposed by Claude, unconfirmed.) A build with `--development`,
`--hot-reload`, `--repl` or `--repl-port` is *inspectable*: nothing is tree-shaken, and the singletons that hold
nothing (`Build`, `TypedMemory<T>`, [Singletons](classes_and_files.md#singletons--implemented)) are ordinary
objects -- allocated at first use, listed by `.instances`, destroyed at exit -- so the REPL and reflection see the
internals as normal classes; since nothing is shaken, every `DynamicLibrary` looks up every symbol it declares.
Every other build, the ordinary one included and not only `--optimized`, is a *production* build, where those
singletons are static objects and everything unused is tree-shaken, down to the classes nothing reachable uses.
`--optimized` with a REPL flag is inspectable. The optimisations that change only speed (chains as one loop,
placement, text appended in place) apply in both; [optimizations.md](optimizations.md) says which build each
optimisation applies in (`conformance/stage6/development_internals`).

### Where a program runs

(Proposed by Claude, unconfirmed.) In the folder `spite` was run from. The compiler finds `launcher/` and
`library/` from its own executable -- the first folder above it that holds `launcher/launcher.spite` -- and never
from the working directory, so a relative path a program opens (`File`, `Directory`, a cache folder) is the
caller's. The launcher's `load` paths are relative to that folder. The outputs with no path go into that folder's
`.spite/` (D283), so running a program adds `.spite/` to the caller's folder and nothing else, and nothing beside
the program's source (`check.sh` runs `conformance/stage6/working_directory` from another folder, and a copy of it
from inside that folder). A program built with `--repl`, `--repl-port` or
`--hot-reload` runs attached to the terminal; any other run's output is printed when it ends, and the compiler
exits with the program's exit code.

### The launcher passes the program's arguments untouched

(Fixes a bug found converting Theseus's game data; proposed by Claude, unconfirmed.) `bin/spite` is a bash
script, and `bin/spite.cmd` runs it with the bash on the `PATH`, which on Windows is Git for Windows' bash. That
bash rewrites every argument that looks like a POSIX path when it starts a Windows program, so
`spite tool -- --prefixes=/Game/Legacy/` reached the program as `--prefixes=C:/Program Files/Git/Game/Legacy/`,
from PowerShell as well, and `MSYS_NO_PATHCONV=1` broke the launcher's own paths instead. Now the launcher writes
its own paths as Windows paths itself (`cygpath -m`: the folder, `--executable-path`, `--c-path`,
`--final-classes`, and any other `--name=/...` before `--`, which the rewriting used to convert) and turns the
rewriting off for the compiler it starts (`MSYS2_ARG_CONV_EXCL="*"`), so every argument after `--` -- and every
argument of `spite connect` -- arrives exactly as typed. On Linux and macOS nothing is rewritten and nothing
changes. `check.sh` runs `conformance/stage6/launcher_arguments` through `bin/spite` with `--prefixes=/Game/Legacy/`,
`/usr/share` and `a b` after `--`.

### How many errors are listed

(A94, a request from the Theseus port; proposed by Claude, unconfirmed.) Every error is `<path>:<line>: error:
<message> (in <Class>.<function>)`, each listed once. The errors in files under the program's own folder come first,
at most 25, followed by `and N more in the program` when there were more. Then come the errors in the packages it
loads -- the standard library, the launcher and every loaded folder -- at most 25 of them together, in the order they
were found, followed by one `and N more in <package>` per package that had more, the package named by its folder, relative to the folder `spite` was run from when it is
inside it (`library` and `launcher` for the language's own). A package whose errors fill the list therefore never
hides one in the program.

### Formatting before compiling, and `spite format`

- **Every compile formats first, and nothing turns it off** (D190, decided by Mortaro; the readings proposed by
  Claude, unconfirmed). After the whole program is read, every `.spite` file of the program's own -- its folder
  with every folder below it, and every `load`-ed root, never `library/` -- whose formatted text differs from what
  is on disk is rewritten, printing `formatted <path>` to the error output, and the program is read again from
  disk before anything else is produced, so what is compiled, and every line an error names, is the formatted
  file. A file that does not lex or parse is reported by reading the program, before the formatter sees it. A file
  the formatter's own safety check refuses ([Style](style.md#style--implemented)) stops the compile with exit
  status 1: `<path>: error: the formatter refuses it, and a program compiles only once formatted: <reason>`.
- There is no switch: `--format` (with any value) is `error: '--format' is not a compiler option: every compile
  formats the program's own files first, and nothing turns that off. To format files without compiling them:
  spite format file_or_folder` (`diagnostics/format_flag`), and a `var format` in a program's `build.spite` is
  `<path>:<line>: error: 'Build.format' cannot be declared: ...` with the same advice (`diagnostics/format_setting`).
  A test input kept unformatted on purpose, like `diagnostics/`, is compiled from a copy by `check.sh`, so the
  formatting lands on the copy.
- **`spite format <file-or-folder> ...`** runs the same formatter without compiling: a file formats just itself, a
  folder every `.spite` file under it except in `.spite/` and `.spite-cache/` folders, with no regard for what a
  program loads.
  Each file rewritten prints `formatted <path>` to the error output; a file the formatter refuses is
  `<path>: not formatted: <reason>`, and the command exits 1 after trying the rest. `--check` rewrites nothing,
  prints each file that would change on standard output, and exits 1 if there is one (0 when everything is
  already formatted). With no file or folder the compiler prints its usage and exits 1; `bin/spite format` with
  none formats the current folder.
