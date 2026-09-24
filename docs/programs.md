# Programs: the entry, the launcher, and settings

A program is a folder. Its **entry file** is the file named after the folder -- `game/game.spite`, class `Game`
-- and the program runs by constructing that class. There is no `main` and no entry point declaration: the
entry class's constructor *is* the program, and when it returns the program ends with exit code `0`.

```bash
spite game
```

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
  optimized unless `--optimized=false` is given. `mode` and `format` are the exceptions: the compiler needs them
  before it has read the program, so only the flag changes them.

### The operating system: `operating_system` and `target_operating_system`

`build.operating_system` is the system doing the compiling, and `build.target_operating_system` the one the
program is compiled for: `"windows"`, `"linux"` or `"mac"`. The target defaults to the compiling system, and
`--target_operating_system=linux` compiles for Linux from any machine. Both are constants, so a condition on
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
`launcher/launcher.spite` at the root of the repository, and its constructor is the whole story:

```gdscript
var build = Build()

func Launcher() {
    load("library")
    load("library/{build.target_operating_system}")
    load(build.program)
}
```

The compiler reads that file first and follows its `load` calls in order:

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

`--final_classes` writes `Launcher` out with the rest of the program, so the printed program shows how it is
loaded too. `Launcher` counts as library code: `Spite.Class.instances` does not list it. What `main` still does
in C is the floor under this: it hands the command line to `Arguments()`, puts standard output in binary mode on
Windows, and, after `Launcher` returns, releases the singletons and the class objects and prints the
`--debug_memory` balance.

## Configuration is a class

There is no configuration file format. A configuration is an ordinary class whose constructor sets its state,
or -- for anything that should differ between runs or builds -- a field of `Environment` or `Build`, reopened in
the program's own folder. Both are Spite, so a setting can be computed, reflected on, and read in the REPL like
anything else.
