# Programs: the entry, the launcher, and settings

A program is a folder. Its **entry file** is the file named after the folder -- `game/game.spite`, class `Game`
-- and the program runs by constructing that class. There is no `main` and no entry point declaration: the
entry class's constructor *is* the program, and when it returns the program ends with exit code `0`.

```bash
spite game
```

The folder is the only way to name a program: `spite game/game.spite` is an error that says `spite game`. By
default the build writes the executable beside the program, `game/game.exe`, and runs it
([compiler.md](compiler.md#where-the-outputs-go)).

Every other `.spite` file in the folder is another class of the program, and every folder inside it is a
namespace ([packages.md](packages.md)): `game/engine/renderer/renderer.spite` is `Engine.Renderer`.

## The entry constructor takes no arguments

The command line is not handed to the entry class. It is read where it is needed, through three singletons that
work anywhere:

| Read it with | What it is |
|---|---|
| `Environment()` | the program's run-time settings, which it declares ([below](#run-time-settings-environment)) |
| `Build()` | what was decided when the program was compiled ([below](#compile-time-settings-build)) |
| `Arguments()` | the raw command line the program was given |

So an entry constructor with parameters is an error that names all three:

```gdscript title=entry_arguments_error/entry_arguments_error.spite entry error
var console = Console()

func EntryArgumentsError(name: String) {
    console.print(name)
}
```
```diagnostic
is the entry constructor, and it takes no arguments
```

A constructor is setup, not logic, and the entry constructor is no exception: `assert` is not allowed in any
constructor ([failure.md](failure.md#assert-is-not-allowed-in-a-constructor)). A program whose constructor grows
complicated splits it into functions it calls, and then reads as a table of contents.

## `Arguments()`: the raw command line

`Arguments()` answers the arguments the program was given, which is everything after the first bare `--` when
the compiler runs it (`spite game -- ada --player=knight`). `count()` and `get(index)` read them by position, and
`.player` reads the value of `--player=...` as a `String?`: `null` when the flag was not given.

```gdscript title=raw_arguments/raw_arguments.spite entry vars=player:knight,rounds:3
var console = Console()

func RawArguments() {
    var arguments = Arguments()
    var arguments_count = arguments.count()
    console.print("arguments", arguments_count)
    announce(arguments)
}

func announce(arguments: Arguments) {
    assert arguments.player
    console.print("player", arguments.player)
}
```
```output
arguments 2
player knight
```

That run is `spite raw_arguments -- --player=knight --rounds=3`. A setting the program always wants is better
declared in `Environment`, which gives it a type and a default.

## Run-time settings: `Environment`

A program's settings are the fields of `Environment`, a singleton in the standard library. The program reopens
it with a file of its own named `environment.spite`, holding one `var` per setting with a literal default, and
reads it with `Environment()` anywhere:

```gdscript title=program_settings/environment.spite
var endpoint = "local"
var workers = 1
var verbose = false
```
```gdscript title=program_settings/program_settings.spite entry vars=endpoint:production,verbose:true
var console = Console()
var environment = Environment()

func ProgramSettings() {
    if environment.endpoint == "production" {
        console.print("using the production endpoint")
    } else {
        console.print("using the local endpoint")
    }
    console.print("workers", environment.workers, "verbose", environment.verbose)
}
```
```output
using the production endpoint
workers 1 verbose true
```

Each field is read once, when `Environment()` is first made, from the first of:

1. the program's own command line, `--endpoint=production` (after `--` when the compiler runs it:
   `spite program_settings -- --endpoint=production`);
2. the process environment variable named by the field in upper case, `ENDPOINT`;
3. the declared default.

Only declared fields are read, so nothing the program does not name is ever loaded. The default's literal is the
setting's type -- `false` is a `Bool`, a whole number an `Int`, `""` a `String` -- and anything else, a type
annotation included, is an error naming the three forms. A value that is not of the setting's type
(`--verbose=maybe`) crashes when `Environment()` is first made: the program was started wrong, and there is
nothing sensible to continue with. The values are read when the program runs, so `if environment.verbose` is an
ordinary `if`, with both branches compiled in.

```gdscript title=setting_type_error/environment.spite
var workers: Int = 1
```
```gdscript title=setting_type_error/setting_type_error.spite entry error
var console = Console()
var environment = Environment()

func SettingTypeError() {
    console.print(environment.workers)
}
```
```diagnostic
so it is declared with only a literal default, and the literal is its type
```

## Compile-time settings: `Build`

`Environment` is read when the program **runs**. `Build` is its twin for when the program is **compiled**: a
singleton in the standard library (`library/build.spite`) whose fields are decided by the compiler and written
into the program as constants. Every compiler option is one of them ([compiler.md](compiler.md#build-options)),
and a program adds its own by reopening `Build` in a file named `build.spite`:

```gdscript title=build_settings/build.spite
var serve = false
```
```gdscript title=build_settings/environment.spite
var name = "client"
```
```gdscript title=build_settings/build_settings.spite entry build=serve:true vars=serve:false,name:tester
var console = Console()
var build = Build()
var environment = Environment()

func BuildSettings() {
    if build.serve {
        console.print("serving as", environment.name)
    } else {
        console.print("this branch is not in the build")
    }
}
```
```output
serving as tester
```

That is `spite build_settings --serve=true -- --serve=false --name=tester`. A `--name=value` given to the
**compiler** -- before the `--` -- sets the `Build` field of that name; a field nobody sets keeps the default it
was declared with. Either way `build.serve` is the literal `true` in the built program: `if build.serve { }` is
decided while compiling, the branch it does not take is never generated, and the program's own `--serve=false`
changes nothing. `name` belongs to `Environment`, so it is still read when the program runs.

- A `Bool` field may be given bare: `--optimized` is `--optimized=true`. Any other bare flag is an error naming
  the value it needs.
- The value has to be of the field's type (`--workers=many` for an `Int` is a compile error), a flag naming a
  field of `Environment` is a compile error that says to pass it after `--`, and a flag naming no field at all is
  a compile error listing the fields `Build` has. A typo never passes silently.
```gdscript title=unknown_flag_error/unknown_flag_error.spite entry error build=verbos:true
var console = Console()

func UnknownFlagError() {
    console.print("never built")
}
```
```diagnostic
'--verbos' is not a compiler option, and this program's Build declares no setting 'verbos'
```

- A program's own `build.spite` may give a compiler option a different default: `var optimized = true` builds it
  optimized unless `--optimized=false` is given. There are no exceptions: the compiler reads the whole program
  before it decides anything, so what it produces -- `var c_source = true` writes the program's C beside it on
  every build, `var run = false` only builds it -- and whether it formats the files are defaults like the rest
  ([compiler.md](compiler.md#choose-the-outputs)).

### The operating system: `operating_system` and `target_operating_system`

`build.operating_system` is the system doing the compiling, and `build.target_operating_system` the one the
program is compiled for: `"windows"`, `"linux"` or `"mac"`. The target defaults to the compiling system, and
`--target-operating-system=linux` compiles for Linux from any machine. Both are constants, so a condition on
them keeps one branch:

```gdscript title=target_system/target_system.spite entry
var console = Console()
var build = Build()

func TargetSystem() {
    var target = build.target_operating_system
    var known = target == "windows" or target == "linux" or target == "mac"
    console.print("known system", known)
    console.print("compiled for this machine", target == build.operating_system)
}
```
```output
known system true
compiled for this machine true
```

Neither `operating_system` nor `program` can be given as a flag: the compiler supplies both. Which operating
system a program targets also decides which folder of the standard library is loaded, which is the next
section.

## How a program is loaded

Nothing about starting a program is hidden in the compiler. Running one is itself a Spite program,
`launcher/launcher.spite` at the root of the repository, and its constructor is the whole story. Its `load`
paths are relative to the repository, which the compiler finds from its own executable; the program itself runs
in the folder `spite` was run from ([compiler.md](compiler.md#where-it-runs)).

```gdscript
var build = Build()

func Launcher() {
    load "library"
    load "library/{build.target_operating_system}"
    load build.program
}
```

The compiler reads that file first and follows its `load` lines in order:

1. `library/`, the standard library every system shares;
2. the folder of the operating system the program is compiled for -- `library/windows/`, `library/linux/` or
   `library/mac/`, each loaded only by name -- whose files reopen the classes that system does differently
   ([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes));
3. the program's own folder, `build.program`, the folder named on the command line.

A `load` in the launcher may use text and the `Build` fields the compiler knows before reading anything; a
`load` anywhere else takes a literal. Loading the program's folder is what runs it: that one `load` constructs
the program's entry class (and runs the `--repl` loop around it, when asked), and the executable's `main` does
nothing but construct `Launcher`.

```gdscript title=loaded_program/loaded_program.spite entry
var console = Console()
var build = Build()

func LoadedProgram() {
    var named_folder = build.program.ends_with("loaded_program")
    console.print("loaded from", named_folder)
    console.print("for this machine", build.target_operating_system == build.operating_system)
}
```
```output
loaded from true
for this machine true
```

`--final-classes` writes `Launcher` out with the rest of the program, so the printed program shows how it is
loaded too. `Launcher` counts as library code: `Spite.Class.instances` does not list it. What `main` still does
in C is the floor under this: it hands the command line to `Arguments()`, puts standard output in binary mode on
Windows, and, after `Launcher` returns, releases the singletons and the class objects and prints the
`--debug-memory` balance.

## Configuration is a class

There is no configuration file format. A configuration is an ordinary class whose constructor sets its state,
or -- for anything that should differ between runs or builds -- a field of `Environment` or `Build`, reopened in
the program's own folder. Both are Spite, so a setting can be computed, reflected on, and read in the REPL like
anything else.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Constructors and the entrypoint

A function named like its class is the constructor. A class without one is constructed with `Person()` and just gets its defaults.
There is no `main`: a program is a folder, its entry file is the file named after the folder (`kal/kal.spite`), and
the program is run by constructing that file's class. The entry constructor takes **no arguments** (D89,
`diagnostics/entry_constructor_arguments`): the command line is read through `Environment()` (run-time settings),
`Build()` (what the build decided) and `Arguments()` (the raw list), anywhere.

```kal.spite
func Kal() {
    var some_key = Arguments().some_key
    if some_key {
        console.print(some_key)
    }
}
```

**Nothing starts a program behind its back** (D97; the details proposed by Claude, unconfirmed). Running one is
itself Spite: `launcher/launcher.spite`, at the root of the repository, is the class the executable's `main`
constructs, and its constructor is the whole of how a program is loaded:

```launcher.spite
var build = Build()

func Launcher() {
    load "library"
    load "library/{build.target_operating_system}"
    load build.program
}
```

The compiler reads the launcher first and follows its `load` calls in order, instead of walking the standard
library by a rule of its own: `library/`, then the folder of the operating system the program is compiled for
(`library/windows/`, `library/linux/` and `library/mac/` are loaded only by name), then the program's folder,
`build.program`, the folder named on the command line. A `load` in the launcher may use text and the `Build`
fields the compiler knows before reading anything, read through a `var` of the launcher bound to `Build()` (D110) (`target_operating_system`, `operating_system`, `program`, and
the ones given as flags); anywhere else `load` still takes a literal. Loading the program's folder is what runs
it: that one `load` constructs the program's entry class, runs a `--repl` loop on it, and releases it. `main`
keeps only the floor -- it hands `argv` to `Arguments()`, puts standard output in binary mode on Windows, and after
`Launcher` returns releases the singletons and class objects and prints the `--debug-memory` balance. `Launcher`
is library code for reflection (`Spite.Class.instances` does not list it) and `--final-classes` prints it.

Configuration files are ordinary classes whose constructor sets the state.

### Program settings: `Environment`  **[implemented]**

D76 (decided by Mortaro, 2026-09-23): "using $variables for both command line environment setup and generics was
a bad idea from my end, lets use it just for generics. instead lets have a singleton for dealing with Environment
arguments so entrypoint is not forced to receive the args of the console." `Environment` is a standard library
singleton (`library/environment.spite`), and a program **reopens** it ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)) with a file of its own named
`environment.spite` to declare the settings it has:

```environment.spite
var serve = false
var name = "client"
var workers = 1
```

```gdscript
var environment = Environment()

func Dungeon() {
    if environment.serve {
        ...
    }
}
```

`Environment()` works anywhere, so the entry constructor no longer has to receive `Arguments` only to pass them
down. Only the declared fields are read: nothing the program does not name is loaded, which is what makes it a
deterministic replacement for packages like Node's dotenv. It mirrors Nullstack's `context.environment`, and it is
how a program such as the game engine gets several environments.

What follows is how it is implemented (proposed by Claude, unconfirmed):

- **Where a value comes from.** Each field is read once, when the singleton is first made, from the first of:
  the program's own command line, `--serve=true` (what the program receives -- after `--` when the compiler runs
  it, [Command line](compiler.md#command-line) -- and stopping at the program's own `--`); the process environment variable named by the field
  in upper case, `SERVE`; the declared default. The command line wins because it is the more deliberate of the
  two. An argument that names no field is left alone for `Arguments` to read.
- **The default's literal is the type.** A setting is declared with nothing but a literal default: `false`/`true`
  is a `Bool`, a whole number (negative too) an `Int`, `""` a `String`. A type annotation, or any other default,
  is a compile error naming the three forms (`diagnostics/environment_setting`). Text that is not a value of the
  setting's type (`--serve=maybe`, `--workers=many`) **crashes** when the singleton is made (D24: a malformed
  setting is a bug in how the program was started, and there is nothing sensible to continue with).
- **How the fields get filled.** The compiler prepends one assignment per declared field to `Environment`'s
  constructor -- `serve = boolean_setting("serve", serve)` -- where `boolean_setting`, `integer_setting` and
  `text_setting` are ordinary Spite functions in `library/environment.spite`. The reading itself is Spite; the
  compiler only writes the calls, the way it writes a Symbol codegen function.
- **`Arguments()` is the command line, anywhere.** `library/environment.spite` reads the command line through
  `Arguments()`, which the compiler answers in any function with the program's command line. It is available
  to programs too, and since D89 it is how a program reaches arguments no setting names: the entry constructor
  receives nothing.
- **An `Environment` field is never given to the compiler.** `spite program --serve=true` is an error that says
  to pass it after `--` (`diagnostics/environment_setting_to_compiler`); a value decided while compiling is a
  `Build` field instead (below).

### Build settings: `Build`  **[implemented]**

D84 (decided by Mortaro), then D85 and D86 (decided by Mortaro): "Variables supplied as flags during compilation
stay hardcoded the others are runtime", then "lets already implement Environment into two things, one for
RuntimeEnvironment and one for CompileEnvironment but make better names for it, we can just change later, so
compiler flags are explicitely its own thing. make all our compiler options pass as a normal program from
CompileEnvironment so people can reopen to force them with defaults." `Environment` (above) is read when the
program **runs**; `Build` (`library/build.spite`, a singleton) is decided when it is **compiled**. The names are
Claude's proposal (`mortaros_missing_decisions.md`). Every compiler option is a `Build` field with a literal
default, and a program adds its own the same way it adds `Environment` settings, by reopening `Build` in a file
named `build.spite`:

```build.spite
var serve = false
```

```gdscript
var build = Build()

func Server() {
    if build.serve {
        listen()
    }
}
```

`spite server --serve=true` builds a server; `spite server` builds the one without `listen()` in it. What follows
is how it is built (proposed by Claude, unconfirmed, except where a decision is named):

- **Every field is a constant.** A `--name=value` before the `--` sets the field of that name; a field nobody
  sets keeps its declared default -- the program's own `build.spite` if it reopens it, else `library/build.spite`.
  Either way the value is written into the program: `build.serve` compiles to `true`, a condition on it is decided
  while compiling, the branch not taken is never generated, and nothing is read when the program runs. The field
  still exists on the `Build` singleton with that value, for reflection.
- **The compiler's options are fields.** The outputs `run`, `executable`, `c_source` and
  `final_classes`, the paths `executable_path` and `c_path`, and `optimized`, `development`, `repl`, `repl_port`,
  `hot_reload` and `debug_memory` (the flag names follow the fields: `--final-classes=folder`,
  `--repl-port=4000`). Formatting is not one of them: every compile formats first and nothing turns it off
  (D190), so `--format` and a `format` field in a program's `build.spite` are compile errors. The compiler reads
  them from the program's resolved `Build`, so a program
  whose `build.spite` says `var optimized = true` is built optimized unless `--optimized=false` is given. Since
  D128 no option is read before the program is, so there is no exception for `mode` any more (`mode`
  is gone: [Command line](compiler.md#command-line)); only `target_operating_system`, which says which library folder is part of the program,
  comes from the flag alone.
- **A `Bool` may be given bare**: `--optimized` is `--optimized=true`. Any other bare flag is an error naming the
  value it needs.
- **Nothing passes silently.** A value that is not of the field's type is a compile error
  (`diagnostics/build_setting_type`); a flag naming an `Environment` field is an error that says to pass it after
  `--`; a flag naming no field is an error listing the fields `Build` has (`diagnostics/unknown_compiler_flag`).
- **Two operating systems** (D86): `operating_system` is the system doing the compiling -- the compiler supplies
  it, and giving it is an error -- and `target_operating_system` is the one the program is compiled for, which
  defaults to it. `--target-operating-system=linux` loads `library/linux/` ([Constructors and the entrypoint](#constructors-and-the-entrypoint), the launcher) and folds,
  so `if build.target_operating_system == "windows" { }` keeps one branch. Each system's folder reopens `Build`
  with `var target_operating_system = "linux"`, which is only the default a compiler that does not fold `Build`
  sees -- the seed while bootstrapping. The compiler learns which system it runs on from its own
  `build.target_operating_system`, folded when it was built.
- **`program`** is the folder named on the command line, which the launcher loads; the compiler supplies it and
  giving it is an error.
- **`--final-classes` prints `Build` with its declared defaults**, not the values one build folded, so running the
  printed program takes its flags again instead of, say, printing its classes forever.

This settles the compile-time home D76 left open (D13's isomorphic split and [Other environments](targets.md#other-environments--planned)'s `$target` read a
`Build` field), and answers the reminder Mortaro asked for with D84 -- "we may split these both into two types of
environments for runtime and compile time" -- which D85 did.
