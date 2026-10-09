# Compiler command line

`spite` compiles one program. A program is a folder, and its entry file is the one named after the folder:
`spite game` compiles `game/game.spite`, whose class `Game` is constructed to start the program. The compiler
first reads the **whole** program (the launcher, the standard library, the program's folder, its `build.spite`
and every folder it `load`s) and only then decides what to produce, from the program's
[`Build`](programs.md#compile-time-settings-build). Every option is a field of `Build`, given as `--name=value`
(a `Boolean` option may be given bare, `--optimized`). A flag is written in kebab-case and sets the `snake_case` field
of the same name: `--repl-port=4000` sets `Build.repl_port`, and a field a program declares in its own
`build.spite`, `worker_stack_size`, is given as `--worker-stack-size=256`. By default
the compiler builds the executable into `.spite/` in the folder you run it from, and runs it: nothing is written
beside the program's source unless a path says so ([where the outputs go](#where-the-outputs-go)).

```
spite game                          build .spite/build/game/game.exe, and run it
spite game --debug-memory           the same, counting allocations and frees
spite game --check                  format it and check that it compiles, building nothing
spite game --build                  build it without running it
spite game --repl-port=4000         serve a REPL on 127.0.0.1:4000 while it runs
spite game --name=production        run it with a setting its Environment declares
spite connect 4000                  talk to a program running with --repl-port=4000
```

Every command and flag, with exactly what it does, is listed under [Command line](../specs/compiler.md#command-line). Flags mix
freely (a production build may keep the REPL), and the outputs combine: one compile can build the executable and
write its final classes.

`bin/spite` (and `bin/spite.cmd`, which runs it from a Windows prompt) is the command itself: it builds the
compiler from the seed for the system it runs on (`bootstrap/seed/linux/spite_compiler.c`, or `windows/`) into
`.spite/spite.exe` the first time, and again whenever the seed is newer, finds a C compiler, makes the folder and path arguments absolute, and passes everything else
through. A program's own settings and arguments reach it exactly as they were typed, from bash or from PowerShell
or `cmd` through `spite.cmd`: `spite tool --prefixes=/Game/Legacy/` gives the program `--prefixes=/Game/Legacy/`,
not the path Git for Windows' bash would make of it ([the rule](../specs/compiler.md#the-launcher-passes-the-programs-arguments-untouched)).

## Name the program

```bash
spite game
spite game/
```

The folder supplies the program's other classes, and the entry class's constructor takes no arguments: the
program reads its command line through `Environment()`, what its build decided through `Build()`, and the raw
arguments through `Arguments()`. A program is named only by its folder: `spite game/game.spite` is an
error that answers with the folder form, `spite game`, and a folder with no entry file named after it is an error
naming the file it looked for ([the texts](../specs/compiler.md#naming-a-program)). The compiler compiles itself the same way,
`spite bootstrap`, whose entry is `bootstrap/bootstrap.spite`.

## How a program is loaded

The compiler reads `launcher/launcher.spite` first. It is a Spite class whose constructor loads `library/`, then the
target operating system's folder of it, then the program's folder, and the compiler follows its `load` lines in order.
[programs.md](programs.md#how-a-program-is-loaded) walks through it.

## Where it runs

The compiler finds `launcher/` and `library/` from its own executable, looking in each folder above it, and
never from the working directory. So `spite` runs, and the program it builds runs, in the folder you ran it from:
a relative path the program opens (`File("save.txt")`, `Directory("levels")`, a cache folder) is relative to
that folder, not to the language's repository. A foreign library's header named by a relative path
(`DynamicLibrary`'s third argument) is looked for in the working directory as well.

## Choose the outputs

The compiler reads the program first, and decides what to produce only after, from `Build`, so a program's own
`build.spite` can choose its outputs like any other option ([programs.md](programs.md#compile-time-settings-build)).
`spite game` builds the executable and runs it, with its settings and arguments. Two flags say to stop sooner:
`--check` (`Build.check`) only checks that the program compiles, and `--build` (`Build.build`) builds the
executable without running it. `final_classes` writes the program back out as Spite into a folder, beside whatever
else the build does ([below](#inspect-merged-classes)), and `optimization_report` writes what the compiler could not
optimise into a file ([below](#read-what-was-not-optimised)); the defaults are in [Build options](#build-options).

`--check` still reads and compiles the whole program and reports every error, and writes nothing but the
formatting, which is not an output: every compile does it first ([below](#formatting)). It writes no executable
and never touches one an earlier build left, so checking while the program runs is safe:

```bash
spite game            # build .spite/build/game/game.exe and run it
spite game --build    # build it, run nothing
spite game --check    # only check that it compiles
```

A build never leaves an old executable behind. `spite game` and `spite game --build` first remove the executable
an earlier build left at their path, so when the build fails, nothing is left there that still runs the old code:
the executable at that path is always the one the last build made, or none.

Whole-program steps (tree shaking, the constants `Build` folds, which templates are made) run on the complete
program before any output is written, so the C written with an executable is the C that executable was built
from.

In that C, every local, parameter and attribute has a `_` after its Spite name (`var near = 3` is
`int32_t near_ = 3;`, an attribute `pascal` is `self->pascal_`), and a function is its class's name joined to its
own (`Map_far`). No snake_case name ends in `_`, so no Spite name can meet a C keyword or a macro from a system
header (Windows defines `near`, `far` and `pascal`): C reserves nothing in Spite. Reflection,
`--final-classes`, crash reports and error messages keep the Spite names.

## Where the outputs go

**Nothing is written beside the program's source unless a path says so**. Each output has its own path
option, and without one it goes into `.spite/`, a folder inside the folder you ran `spite` from. That is the folder a
relative path the program opens is read from ([above](#where-it-runs)). `.spite/` holds only what the compiler
writes, and can be deleted whenever you like:

| Output | Path option | Default |
|---|---|---|
| the executable | `executable_path` | `.spite/build/game/game.exe` (no `.exe` on Linux and macOS) |

The folder under `.spite/build/` is the program folder's own path from the folder you ran `spite` in, so
`spite examples/hello` builds `.spite/build/examples/hello/hello.exe`, and two programs named alike never share an
executable. A program outside that folder (`spite ../other/game`, or an absolute path somewhere else) is built into
`.spite/elsewhere/game_<number>/game.exe`, the number worked out from the program folder's whole path the way the
C's is below. `spite game` builds and runs it from there, so where it lands matters only when you want to keep it:

```bash
spite game --build --executable-path=build/game.exe
```

`.spite/git/` holds the checkouts of the repositories a program's `load`s pin to a commit
([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)). In the language's own repository, `.spite/`
also holds the compiler `bin/spite` builds and the intermediate C and objects below. No
`.spite/` folder is ever read as part of a program, and neither is an old `.spite-cache/` folder, which can
be deleted.

A folder the path names is created. `--executable-path` given with `--check`, which builds no executable, is an
error ([the texts](../specs/compiler.md#outputs)), so a path never passes silently. Every `Build`
field is a constant in the built program, the paths included.

What a build writes **beside its executable**, wherever that is (`.spite/build/game/` unless `--executable-path`
moves it): `game.crashes`, the map from a crash report's
site ids to their lines ([failure.md](failure.md)), and, for a `--hot-reload` build, `game.reload_host`,
`game.reload_files` and each reload's `game_reload_1.dll` with its C ([repl.md](repl.md#live-reload---hot-reload)).
The one intermediate is the C the C compiler reads: it goes to the language repository's `.spite/game_<number>.c`, the number worked out from the executable's whole path, so
two builds of programs named alike in different folders at once never compile each other's C; it is overwritten
by the next build of the same executable. A C compiler that reports success without writing the
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
| `check` | `false` | only checks that the program compiles: no executable, nothing run ([above](#choose-the-outputs)) |
| `build` | `false` | builds the executable without running it |
| `executable_path` | `""` | where the executable goes; `""` is `.spite/build/<program>/` in the working folder ([above](#where-the-outputs-go)) |
| `final_classes` | `""` | writes the merged classes to this folder |
| `optimization_report` | `""` | writes what could not be optimised, and why, to this file ([below](#read-what-was-not-optimised)) |
| `optimized` | `false` | a release build: `-O3` instead of `-O0`, and link-time optimisation across the [translation units](#translation-units-the-c-compiled-in-parallel-and-cached) ([below](#release-builds)) |
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
spite game --build --executable-path=build/game.exe --debug-memory
```

A program may give an option a different default by reopening `Build` in its own `build.spite`
(`var optimized = true`, or `var debug_memory = true` to always count its memory), and add options of its own the same
way. No option is read before the program is, so every one of them, the outputs included, can be given a default
there, except `target_operating_system`, which comes from the flag alone ([why](../specs/compiler.md#outputs)). Every `Build` field is
a constant in the built program, so with `var build = Build()` beside the attributes, `if build.debug_memory { }`
keeps one branch.

None of this is in the program that is built: choosing, building and writing outputs is the compiler's own
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

What the C compiler says is shown only when it fails ([the rule](../specs/compiler.md#what-the-c-compiler-says)): the C is the
compiler's, so a warning about it is nothing you could act on, and it never reaches the output of a program that
built.

## Release builds

`--optimized` is the release build: the C compiler is asked for `-O3` instead of the default build's `-O0`, and
when the executable is built from several [translation units](#translation-units-the-c-compiled-in-parallel-and-cached),
for link-time optimisation too (`-flto=thin` with clang, linked by `lld` except on macOS; `-flto=auto` with gcc), so
a function in one unit is still inlined into another.

The compiler is fastest by default: the default build is the one that compiles fastest, since it is the one you
rebuild all day, and each flag trades something on purpose. `--optimized` trades compile time for run speed;
`--hot-reload` trades run speed for live information about the running program, and compiles like the default
build. So the default build and `--hot-reload` compile the C at `-O0`, and only `--optimized` asks for more.

A default or `--hot-reload` build is never shipped, so the C compiler may use every instruction the machine
building it has (`-march=native`, or `-mcpu=native` on ARM): it runs where it was built. An `--optimized` build is
the one shipped, so it stays portable and runs on any processor of its system. Neither is a setting:

```bash
spite game              # -O0, for this machine's processor
spite game --optimized  # release: -O3 and link-time optimisation, for any processor
```

How fast a release build is, against the same program written in C plainly and tuned by hand, is
`bash benchmarks/run.sh` ([benchmarks/README.md](../benchmarks/README.md)); what each optimisation level costs is in
[its case](../benchmarks/a_release_build_is_o3_with_link_time_optimisation/#each-optimisation-level).

### Measure only a production build

REPL and live-reload builds are slower on purpose. They exist to tell you more while the program runs: every
function sits in a slot so it can be swapped, the program can stop at a breakpoint, and the REPL can inspect any
object. None of that is free, and none of it is meant to be fast. A default build is compiled at `-O0` for its own
reason: it is the build you rebuild all day, so it is the one that compiles fastest, not the one that runs fastest.

So any benchmark or performance comparison of Spite uses a production build: `--optimized`, with no `--repl`, no
`--repl-port`, no `--hot-reload` and no `--debug-memory` (which counts every allocation). A number measured on any other build describes the tooling, not the program.

```bash
spite game --optimized    # the only build to measure
```

## Translation units: the C compiled in parallel and cached

A big program is built from several C files, in a default build and in an `--optimized` one, compiled at the same time and
remembered. The generated C is split into one header (every type, macro and prototype, and an `extern` line for
each variable at file level) and a number of units that include it. Each function goes into the unit a hash of
its name picks, so editing a function leaves every other function where it was; the variables are defined in the
first unit; the small `static inline` helpers stay in the header, a copy for each unit. The C compiler compiles as
many units at once as the machine has processors, and the objects are linked.

Each unit's object is kept in the language repository's `.spite/objects`, named by a hash of everything it
was compiled from: the C compiler's command and flags, the header and the unit. A build finds the objects it
already has and compiles only the rest, so building a program again after changing nothing only links it, and
after changing one function's body compiles one unit. What the cache cannot save: an edit that adds or removes a
function, changes a type, or adds or removes a piece of constant text changes the header (the texts are
file-level variables numbered in order), and every unit is compiled again.

The cache cleans itself: once `.spite/objects` passes 1 GiB, a build removes what was used least recently until
it is back under, so it never needs deleting. The cap is the compiler's choice, not a setting.

How many units is the compiler's choice, not a setting: the largest power of two that is no more than one unit per
768 KiB of C, no more than the number of processors, and at most 64, which the measurements below settled on:
eight units for the compiler's 7 MB of C on a machine with eight processors or more. A program under 1.5 MB of C
stays one file, compiled as before and not cached. A `--hot-reload` build, and a program whose C includes a
foreign library's header (a `DynamicLibrary` given one), are always one file.

An `--optimized` build, in seconds of the whole `spite` command on a machine with 32 logical processors
and clang 19.1.5: from one file as before, and from units with the cache empty (cold), full (warm) and after one
function's body changed:

| program | C | one file | cold | warm | one edit |
|---|---|---|---|---|---|
| the compiler | 7.2 MB | 37.7 | 14.5 | 11.4 | 13.3 |
| a large game's `kal_character` | 14.1 MB | 80.1 | 45.7 | 30.7 | 67.2 |
| a generated 209 206-line program | 20.5 MB | 181.2 | 50.0 | 34.3 | 37.3 |

A warm build is the Spite compile plus a ThinLTO link, which optimises the whole program again each time; the
game's edit was in a generic class, which changes every instantiation of it. A default build is split by the same
rule and compiles each unit at `-O0`, linked without link-time optimisation: on a busier day than the table's, the
compiler took 19.0 s from one file and 22.5 cold, 12.8 warm and 13.3 after one edit from units, so the build you
repeat all day, after an edit, is the faster one. How they were measured, and the unit counts tried, are in
[the case of translation units](../benchmarks/the_c_is_compiled_in_parallel_units_and_cached/#compile-time-at-scale);
`bash scripts/build_times.sh` measures them again.

## Compile for another system

`--target-operating-system=linux` (or `windows`, or `mac`) compiles for that system from any machine: it loads
`library/linux/` instead of this machine's folder, and `build.target_operating_system` is `"linux"` in the
program. Use it to check that the folders of another system still compile:

```bash
spite game --check --target-operating-system=linux
```

## Pass settings to the program

A program's settings are written exactly like the compiler's flags, beside them: kebab-case on the command line,
snake_case in code. A program reads its run-time settings from the `Environment` singleton, whose fields it
declares by reopening `Environment` in its own `environment.spite` (see
[programs.md](programs.md#run-time-settings-environment)):

```bash
spite game --optimized --player-name=Bob --volume=7
```

`--optimized` sets a `Build` field (the compiler's own options, or one the program declares in its
`build.spite`) and is written into the build as a constant; `--player-name=Bob` and `--volume=7` set
`Environment.player_name` and `Environment.volume`, so the compiler passes them to the program when it runs it.
Every other argument after the folder that is not a flag (`spite game ada`) is passed to the program too, for
`Arguments` to read. There is no `--` separator: a bare `--` is an error showing the form above.

Compiler flags and program settings share one set of names, so an `Environment` field named like a `Build` field
is a compile error naming both. A flag naming no field of either is an error listing the fields `Build` has, a
setting given the snake_case spelling is an error naming the kebab one, and a value that is not of the setting's
type is an error naming the forms it takes, so typos never pass silently.

## Formatting

**The compiler is the formatter, and every compile formats first**. Once the whole program has been read,
each of the program's own files whose formatted text differs from what is on disk is rewritten, printing
`formatted <path>`, and the program is read again, so what is compiled, and every line an error names, is the
formatted file. Nothing turns this off, and a file the formatter refuses stops the compile: a program is never
compiled from text that is not in the one style ([the rules](../specs/compiler.md#formatting-before-compiling)).

**A file edited during the compile is never overwritten.** Just before writing a formatted file, the compiler reads it
again; if it no longer holds the text the compile read (another editor or agent changed it meanwhile), it is left
as it is now, `<path> changed while it was being compiled` is printed, and the program is read again from disk. A
file whose formatting does not change is never written, so its modification time never moves.

There is no command that only formats. A linter or a language server runs `spite game --check`, which formats the
program and checks it without building, and there is no mode that checks the formatting without writing it: the
fix is always the compiler's own. What the formatter rewrites, and what it refuses to, is [style.md](style.md).

## Development builds and tree shaking

A normal build keeps only what the program uses: a condition on a codegen value or a `Build` field keeps one
branch, a template exists only for the names called, and reflection only where it is read
([metaprogramming.md](metaprogramming.md#tree-shaking)).

An **inspectable build** (`--development`, `--hot-reload`, `--repl` or `--repl-port`) keeps every generated
function instead, so live reload has all of them to swap and the REPL can reach every internal, and the
singletons that hold nothing, such as `Build`, are ordinary objects that `.instances` lists. Every other build,
ordinary or `--optimized`, is a production build ([the rule](../specs/compiler.md#inspectable-and-production-builds)). Conditions on
codegen values and `Build` fields fold in both: they are facts of the build, not values the running program could
change. Which optimisation applies in which build is in [optimizations.md](optimizations.md).

## Counting memory: `--debug-memory`

`--debug-memory` builds the program with an allocation table and prints `allocations: N frees: N` when it ends.
The two numbers differ only when something leaked, and then the report names the classes whose instances are
still alive, which is how a leaked cycle shows up ([memory.md](memory.md#cycles-leak)). The table costs a
lookup under a lock on every allocation and free, and exists only in a `--debug-memory` build: any other build
calls the allocator directly.

## Compile time

Compiling grows linearly with the program: every whole-program step (reading, formatting, analysis, template
instances, call effects, tree shaking) works on each class a fixed number of times, and anything looked up by
name is found through a table, never by walking every class again. The measure is a data-heavy program: a
walk over a folder of small record classes, each one `fill(item)` of 10 to 30 assignments, with
`Filler<record.class>` made for each, and the items kept in a `Dictionary` keyed by number. CPU seconds of the
compiler alone (`--check`, so no C compiler), on Windows with clang:

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
is generated twice; the first pass stops once it has learned them, before the C is assembled and tree
shaken, since the second pass writes it again. Every file is read once: the formatter formats the text the
compile read. The compiler compiling itself, which has neither, takes 1.7 s of CPU, as before.

Two more steps that were quadratic, found building a large game with `--hot-reload` (about 460 files; 880,000 lines
of C for its server, 1.3 million for its client): which functions can reach a wait was worked out by
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

`--final-classes=folder` writes the discovered, merged classes as readable `.spite` files, one per class, under
the namespace folders it belongs to. It is useful after a `load` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program. `Build` is printed with the defaults it
was declared with (the program's own `build.spite` included), not the values this build folded, so running the
printed program takes its flags again. A repository loaded at two commits is printed once per version, each in
a folder named by the repository and the commit (`engine_6c7dca9/`), since the versions are two libraries
([packages.md](packages.md#two-versions-of-one-repository)).

Each attribute and function is marked with one comment line linking the `.spite` file that supplied it, the
program's own file, a file it loads, the reopening that replaced it, or the compiler's own reopening
(`bootstrap/source/generation/prelude.spite`) for a body the compiler supplies, as a path relative to the printed
file. The folder holds a `.final-classes` file, which is what lets a comment there link a `.spite` file
([style.md](style.md#comments-are-links)), so the printed program compiles with its links checked like any other.

What is written is what the program **ends up with**, not what was written down: the classes come from the
generator after it has run, so a class the generator never made is not there, a generic template is not there,
and each of its instantiations is. The tree shaking of a production build's C runs after that, so a library class
the program never uses (`FileSystemWatcher`, `Socket`) may still be printed although its C was dropped.

Every class the program names comes from a file in `library/`, including `Memory.Heap`, `DynamicLibrary`,
`String` and the numbers (`Integer`, `Long`, `Double`, `Memory.Address`, ...), so every one of them is printed like a
class of your own. A function whose body the compiler supplies (the floor that stays C, such as
`Memory.Heap.allocate`) is added to its class as the compiler's own reopening, and is printed as a declaration
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

The folder has no default: anything inside the program's own folder would be read back as part of the program,
so the folder is always named. It combines with every build; with `--check` it is the only thing written:

```bash
spite game --final-classes=.spite/final --check
```

## Read what was not optimised

The compiler optimises on its own and says nothing about what worked ([optimizations.md](optimizations.md)). What it
could **not** do is what costs you, so `--optimization-report=file` writes exactly that: one Markdown file listing
every place an optimisation fell back, each line a link to the source line and the reason, so every line is
something you can act on. Nothing is written without the flag, and the program built is the same either way.

```bash
spite game --optimization-report=.spite/optimization.md --check
```

The report has one section per optimisation, with how many places it lists:

- **Lists that hold references**: each `List<T>` whose items are kept as references, at the line that first made
  it, and why `T` cannot be laid inline (an attribute that is a list or another object, a `drop()`, a function
  using `this` as a value), or that it would fit and a `Vector<T>` holds it inline.
- **Lists not in the frame**: a local list that could have lived in the function's frame
  ([optimizations.md](optimizations.md#a-local-list-of-known-size-lives-in-the-frame)) and did not: appended to in
  a loop, more items than the frame holds, or the line that does more than read it.
- **Objects not in the frame**: a local made by a constructor that stays on the heap
  ([optimizations.md](optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)), with the line
  that lets it go (`kept.append(badge)`), or what about its class keeps every object on the heap.
- **Copies not elided**: a `var twin = original.copy()` that allocates, with the line that lets the copy go, or
  why its class cannot be copied into the frame.
- **Overflow checks kept**: each `+`, `-`, `*` or `-` in front of a whole number that keeps its check
  ([optimizations.md](optimizations.md#arithmetic-a-range-proves-is-not-checked)), with the operand whose range
  nothing proves or the ranges that let the answer pass its type
  (`` `product * 3` keeps its overflow check: 'product' runs from -2147483648 to 2147483647, so the answer may not
  fit in an Integer ``). Hold a total in a wider type, or bound a value where it is made, and the line goes.
- **Waits that hold the frame**: each call a frame loop would have started
  ([concurrency.md](concurrency.md#a-wait-in-a-frame-does-not-hold-the-frame)) that waits in place instead,
  because the call or the frame writes something back from a value it read before a wait
  (`` `tally.add_late()` waits in place: Tally.add_late writes `count` from a value it read before a wait, and the
  frame could write it while the call waits ``). Read the value again after the wait, and the line goes.

A line looks like this, its link relative to the report's folder so it opens from wherever the file is read:

```markdown
- [game/game.spite:23](../game/game.spite#L23): `badge` (`Badge`) is made on the heap: line 24 lets it go: `badges.append(badge)`
```

Like `--final-classes`, the report describes what the program ends up with: a function that tree shaking removes
reports nothing, and the library's lines are listed with the program's, sorted by file and line, since they cost
the program the same. An inspectable build (`--development`, `--repl`, `--repl-port`, `--hot-reload`) places
nothing in the frame by design; its report says so and lists only the lists that hold references and the overflow
checks kept.

## Errors and usage

With no program, the compiler prints its usage text and exits unsuccessfully. A `.spite` file named instead of
its folder, a folder with no entry file, an entry constructor that takes arguments, `--check` given with `--build` or
`--executable-path`, a flag the compiler no longer has (`--run`, `--executable`), a flag with an underscore, a `--name=value` that names no field of `Build` or `Environment`, and a bare `--` are errors
too, each naming the fix; their texts are [in the specification](../specs/compiler.md#naming-a-program).

A program's own errors come first. When a package it loads is broken too (a game engine package halfway through a
migration, say), its errors are listed after the program's, so they never hide the program's own; how many of each
are listed is [in the specification](../specs/compiler.md#how-many-errors-are-listed).

---

Next: [REPL and live reload](repl.md), running a program with a REPL and swapping changed classes into it.
