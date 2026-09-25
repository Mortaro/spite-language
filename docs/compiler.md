# Compiler command line

`spite` compiles one program. A program is a folder, and its entry file is the one named after the folder:
`spite game` compiles `game/game.spite`, whose class `Game` is constructed to start the program. The compiler
first reads the **whole** program -- the launcher, the standard library, the program's folder, its `build.spite`
and every folder it `load`s -- and only then decides what to produce, from the program's
[`Build`](programs.md#compile-time-settings-build). Every option is a field of `Build`, given as `--name=value`
(a `Bool` option may be given bare, `--optimized`). By default the compiler builds the executable beside the
program and runs it.

```
spite program                         build program/program.exe beside the program, and run it
spite program --optimized             an optimized build
spite program --development           keep everything (no tree shaking), for live reload
spite program --debug_memory          count allocations and frees, and print the balance at the end
spite program --repl                  run it, then open a REPL on the running program
spite program --repl_port=4000        serve a REPL on 127.0.0.1:4000 while it runs
spite program --hot_reload            swap edited classes into the running program, keeping its state
spite program --serve=true            decide a Build field the program declares, while compiling
spite program -- --serve=true         run it with a setting its Environment declares
spite program --c_source              write program/program.c too, then build and run
spite program --c_source --run=false  write the C and nothing else
spite program --executable --run=false            build the executable without running it
spite program --executable --c_source --run=false build it and write its C, in one compile
spite program --executable_path=build/game.exe    put the executable somewhere else (--c_path= for the C)
spite program --run=false             compile the program and write nothing: its errors, if any
spite program --final_classes=folder  also write the program back out as Spite, every class as it ended up
spite program --format=false          leave the program's files as they are written
spite program --target_operating_system=linux --c_source --run=false   write the C for another system
spite format <file-or-folder> ...     format files without compiling them
spite format --check <path> ...       rewrite nothing; fail listing every file that would change
spite connect 4000                    talk to a running --repl_port program
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
Each output is a `Bool` field, and every one that is on is produced by the same compile:

| Output | Default | What it produces |
|---|---|---|
| `run` | `true` | builds the executable, then runs it with everything after `--` |
| `executable` | `false` | builds the executable without needing to run it |
| `c_source` | `false` | writes the generated C |
| `format` | `true` | rewrites the program's own files in the one style ([below](#formatting)) |
| `final_classes` | `""` | writes the program back out as Spite into this folder ([below](#inspect-merged-classes)) |

Running a program runs its executable, so `run` builds it too, in the same place `executable` would. With every
output off (`--run=false`), the compiler still reads and compiles the whole program and reports its errors, and
writes nothing:

```bash
spite game                                      # build game/game.exe and run it
spite game --c_source                           # the same, and write game/game.c to read
spite game --executable --c_source --run=false  # build game/game.exe and write game/game.c, run nothing
spite game --run=false                          # only check that it compiles
```

Whole-program steps -- tree shaking, the constants `Build` folds, which templates are made -- run on the complete
program before any output is written, so the C written beside an executable is the C that executable was built
from.

## Where the outputs go

Each output has its own path option, and without one it is written **beside the program**, in its own folder:

| Output | Path option | Default |
|---|---|---|
| the executable | `executable_path` | `game/game.exe` (`game/game` on Linux and macOS) |
| the C | `c_path` | `game/game.c` |

```bash
spite game --executable --run=false --executable_path=build/game.exe
spite game --c_source --run=false --c_path=build/game.c
```

A folder the path names is created. A path option given for an output that is off is an error saying which flag
is missing (`--c_path` without `--c_source`), so a path never passes silently. Every `Build` field is a constant
in the built program, the paths included.

What a build writes **beside its executable**, wherever that is: `game.crashes`, the map from a crash report's
site ids to their lines ([failure.md](failure.md)), and, for a `--hot_reload` build, `game.reload_host`,
`game.reload_files` and each reload's `game_reload_1.dll` with its C ([repl.md](repl.md#live-reload---hot_reload)).
The one intermediate that is not an output is the C the C compiler reads when `--c_source` is off: it goes to the
language repository's `.spite-cache/game.c`, and is overwritten by the next build of a program of that name.

`spite reload <folder> ... --executable_path=<running executable>` is what a `--hot_reload` program runs to
rebuild itself: given the options it was built with, it compiles only the classes whose files changed into a
library beside the executable and prints what it rebuilt ([repl.md](repl.md#live-reload---hot_reload)).

## Build options

Every option is a `Build` field with a literal default, declared in `library/build.spite`:

| Option | Default | What it does |
|---|---|---|
| `run` | `true` | builds the executable and runs it |
| `executable` | `false` | builds the executable |
| `executable_path` | `""` | where the executable goes; `""` is beside the program |
| `c_source` | `false` | writes the generated C |
| `c_path` | `""` | where the C goes; `""` is beside the program |
| `format` | `true` | rewrites the program's own files in the one style; `--format=false` leaves them |
| `final_classes` | `""` | writes the merged classes to this folder |
| `optimized` | `false` | asks the C compiler for `-O2` instead of `-O0` |
| `development` | `false` | keeps everything, no tree shaking, for live reload |
| `repl` | `false` | runs the program with an in-place REPL |
| `repl_port` | `0` | serves the remote REPL on this port (see [repl.md](repl.md)) |
| `hot_reload` | `false` | swaps changed classes into the running program and implies `development` (see [repl.md](repl.md#live-reload---hot_reload)) |
| `debug_memory` | `false` | counts allocations and frees and prints them when the program ends |
| `operating_system` | the compiling machine | cannot be given: it is the system doing the compiling |
| `target_operating_system` | `operating_system` | the system the program is compiled for |
| `program` | the folder named | cannot be given: it is the folder on the command line, which the launcher loads |

```bash
spite game --optimized
spite game --debug_memory
spite game --executable --run=false --executable_path=build/game.exe --debug_memory
```

A program may give an option a different default by reopening `Build` in its own `build.spite`
(`var optimized = true`, or `var c_source = true` to always write its C), and add options of its own the same
way. No option is read before the program is, so every one of them, the outputs and `format` included, can be
given a default there -- except `target_operating_system`, which the launcher needs to know which folder of
`library/` is part of the program before it can read the rest, so it comes from the flag alone. Every `Build` field is a constant in the built program, so with `var build = Build()`
beside the attributes, `if build.debug_memory { }` keeps one branch.

The compiler uses the `CC` environment variable when set; otherwise it tries `cc`, `clang`, then `gcc`:

```bash
CC=clang spite game --optimized
```

## Compile for another system

`--target_operating_system=linux` (or `windows`, or `mac`) compiles for that system from any machine: it loads
`library/linux/` instead of this machine's folder, and `build.target_operating_system` is `"linux"` in the
program. `check.sh` uses it to hold the folders it cannot run to compiling:

```bash
spite bootstrap --c_source --run=false --c_path=compiler_linux.c --target_operating_system=linux
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

**The compiler is the formatter.** Formatting is one of the outputs, decided like the others after the whole
program has been read: with `format` on (the default), each of the program's own files -- the entry folder and
every `load`-ed root, not `library/` -- whose formatted text differs from what is on disk is rewritten, printing
`formatted <path>`. If any file was rewritten, the program is read again from disk before anything else is
produced, so what is compiled, and every line an error names, is the formatted file. `--format=false`, or
`var format = false` in the program's `build.spite`, leaves the files as they are.

`spite format` runs the same formatter on files rather than on a program: each file named, and every `.spite`
file under each folder named (the whole tree, `library/` included, except `.spite-cache/` folders), without
compiling anything. `spite format --check` rewrites nothing and exits 1 listing every file that would change.
What it rewrites, and what it refuses to (naming), is [style.md](style.md).

## Development builds and tree shaking

A normal build keeps only what the program uses: a condition on a codegen value or a `Build` field keeps one
branch, a template exists only for the names called, and reflection only where it is read
([metaprogramming.md](metaprogramming.md#tree-shaking)). `--development` keeps everything instead, so a
condition on a codegen value becomes a run-time `if` that live reload could flip, and every class in the folder is
emitted.

## Counting memory: `--debug_memory`

`--debug_memory` builds the program with an allocation table and prints `allocations: N frees: N` when it ends.
The two numbers differ only when something leaked, and then the report names the classes whose instances are
still alive, which is how a leaked cycle shows up ([memory.md](memory.md#cycles-leak)). Every program in
`conformance/`, `examples/` and these pages is run this way by `check.sh`, and must balance.

## Inspect merged classes

`--final_classes=folder` writes the discovered, merged classes as readable `.spite` files -- one per class, under
the namespace folders it belongs to. It is useful after `load(...)` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program, which `check.sh` proves on every run. `Build` is printed with the defaults it
was declared with, not the values this build folded, so running the printed program takes its flags again.

What is written is what the program **ends up with**, not what was written down: the classes come from the
generator after it has run, so a class tree shaking removed is not there, a generic template is not there, and
each of its instantiations is.

Every class the program names comes from a file in `library/`, including `Memory`, `DynamicLibrary`, `String`
and the numbers (`Int`, `Long`, `Double`, ...), so every one of them is printed like a class of your own. A
function whose body the compiler supplies -- the floor that stays C, such as `Memory.allocate_bytes` -- is added
to its class as the compiler's own reopening, and is printed as a declaration without a body. The printed
`memory.spite` begins:

```gdscript
singleton

func text(address: Long, length: Long): String {
    var bytes = allocate_bytes(length + 1)
    copy_bytes(address, bytes, length)
    return String(bytes, length)
}

func terminated_text(address: Long): String {
    var length: Long = 0
    while read_byte(address, length) != 0 {
        length = length + 1
    }
    return text(address, length)
}

func allocate_bytes(bytes: Long): Long
func resize(address: Long, bytes: Long): Long
func free(address: Long)
```

A declaration without a body is what the compiler reads back, so the printed program still compiles: the
printed `memory.spite` reopens `Memory` with the same members, and the compiler supplies the same bodies again.
Anywhere else, a `func` with no body is an error, because only the compiler can supply one.

`instantiated/` holds one file per generic instantiation, named for the values it was given
(`weapon_int_true.spite`, class `WeaponIntTrue`), written as a `type` with the members that instantiation has.
`Build` is printed with its declared defaults, and `Launcher` with the rest.

Which root supplied each declaration is **not** shown yet. It cannot be a comment, since a comment is only ever
a link to a markdown heading ([style.md](style.md#comments-are-links)), so it needs a form of its own
([manual, open question 10](../manual.md#open-questions)).

The folder has no default: anything inside the program's own folder would be read back as part of the program,
so the folder is always named. Like every output, it combines with the others; `--run=false` writes only it:

```bash
spite game --final_classes=.spite-cache/final --run=false
```

## Errors and usage

With no program, the compiler prints its usage text and exits unsuccessfully. A `.spite` file named instead of
its folder, a folder with no entry file, an entry constructor that takes arguments, a path option for an output
that is off, and a `--name=value` before `--` that names no `Build` field are errors too.
