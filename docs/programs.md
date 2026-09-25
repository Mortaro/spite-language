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
setting's type -- `false` is a `Boolean`, a whole number an `Integer`, `""` a `String` -- and anything else, a type
annotation included, is an error naming the three forms. A value that is not of the setting's type
(`--verbose=maybe`) crashes when `Environment()` is first made: the program was started wrong, and there is
nothing sensible to continue with. The values are read when the program runs, so `if environment.verbose` is an
ordinary `if`, with both branches compiled in.

That reading is the whole run-time cost: one pass over the command line and one environment lookup per declared
field, once, when `Environment()` is first made. A program that never makes `Environment()` carries none of it
([rules](#program-settings-environment--implemented)).

```gdscript title=setting_type_error/environment.spite
var workers: Integer = 1
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

- A `Boolean` field may be given bare: `--optimized` is `--optimized=true`. Any other bare flag is an error naming
  the value it needs.
- The value has to be of the field's type (`--workers=many` for an `Integer` is a compile error), a flag naming a
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
  optimized unless `--optimized=false` is given. The compiler reads the whole program before it decides
  anything, so what it produces is a default like the rest -- `var c_source = true` writes the program's C
  beside it on every build, `var run = false` only builds it ([compiler.md](compiler.md#choose-the-outputs)).
  Formatting is not an option: every compile formats first, and `format` is not a field a program may declare.
  `target_operating_system` is the one field only a flag sets ([below](#build-settings-build--implemented)).

`Build` costs nothing when the program runs: every field is a constant written into the program, and nothing is
read or decided at run time.

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
`load` anywhere else names its folder with literal text, though it may sit under an `if` on a `Build` field
([packages.md](packages.md#load-is-a-bundle-boundary)). Loading the program's folder is what runs it: that one `load` constructs
the program's entry class (and runs the `--repl` loop around it, when asked), and of the program the
executable's `main` constructs only `Launcher`.

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
loaded too. What the executable's `main` still does below it -- handing over the command line, and the teardown
after `Launcher` returns -- is listed in [the rules](#constructors-and-the-entrypoint).

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

There is no `main`: a program is a folder, its entry file is the file named after the folder (`kal/kal.spite`,
class `Kal`), and the program is run by constructing that class; when the constructor returns, the program
ends with exit code `0`. A program is named only by its folder (D130): `error: 'game/game.spite' is a file, and
a program is named by its folder: spite game`, and a folder with no entry file named after it is an error naming
the file looked for. No other file may reopen the entry class
([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)).

The entry constructor takes **no arguments** (D89): `'Kal' is the entry constructor, and it takes no arguments:
a program reads its run-time settings through Environment() (declared in its environment.spite, given after
'--'), what its build decided through Build(), and the raw command line through Arguments(), anywhere`
(`diagnostics/entry_constructor_arguments`).

```kal.spite
var console = Console()

func Kal() {
    var arguments = Arguments()
    var some_key = arguments.some_key
    if some_key {
        console.print(some_key)
    }
}
```

**`Arguments()` is the raw command line, anywhere** (D89; the members proposed by Claude, unconfirmed): what
the program was given -- after the first bare `--` when the compiler runs it -- with `count()`, `get(index)`, and
`.name` answering the value of `--name=...` as a `String?`, `null` when that flag was not given
(`conformance/stage6/program_arguments`). The compiler answers it in any function; its only cost is the slot
`main` fills with the process's arguments.

**Nothing starts a program behind its back** (D97, D131; the details proposed by Claude, unconfirmed). Running
one is itself Spite: `launcher/launcher.spite`, at the root of the repository (class `Launcher`), is the class
the executable's `main` constructs, and its constructor, shown in [How a program is loaded](#how-a-program-is-loaded),
is the whole of how a program is loaded:

- The compiler reads the launcher first and follows its `load` lines in order, instead of walking the standard
  library by a rule of its own: `library/`, then the folder of the operating system the program is compiled for
  (`library/windows/`, `library/linux/` and `library/mac/` are loaded only by name), then the program's folder,
  `build.program`, the folder named on the command line. Its paths are relative to the repository, which the
  compiler finds beside its own executable.
- A `load` in the launcher may use text and the `Build` fields the compiler knows before reading anything
  (`target_operating_system`, `operating_system`, `program`, and the ones given as flags), read through a `var`
  of the launcher bound to `Build()` (D110). Anywhere else a `load` names its folder with literal text: `a
  program's 'load' names its folder with text, like 'load "engine"': the compiler reads every loaded folder before
  it compiles, so the folder cannot be computed`; it may sit under an `if` on a `Build` field
  ([`load` is a bundle boundary](packages.md#load-is-a-bundle-boundary)).
- Loading the program's folder is what runs it: that one `load` constructs the program's entry class, runs a
  `--repl` loop on it when asked, and releases it; every other `load` compiles to nothing (D177).
- `main` keeps only the floor: it hands `argv` to `Arguments()`, puts standard output in binary mode on Windows,
  constructs `Launcher`, and after `Launcher` returns releases the singletons and class objects and prints the
  `--debug-memory` balance.
- `Launcher` is library code for reflection (`Spite.Class.instances` does not list it), and `--final-classes`
  prints it with the rest of the program.

**Configuration is a class.** There is no configuration file format: a configuration is an ordinary class whose
constructor sets its state, or a field of `Environment` or `Build`.

### Program settings: `Environment`  **[implemented]**

`Environment` (D76, D132) is a standard library singleton (`library/environment.spite`), and a program
**reopens** it ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)) with a
file of its own named `environment.spite` to declare the settings it has, one `var` per setting. `Environment()`
works anywhere. Only the declared fields are read: nothing the program does not name is loaded, which makes it a
deterministic replacement for packages like Node's dotenv.

What follows is how it is built (proposed by Claude, unconfirmed):

- **Where a value comes from.** Each field is read once, when the singleton is first made, from the first of:
  the program's own command line, `--serve=true` (what the program receives -- after `--` when the compiler runs
  it, [Command line](compiler.md#command-line) -- and stopping at the program's own `--`); the process
  environment variable named by the field in upper case, `SERVE`; the declared default. The command line wins
  because it is the more deliberate of the two. An argument that names no field is left alone for `Arguments` to
  read.
- **A setting's flag is spelled like its field**, `--worker_count=4` for `var worker_count = 1`: D188's
  kebab-case covers the compiler's flags, and whether settings after `--` follow it is open
  (`mortaros_missing_decisions.md` item 153). `--worker-count=4` is not read today; it is left to `Arguments`.
- **The default's literal is the type.** A setting is declared with nothing but a literal default: `false`/`true`
  is a `Boolean`, a whole number (negative too) an `Integer`, `""` a `String`. A type annotation, or any other
  default, is a compile error naming the three forms: `'Environment.workers' is a setting, read from the
  program's command line (--workers=value) or its environment (WORKERS), so it is declared with only a literal
  default, and the literal is its type: 'var workers = false' (Boolean), 'var workers = 0' (Integer) or 'var
  workers = ""' (String)` (`diagnostics/environment_setting`). Text that is not a value of the setting's type
  (`--serve=maybe`, `--workers=many`) **crashes** when the singleton is made (D24: a malformed setting is a bug in
  how the program was started, and there is nothing sensible to continue with).
- **How the fields get filled.** The compiler prepends one assignment per declared field to `Environment`'s
  constructor -- `serve = boolean_setting("serve", serve)` -- where `boolean_setting`, `integer_setting` and
  `text_setting` are ordinary Spite functions in `library/environment.spite`. The reading itself is Spite; the
  compiler only writes the calls, the way it writes a Symbol codegen function
  (`conformance/stage6/environment_settings`).
- **An `Environment` field is never given to the compiler.** `spite program --serve=true` is an error that says
  to pass it after `--`: `'--serve' was given to the compiler, but 'Environment.serve' is read when the program
  runs: give it to the program after '--' (spite program -- --serve=value), or, to decide it when compiling,
  declare it in the program's build.spite instead` (`diagnostics/environment_setting_to_compiler`).
- **Run-time cost** (D177): one pass over the command line and one environment lookup per declared field, once,
  when `Environment()` is first made. A program that never makes `Environment()` has none of its code: the
  class is tree-shaken with the rest of the unused library.

### Build settings: `Build`  **[implemented]**

`Environment` (above) is read when the program **runs**; `Build` (`library/build.spite`, a singleton) is decided
when it is **compiled** (D84, D85, D86; the two names kept by D132). Every compiler option is a `Build` field
with a literal default, and a program adds its own the same way it adds `Environment` settings, by reopening
`Build` in a file named `build.spite`. What follows is how it is built (proposed by Claude, unconfirmed, except
where a decision is named):

- **Every field is a constant.** A `--name=value` before the `--` sets the field of that name; a field nobody
  sets keeps its declared default -- the program's own `build.spite` if it reopens it, else `library/build.spite`.
  Either way the value is written into the program: `build.serve` compiles to `true`, a condition on it is decided
  while compiling, the branch not taken is never generated, and nothing is read when the program runs. The field
  still exists on the `Build` singleton with that value, for reflection.
- **Flags are kebab-case** (D188): `--repl-port=4000` sets the field `repl_port`, and a flag written with `_` is
  an error before anything is read: `'--repl_port' is written '--repl-port': a flag is kebab-case, and it sets
  the Build field 'repl_port'` (`diagnostics/underscore_flag`).
- **The compiler's options are fields** (D85, D128, D129): the outputs `run`, `executable`, `c_source` and
  `final_classes`, the paths `executable_path` and `c_path`, and `optimized`, `development`, `repl`, `repl_port`,
  `hot_reload` and `debug_memory` ([Command line](compiler.md#command-line)). The compiler reads them from the
  program's resolved `Build`, after the whole program is read (D128), so a program whose `build.spite` says
  `var optimized = true` is built optimized unless `--optimized=false` is given.
- **Formatting is not an option** (D190): every compile formats first and nothing turns it off, so `--format`
  and a `format` field in a program's `build.spite` are compile errors (`diagnostics/format_flag`,
  `diagnostics/format_setting`).
- **`target_operating_system` comes from the flag alone**: the launcher needs it to know which library folder is
  part of the program before reading the rest. A `var target_operating_system = ...` in a program's
  `build.spite` is silently overridden today; whether it should be an error is open
  (`mortaros_missing_decisions.md`).
- **A `Boolean` may be given bare** (D120): `--optimized` is `--optimized=true`. Any other bare flag is an error
  naming the value it needs.
- **Nothing passes silently.** A value that is not of the field's type is a compile error: `'--workers=many' was
  given to the compiler, but 'Build.workers' is an Integer: a whole number, like --workers=3`
  (`diagnostics/build_setting_type`); a flag naming an `Environment` field is the error above; a flag naming no
  field is an error listing the fields `Build` has: `'--serve' is not a compiler option, and this program's Build
  declares no setting 'serve' (it declares run, executable, ...)` (`diagnostics/unknown_compiler_flag`).
- **Two operating systems** (D86): `operating_system` is the system doing the compiling -- the compiler supplies
  it, and giving it is an error -- and `target_operating_system` is the one the program is compiled for, which
  defaults to it. `--target-operating-system=linux` loads `library/linux/` (the launcher,
  [above](#constructors-and-the-entrypoint)) and folds, so `if build.target_operating_system == "windows" { }`
  keeps one branch. Each system's folder reopens `Build` with `var target_operating_system = "linux"`, which is
  only the default a compiler that does not fold `Build` sees -- the seed while bootstrapping. The compiler
  learns which system it runs on from its own `build.target_operating_system`, folded when it was built.
- **`program`** is the folder named on the command line, which the launcher loads; the compiler supplies it and
  giving it is an error.
- **`--final-classes` prints `Build` with its declared defaults**, not the values one build folded, so running the
  printed program takes its flags again instead of, say, printing its classes forever.
- **Run-time cost** (D177): none. `Build` holds nothing at run time -- every field is folded -- so in a
  production build it is one static object with no code behind it, and in an inspectable build an ordinary
  singleton ([Singletons](classes_and_files.md#singletons--implemented)); choosing and writing the outputs is the
  compiler's work, not the program's.

`Build` is the compile-time home that D13's isomorphic split and
[Other environments](targets.md#other-environments--planned)' target field read (the `$target` variable is gone, D76).
