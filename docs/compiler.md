# Compiler command line

`spite` compiles one program. A program is a folder, and its entry file is the one named after the folder:
`spite game` compiles `game/game.spite`, whose class `Game` is constructed to start the program. Every option is
a field of the [`Build`](metaprogramming.md#build-settings-build) singleton, given as `--name=value` (a `Bool`
option may be given bare, `--optimized`). The default action is to build and run the program.

## Name the program

```bash
spite game
spite game/
```

The folder supplies the program's other classes, and the entry class's constructor takes no arguments: the
program reads its command line through `Environment()`, what its build decided through `Build()`, and the raw
arguments through `Arguments()`. A path to a `.spite` file still works, which is how the compiler compiles
itself (`spite bootstrap/spite_compiler.spite`, whose folder is not named after it) and how `--mode=format`
names the file to format.

## Choose an output mode

`--mode=run` is the default. It emits C into `.spite-cache/`, builds an executable, and runs it:

```bash
spite game
spite game --mode=run
```

`--mode=c` prints generated C to standard output. Redirect it when a C file is wanted:

```bash
spite game --mode=c > game.c
```

`--mode=build` builds the executable but does not run it. It requires `--output=path`:

```bash
spite game --mode=build --output=build/game.exe
```

`--mode=tokens` prints the lexer tokens, including their source positions, `--mode=tree` the parsed syntax tree,
and `--mode=format` / `--mode=check_format` format one file:

```bash
spite game/game.spite --mode=tokens
spite game/game.spite --mode=tree
```

## Build options

Every option is a `Build` field with a literal default, declared in `library/build.spite`:

| Option | Default | What it does |
|---|---|---|
| `mode` | `"run"` | `run`, `c`, `build`, `tokens`, `tree`, `format`, `check_format` |
| `output` | `""` | where `--mode=build` writes the executable |
| `optimized` | `false` | asks the C compiler for `-O2` instead of `-O0` |
| `development` | `false` | keeps everything, no tree shaking, for live reload |
| `repl` | `false` | runs the program with an in-place REPL |
| `repl_port` | `0` | reserved for the remote REPL |
| `format` | `true` | formats the program's files before compiling; `--format=false` skips it |
| `debug_memory` | `false` | counts allocations and frees and prints them when the program ends |
| `final_classes` | `""` | writes the merged classes to this folder instead of compiling |
| `operating_system` | the compiling machine | cannot be given: it is the system doing the compiling |
| `target_operating_system` | `operating_system` | the system the program is compiled for |

```bash
spite game --optimized
spite game --debug_memory
spite game --mode=build --output=build/game.exe --debug_memory
```

A program may give an option a different default by reopening `Build` in its own `build.spite`
(`var optimized = true`), and add options of its own the same way. `mode` and `format` are read from the flag
only, because the compiler needs them before it has read the program. Every `Build` field is a constant in the
built program, so `if Build().debug_memory { }` keeps one branch.

The compiler uses the `CC` environment variable when set; otherwise it tries `cc`, `clang`, then `gcc`:

```bash
CC=clang spite game --optimized
```

## Compile for another system

`--target_operating_system=linux` (or `windows`, or `mac`) compiles for that system from any machine: it loads
`library/linux/` instead of this machine's folder, and `Build().target_operating_system` is `"linux"` in the
program. `check.sh` uses it to hold the folders it cannot run to compiling:

```bash
spite bootstrap/spite_compiler.spite --mode=c --target_operating_system=linux > compiler_linux.c
```

## Pass settings to the program

Everything after the first bare `--` belongs to the program, not the compiler. A program reads its run-time
settings from the `Environment` singleton, whose fields it declares by reopening `Environment` in its own
`environment.spite` (see [metaprogramming.md](metaprogramming.md#program-settings-environment)):

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

## Inspect merged classes

`--final_classes=folder` writes the discovered, merged classes as readable `.spite` files -- one per class, under
the namespace folders it belongs to. It is useful after `load(...)` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program, which `check.sh` proves on every run. `Build` is printed with the defaults it
was declared with, not the values this build folded, so running the printed program takes its flags again.

What is written is what the program **ends up with**, not what was written down: the classes come from the
generator after it has run, so a class tree shaking removed is not there, a generic template is not there, and
each of its instantiations is.

`built_in/` holds the classes the compiler provides rather than a file -- `Console`, `File`, `Directory`,
`Process`, `Program` -- and `instantiated/` holds one file per generic instantiation, named for the values it
was given (`WeaponIntTrue`, `PairStringInt`). Both are written as `type` declarations, because a `type` is how
Spite already names members without bodies.

Still missing: `Int`, `String`, `List<T>` and `Dictionary<T>` have no class table inside the compiler at all --
they are handled by name in the generator, so there is nothing to print until they become real classes
(manual.md section 15, "Pure Spite").

Which root supplied each declaration is **not** shown yet. It cannot be a comment, since a comment is only ever
a link to a markdown heading (manual.md section 12), so it needs a form of its own -- see manual.md's open
questions.

```bash
spite game --final_classes=.spite-cache/final
```

This inspection mode takes precedence over `--mode`: it discovers and writes classes without generating or
running C.

## Errors and usage

With no program, the compiler prints its usage text and exits unsuccessfully. An unreadable entry file, an entry
constructor that takes arguments, an unknown `--mode`, a missing `--output` for build mode, and a `--name=value`
before `--` that names no `Build` field are errors too.
