# Compiler command line

`spite` compiles one entry `.spite` file. The entry file may be supplied positionally or with `--file=`, and every
other option uses `--name=value` unless noted otherwise. The default action is to build and run the program.

## Select the entry file

These forms are equivalent:

```bash
spite game/game.spite
spite --file=game/game.spite
```

The entry file must end in `.spite`. Its containing folder supplies the program's other classes, and the entry
class is constructed to start the program.

## Choose an output mode

`--mode=run` is the default. It emits C into `.spite-cache/`, builds an executable, and runs it:

```bash
spite game/game.spite
spite --file=game/game.spite --mode=run
```

`--mode=c` prints generated C to standard output. Redirect it when a C file is wanted:

```bash
spite game/game.spite --mode=c > game.c
```

`--mode=build` builds the executable but does not run it. It requires `--output=path`:

```bash
spite game/game.spite --mode=build --output=build/game.exe
```

`--mode=tokens` prints the lexer tokens, including their source positions. It is useful when checking how a piece
of source was read:

```bash
spite game/game.spite --mode=tokens
```

`--mode=tree` prints the parsed syntax tree rather than generating C:

```bash
spite game/game.spite --mode=tree
```

The only accepted mode names are `run`, `c`, `build`, `tokens`, and `tree`.

## Build options

`--optimized` asks the C compiler for an optimized build (`-O2`). Without it, the compiler uses `-O0`:

```bash
spite game/game.spite --optimized
```

`--debug_memory=true` enables allocation accounting in a built or run program. The corpus uses it to require that
allocations and frees balance:

```bash
spite game/game.spite --mode=run --debug_memory=true
spite game/game.spite --mode=build --output=build/game.exe --debug_memory=true
```

Any value other than the exact text `true` leaves memory debugging off. It does not affect `c`, `tokens`, or
`tree` output.

The compiler uses the `CC` environment variable when set; otherwise it tries `cc`, `clang`, then `gcc`:

```bash
CC=clang spite game/game.spite --optimized
```

## Pass settings to the program

Everything after the first bare `--` belongs to the program, not the compiler. A program reads its settings from
the `Environment` singleton, whose fields it declares by reopening `Environment` in its own `environment.spite`
(see [metaprogramming.md](metaprogramming.md#program-settings-environment)):

```bash
spite server/server.spite -- --name=production
```

Before the `--`, a `--name=value` is either one of the compiler's own flags (`file`, `mode`, `output`,
`debug_memory`, `final-classes`, and `runtime`, reserved for compiler work with no user-facing behavior yet) or
a setting the program's `Environment` declares -- and a setting given to the compiler is **hardcoded into the
build** instead of read when the program runs
(see [metaprogramming.md](metaprogramming.md#settings-given-to-the-compiler-are-hardcoded)):

```bash
spite server/server.spite --serve=true -- --name=production
```

Any other `--name=value` there is an error that shows both places it could belong. `--operational_system=linux`
is such a setting: it picks the `library/linux/` folder and writes C for Linux from any machine.

## Inspect merged classes

`--final-classes` writes the discovered, merged classes as readable `.spite` files -- one per class, under the
namespace folders it belongs to. It is useful after `load(...)` or a reopening: the file holds the declarations
that won, so reopening stops being invisible. What it writes is a program, not a report: running the printed
entry file runs the same program, which `check.sh` proves on every run.

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
spite game/game.spite --final-classes
spite game/game.spite --final-classes=build/final-classes
```

The bare form writes to `.spite-cache/final/`; the assigned form uses the directory after `=`. This inspection mode
takes precedence over `--mode`: it discovers and writes classes without generating or running C.

## Errors and usage

With no entry file, the compiler prints its usage text and exits unsuccessfully. An unreadable entry file, an
unknown `--mode`, a missing `--output` for build mode, or a `--name=value` before `--` that is not a compiler flag is also an error. The
compiler does not accept planned flags such as `--development`, `--repl`, `--format`, or `--no-format` yet.
