# Compiler command line

`spite` compiles one program. A program is a folder, and its entry file is the one named after the folder:
`spite game` compiles `game/game.spite`, whose class `Game` is constructed to start the program. The compiler
first reads the **whole** program -- the launcher, the standard library, the program's folder, its `build.spite`
and every folder it `load`s -- and only then decides what to produce, from the program's
[`Build`](programs.md#compile-time-settings-build). Every option is a field of `Build`, given as `--name=value`
(a `Boolean` option may be given bare, `--optimized`). A flag is written in kebab-case and sets the `snake_case` field
of the same name: `--repl-port=4000` sets `Build.repl_port`, and a field a program declares in its own
`build.spite`, `worker_stack_size`, is given as `--worker-stack-size=256` (D188). A flag written with an underscore
is an error naming the hyphen form: `error: '--repl_port' is written '--repl-port': a flag is kebab-case, and it
sets the Build field 'repl_port'`. By default the compiler builds the executable beside the program and runs it.

```
spite program                         build program/program.exe beside the program, and run it
spite program --optimized             an optimized build
spite program --development           an inspectable build: nothing tree-shaken, internals as ordinary objects
spite program --debug-memory          count allocations and frees, and print the balance at the end
spite program --repl                  run it, then open a REPL on the running program
spite program --repl-port=4000        serve a REPL on 127.0.0.1:4000 while it runs
spite program --hot-reload            swap edited classes into the running program, keeping its state
spite program --serve=true            decide a Build field the program declares, while compiling
spite program -- --serve=true         run it with a setting its Environment declares
spite program --c-source              write program/program.c too, then build and run
spite program --c-source --run=false  write the C and nothing else
spite program --executable --run=false            build the executable without running it
spite program --executable --c-source --run=false build it and write its C, in one compile
spite program --executable-path=build/game.exe    put the executable somewhere else (--c-path= for the C)
spite program --run=false             compile the program and write nothing: its errors, if any
spite program --final-classes=folder  also write the program back out as Spite, every class as it ended up
spite program --target-operating-system=linux --c-source --run=false   write the C for another system
spite format <file-or-folder> ...     format files without compiling them
spite format --check <path> ...       rewrite nothing; fail listing every file that would change
spite connect 4000                    talk to a running --repl-port program
spite connect 4000 --command="..."    send one REPL command and print its JSON answer
```

Flags mix freely -- a production build may keep the REPL -- and the outputs combine: one compile can build the
executable, write its C and write its final classes. `bin/spite` is the command itself: it builds the compiler from
`bootstrap/seed/spite_compiler.c` the first time (and again whenever the seed changes), finds a C compiler, and
passes everything else through.

## Name the program

```bash
spite game
spite game/
```

The folder supplies the program's other classes, and the entry class's constructor takes no arguments: the
program reads its command line through `Environment()`, what its build decided through `Build()`, and the raw
arguments through `Arguments()`. A program is named only by its folder (D130): a path to a `.spite` file is an
error that names the folder form, `error: 'game/game.spite' is a file, and a program is named by its folder:
spite game`, and a folder with no entry file named after it is an error naming the file it looked for. The
compiler compiles itself the same way, `spite bootstrap`, whose entry is `bootstrap/bootstrap.spite`.

## How a program is loaded

The compiler reads `launcher/launcher.spite` first -- a Spite class whose constructor loads `library/`, then the
target operating system's folder of it, then the program's folder -- and follows its `load` calls in order.
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
Each output is a `Boolean` field, and every one that is on is produced by the same compile:

| Output | Default | What it produces |
|---|---|---|
| `run` | `true` | builds the executable, then runs it with everything after `--` |
| `executable` | `false` | builds the executable without needing to run it |
| `c_source` | `false` | writes the generated C |
| `final_classes` | `""` | writes the program back out as Spite into this folder ([below](#inspect-merged-classes)) |

Running a program runs its executable, so `run` builds it too, in the same place `executable` would. With every
output off (`--run=false`), the compiler still reads and compiles the whole program and reports its errors, and
writes nothing but the formatting, which is not an output: every compile does it first ([below](#formatting)):

```bash
spite game                                      # build game/game.exe and run it
spite game --c-source                           # the same, and write game/game.c to read
spite game --executable --c-source --run=false  # build game/game.exe and write game/game.c, run nothing
spite game --run=false                          # only check that it compiles
```

Whole-program steps -- tree shaking, the constants `Build` folds, which templates are made -- run on the complete
program before any output is written, so the C written beside an executable is the C that executable was built
from.

In that C, every local, parameter and attribute has a `_` after its Spite name (`var near = 3` is
`int32_t near_ = 3;`, an attribute `pascal` is `self->pascal_`), and a function is its class's name joined to its
own (`Map_far`). No snake_case name ends in `_`, so no Spite name can meet a C keyword or a macro from a system
header (Windows defines `near`, `far` and `pascal`): C reserves nothing in Spite (D168).

## Where the outputs go

Each output has its own path option, and without one it is written **beside the program**, in its own folder:

| Output | Path option | Default |
|---|---|---|
| the executable | `executable_path` | `game/game.exe` (`game/game` on Linux and macOS) |
| the C | `c_path` | `game/game.c` |

```bash
spite game --executable --run=false --executable-path=build/game.exe
spite game --c-source --run=false --c-path=build/game.c
```

A folder the path names is created. A path option given for an output that is off is an error saying which flag
is missing (`--c-path` without `--c-source`), so a path never passes silently. Every `Build` field is a constant
in the built program, the paths included.

What a build writes **beside its executable**, wherever that is: `game.crashes`, the map from a crash report's
site ids to their lines ([failure.md](failure.md)), and, for a `--hot-reload` build, `game.reload_host`,
`game.reload_files` and each reload's `game_reload_1.dll` with its C ([repl.md](repl.md#live-reload---hot-reload)).
The one intermediate that is not an output is the C the C compiler reads when `--c-source` is off: it goes to the
language repository's `.spite-cache/game.c`, and is overwritten by the next build of a program of that name.

`spite reload <folder> ... --executable-path=<running executable>` is what a `--hot-reload` program runs to
rebuild itself: given the options it was built with, it compiles only the classes whose files changed into a
library beside the executable and prints what it rebuilt ([repl.md](repl.md#live-reload---hot-reload)).

## Build options

Every option is a `Build` field with a literal default, declared in `library/build.spite`:

| Option | Default | What it does |
|---|---|---|
| `run` | `true` | builds the executable and runs it |
| `executable` | `false` | builds the executable |
| `executable_path` | `""` | where the executable goes; `""` is beside the program |
| `c_source` | `false` | writes the generated C |
| `c_path` | `""` | where the C goes; `""` is beside the program |
| `final_classes` | `""` | writes the merged classes to this folder |
| `optimized` | `false` | asks the C compiler for `-O2` instead of `-O0` |
| `development` | `false` | an inspectable build: keeps everything, no tree shaking, internals as ordinary objects |
| `repl` | `false` | runs the program with an in-place REPL |
| `repl_port` | `0` | serves the remote REPL on this port (see [repl.md](repl.md)) |
| `hot_reload` | `false` | swaps changed classes into the running program and implies `development` (see [repl.md](repl.md#live-reload---hot-reload)) |
| `debug_memory` | `false` | counts allocations and frees and prints them when the program ends |
| `operating_system` | the compiling machine | cannot be given: it is the system doing the compiling |
| `target_operating_system` | `operating_system` | the system the program is compiled for |
| `program` | the folder named | cannot be given: it is the folder on the command line, which the launcher loads |

```bash
spite game --optimized
spite game --debug-memory
spite game --executable --run=false --executable-path=build/game.exe --debug-memory
```

A program may give an option a different default by reopening `Build` in its own `build.spite`
(`var optimized = true`, or `var c_source = true` to always write its C), and add options of its own the same
way. No option is read before the program is, so every one of them, the outputs included, can be
given a default there -- except `target_operating_system`, which the launcher needs to know which folder of
`library/` is part of the program before it can read the rest, so it comes from the flag alone. Every `Build` field is a constant in the built program, so with `var build = Build()`
beside the attributes, `if build.debug_memory { }` keeps one branch.

The compiler uses the `CC` environment variable when set; otherwise it tries `cc`, `clang`, then `gcc`:

```bash
CC=clang spite game --optimized
```

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
each of the program's own files -- the entry folder and every `load`-ed root, not `library/` -- whose formatted
text differs from what is on disk is rewritten, printing `formatted <path>`. If any file was rewritten, the program
is read again from disk before anything else is produced, so what is compiled, and every line an error names, is
the formatted file. Nothing turns this off: `--format` is an error, and so is a `format` field in the program's
`build.spite`. A file the formatter refuses is an error of the compile, which stops there, naming the file and the
reason; a program is never compiled from text that is not in the one style.

`spite format` runs the same formatter on files rather than on a program: each file named, and every `.spite`
file under each folder named (the whole tree, `library/` included, except `.spite-cache/` folders), without
compiling anything. `spite format --check` rewrites nothing and exits 1 listing every file that would change.
What it rewrites, and what it refuses to (naming), is [style.md](style.md).

## Development builds and tree shaking

A normal build keeps only what the program uses: a condition on a codegen value or a `Build` field keeps one
branch, a template exists only for the names called, and reflection only where it is read
([metaprogramming.md](metaprogramming.md#tree-shaking)).

An **inspectable build** is one built with `--development`, `--hot-reload`, `--repl` or `--repl-port`
([D143](decisions.md)). It keeps every generated function instead, so live reload has all of them to
swap and the REPL can reach every internal, and the singletons that hold nothing -- `Memory`, `Build`,
`TypedMemory<T>` -- are ordinary objects that `.instances` lists, instead of the static objects a production build
makes of them. Every other build, ordinary or `--optimized`, is a production build. Conditions on codegen values
and `Build` fields fold in both: they are facts of the build, not values the running program could change. Which
optimisation applies in which build is in [optimizations.md](optimizations.md).

## Counting memory: `--debug-memory`

`--debug-memory` builds the program with an allocation table and prints `allocations: N frees: N` when it ends.
The two numbers differ only when something leaked, and then the report names the classes whose instances are
still alive, which is how a leaked cycle shows up ([memory.md](memory.md#cycles-leak)). Every program in
`conformance/`, `examples/` and these pages is run this way by `check.sh`, and must balance.

## Inspect merged classes

`--final-classes=folder` writes the discovered, merged classes as readable `.spite` files -- one per class, under
the namespace folders it belongs to. It is useful after a `load` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program, which `check.sh` proves on every run. `Build` is printed with the defaults it
was declared with, not the values this build folded, so running the printed program takes its flags again.

What is written is what the program **ends up with**, not what was written down: the classes come from the
generator after it has run, so a class tree shaking removed is not there, a generic template is not there, and
each of its instantiations is.

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
`Build` is printed with its declared defaults, and `Launcher` with the rest.

Which root supplied each declaration is **not** shown yet. It cannot be a comment, since a comment is only ever
a link to a markdown heading ([style.md](style.md#comments-are-links)), so it needs a form of its own
([open question 10](open_questions.md#open-questions)).

The folder has no default: anything inside the program's own folder would be read back as part of the program,
so the folder is always named. Like every output, it combines with the others; `--run=false` writes only it:

```bash
spite game --final-classes=.spite-cache/final --run=false
```

## Errors and usage

With no program, the compiler prints its usage text and exits unsuccessfully. A `.spite` file named instead of
its folder, a folder with no entry file, an entry constructor that takes arguments, a path option for an output
that is off, and a `--name=value` before `--` that names no `Build` field are errors too.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Command line

```
spite program                         build program/program.exe beside the program and run it (programs.md)
spite program --c-source              also write program/program.c (--c-path=path puts it elsewhere)
spite program --executable --run=false   build without running (--executable-path=path puts it elsewhere)
spite program --run=false             compile the whole program and write nothing
spite program --optimized             optimized build
spite program --development           an inspectable build: no tree shaking, internals as ordinary objects (D143)
spite program --hot-reload            swap changed classes into the running program (implies --development)
spite program --repl                  run with an in-place REPL
spite program --repl-port=4000        run with a remote REPL an AI can connect to, to explore memory and debug
spite program --serve=true            decide a Build field the program declares while compiling (programs.md)
spite program -- --serve=true         run it with a setting its Environment declares (programs.md)
spite program --final-classes=folder  write the final class folder
spite connect 4000                    talk to a running --repl-port program (see repl.md)
spite connect 4000 --command="..."    send one REPL command, print its raw JSON response line, and exit
spite program -- ada --player=x       run it, passing everything after -- to the program's Arguments
spite program --c-source --run=false --target-operating-system=linux   write the C for another system (metaprogramming.md and packages.md)
spite format <file-or-folder> ...     format files without compiling (recursive on a folder)
spite format --check <path> ...       rewrite nothing; exit 1 listing (to stdout) every file that would change
spite reload program ... --executable-path=<running>   what a --hot-reload program runs to rebuild itself
```

**Inspectable and production builds** (D143, decided by Mortaro; the readings below proposed by Claude,
unconfirmed; implemented 2026-09-25). A build with `--development`, `--hot-reload`, `--repl` or `--repl-port` is
*inspectable*: nothing is tree-shaken, and the singletons that hold nothing (`Memory`, `Build`, `TypedMemory<T>`,
[Singletons](classes_and_files.md#singletons--implemented)) are ordinary objects -- allocated at first use, listed by `.instances`, destroyed at exit -- so the REPL
and reflection see the internals as normal classes. Every other build, the ordinary one included and not only
`--optimized`, is a *production* build, where those singletons are static objects and everything unused is
tree-shaken. The optimisations that change only speed (chains as one loop, placement, text appended in place)
apply in both; `docs/optimizations.md` says which build each optimisation applies in
(`conformance/stage6/development_internals`).

Flags mix freely (an `--optimized` build may keep the REPL, and is then inspectable). Building and running are
the same command. Every flag is a field of `Build` ([Build settings: `Build`](programs.md#build-settings-build--implemented)), and a program is named only by its folder (D89,
D130): a path to a `.spite` file is an error naming the folder form, and the compiler compiles itself as `spite bootstrap`, whose entry is
`bootstrap/bootstrap.spite`, class `Bootstrap`.

**The whole program first, then the outputs** (D128, D129; the spelling below proposed by Claude, unconfirmed;
implemented 2026-09-25). The compiler reads the launcher, the library, the program's folder, its `build.spite` and
every `load` before it reads any option, and then decides what to produce from the program's resolved `Build`,
so a program's own `build.spite` can set every option. The one exception is `target_operating_system`, which the
launcher needs to know which folder of `library/` belongs to the program, so it is read from the flag alone. The
outputs are `Boolean` fields, and every one that is on comes from the same compile: `run` (default `true`: build the
executable and run it), `executable` (build it without running), `c_source` (write the C), and `final_classes`, which stays the folder to write to
(`""` is off), since any folder inside the program would be read back as part of it. With every output off the
compiler still compiles the whole program and reports its errors. Tree shaking and every other whole-program
step run before any output is written.

- **Where outputs go** (D129): `executable_path` and `c_path`, each `""` by default, which means beside the
  program: `game/game.exe` (`game/game` for Linux and macOS) and `game/game.c`. A path option given on the
  command line for an output that is off is an error naming the missing flag. What describes an executable stays
  beside it wherever it goes: the `.crashes` map ([Failure: three outcomes and no others](failure.md#failure-three-outcomes-and-no-others--partial)) and a `--hot-reload` build's `.reload_host`,
  `.reload_files` and `_reload_<n>` libraries. The one intermediate is the C the C compiler reads when `c_source`
  is off, written to the language repository's `.spite-cache/<name>.c`.
- **Formatting is an output** (D128): after the whole program is read, a file of the program whose formatted text
  differs is rewritten, and if any was, the program is read again from disk before anything else is produced, so
  what is compiled -- and every line an error names -- is the formatted file.
- **The compiler's own C goes to its default path**, `bootstrap/bootstrap.c`: every `Build` field is a constant in
  what is built, so a `--c-path` naming a different file each run would be written into the C and the fixpoint
  would never hold.
- `--mode`, `--output`, `--mode=tokens`, `--mode=tree` and the `.spite`-file forms of `--mode=format` are gone;
  `spite format` formats files itself, and `spite reload` is a command the running program uses, not an output.

**Where the compiler's flags end** (proposed by Claude, unconfirmed; implemented 2026-09-23): at the first bare
`--`. Everything after it reaches the program's `Arguments` verbatim -- `arguments.get(0)` is the first,
`arguments.player` reads `--player=...`, and `Environment` reads the settings it declares -- and none of it is
read as a compiler flag or a file to compile. A program's own `Arguments` stops at `--` the same way, which is the ordinary meaning of `--`.
Before this, a program run by the compiler received no arguments at all (`conformance/stage6/program_arguments`).

**Where a program runs** (proposed by Claude, unconfirmed; implemented 2026-09-24): in the folder `spite` was run
from. The compiler finds `launcher/` and `library/` from its own executable -- the first folder above it that
holds `launcher/launcher.spite` -- and never from the working directory, so `bin/spite` no longer changes
directory, and a relative path a program opens (`File`, `Directory`, a cache folder) is the caller's. The
launcher's `load` paths are relative to that folder, and the executable is built beside the program (D129), so
running a program leaves nothing in the caller's folder. Before this, every program ran with the repository as its working
directory (`check.sh` runs `conformance/stage6/working_directory` from another folder).

- **Flags are kebab-case, fields stay snake_case** (D188, decided by Mortaro): `--repl-port=4000` sets
  `Build.repl_port`, `--hot-reload` sets `hot_reload`, and a program's own field `worker_stack_size` is
  `--worker-stack-size=256` (`conformance/stage6/build_settings`). An underscore in a flag's name is an error
  naming the hyphen form, "'--repl_port' is written '--repl-port': a flag is kebab-case, and it sets the Build
  field 'repl_port'" (`diagnostics/underscore_flag`); the value after `=` is never touched. The readings below are
  **(proposed by Claude, unconfirmed)**: the rule covers the compiler's flags before the `--`, and every message
  that names a flag names the kebab form; a program's run-time settings after the `--`, read by `Environment` when
  the program runs, keep their field's own spelling for now (`-- --player-name=x`), since translating them is a
  question of its own (`mortaros_missing_decisions.md`). Compile time only (D177).
- Before the `--`, a `--name=value` sets the `Build` field of that name, and one that names no field is an error
  that shows both places a setting can belong -- `build.spite` to decide it while compiling, `environment.spite`
  and `spite program -- --name=value` to read it when the program runs -- so typos never pass silently
  (`diagnostics/unknown_compiler_flag`).
- **Automatic formatting (milestone 7a; since D128 an output decided after the whole program is read).** Every
  `spite program ...` compile formats every `.spite`
  file that belongs to the program -- the entry file's own folder (flat, matching [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)'s discovery
  rule), plus every `load`-ed root, recursively -- rewriting a file only when its formatted text differs
  from what is on disk, and printing `formatted <path>` to
  stderr for each one, then reads the program again when one changed. A file with a parse error is left untouched (the ordinary diagnostics still report it);
  a file the formatter's own safety check refuses to touch (see [Style](style.md#style--implemented)) is left as it is
  and its reason is printed as an error, which stops the compile. Nothing turns this off (D190, which removed
  `--format=false` and the `format` field): a test input kept deliberately unformatted, like `diagnostics/`, is
  compiled from a copy by `check.sh`, so the formatting lands on the copy.
- `spite format <file-or-folder> ...` runs the same formatter standalone, without compiling, and is a command of
  the compiler itself (like `spite connect`), not an option: a file formats just
  itself, a folder recurses into every `.spite` file under it except `.spite-cache/` folders (no bundle/`load`
  awareness -- every file found is formatted, unconditionally). `--check` rewrites nothing and instead lists (to stdout) every file
  that would change, exiting 1 if that list is non-empty (0 if the whole tree is already clean).
