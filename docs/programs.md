# Programs: the entry, the launcher, and settings

A program is a folder. Its **entry file** is the file named after the folder (`game/game.spite`, class `Game`),
and the program runs by constructing that class. There is no `main` and no entry point declaration: the
entry class's constructor *is* the program, and when it returns the program ends with exit code `0`.

```bash
spite game
```

The folder is the only way to name a program: `spite game/game.spite` is an error that says `spite game`. By
default the build writes the executable into `.spite/` in the folder you ran `spite` from,
`.spite/build/game/game.exe`, and runs it; nothing is written beside the program's source
([compiler.md](compiler.md#where-the-outputs-go)).

Every other `.spite` file in the folder is another class of the program, and every folder inside it is a
namespace ([packages.md](packages.md)): `game/engine/renderer.spite` is `Engine.Renderer`, and
`game/engine/physics/physics.spite` is `Engine.Physics.Physics`.

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

`Arguments()` answers the arguments the program was given. When the compiler runs it, those are the settings
its `Environment` declares and every argument after the folder that is not a flag (`spite game ada knight`).
`count()` and `get(index)` read them by position, and `.player_name` reads the value of `--player-name=...` as a
`String?`: `null` when the flag was not given.

```gdscript title=raw_arguments/raw_arguments.spite entry arguments=ada,knight
var console = Console()

func RawArguments() {
    var arguments = Arguments()
    var arguments_count = arguments.count()
    console.print("arguments", arguments_count)
    announce(arguments)
}

func announce(arguments: Arguments) {
    var first = arguments.get(0)
    var second = arguments.get(1)
    console.print("player", first, "class", second)
}
```
```output
arguments 2
player ada class knight
```

That run is `spite raw_arguments ada knight`. A setting the program always wants is better declared in
`Environment`, which gives it a type, a default and a name the compiler checks.

## Run-time settings: `Environment`

A program's settings are the fields of `Environment`, a singleton in the standard library. The program reopens
it with a file of its own named `environment.spite`, holding one `var` per setting with a literal default, and
reads it with `Environment()` anywhere:

```gdscript title=program_settings/environment.spite
var endpoint = "local"
var worker_count = 1
var verbose = false
```
```gdscript title=program_settings/program_settings.spite entry vars=endpoint:production,worker-count:4,verbose:true
var console = Console()
var environment = Environment()

func ProgramSettings() {
    if environment.endpoint == "production" {
        console.print("using the production endpoint")
    } else {
        console.print("using the local endpoint")
    }
    console.print("workers", environment.worker_count, "verbose", environment.verbose)
}
```
```output
using the production endpoint
workers 4 verbose true
```

That run is `spite program_settings --endpoint=production --worker-count=4 --verbose`: a setting is written like
a compiler flag, beside the compiler's own flags, kebab-case on the command line and snake_case in code, and a
`Boolean` may be given bare. Each field is read once, when `Environment()` is first made, from the first of:

1. the program's own command line, `--endpoint=production` (given to the compiler, which passes it on, or to
   the built program run directly);
2. the process environment variable named by the field in upper case, `ENDPOINT`;
3. the declared default.

Only declared fields are read, so nothing the program does not name is ever loaded. The default's literal is the
setting's type: `false` is a `Boolean`, a whole number an `Integer`, `""` a `String`. Anything else, a type
annotation included, is an error naming the three forms. A value that is not of the setting's type
(`--verbose=maybe`) crashes when `Environment()` is first made: the program was started wrong, and there is
nothing sensible to continue with. The values are read when the program runs, so `if environment.verbose` is an
ordinary `if`, with both branches compiled in.

That reading is the whole run-time cost: one pass over the command line and one environment lookup per declared
field, once, when `Environment()` is first made. A program that never makes `Environment()` carries none of it
([rules](../specs/programs.md#program-settings-environment)).

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
```gdscript title=build_settings/build_settings.spite entry build=serve:true vars=name:tester
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

That is `spite build_settings --serve=true --name=tester`. A `--name=value` naming a `Build` field sets that
field while compiling; a field nobody sets keeps the default it was declared with. Either way `build.serve` is
the literal `true` in the built program: `if build.serve { }` is decided while compiling, and the branch it does
not take is never generated. `name` belongs to `Environment`, so the compiler passes `--name=tester` to the
program, which reads it when it runs.

- A `Boolean` field may be given bare: `--optimized` is `--optimized=true`. Any other bare flag is an error naming
  the value it needs.
- The value has to be of the field's type (`--workers=many` for an `Integer` is a compile error, for a `Build`
  field and an `Environment` one alike), a flag naming no field of either is a compile error listing the fields
  `Build` has, and an `Environment` field named like a `Build` field is a compile error naming both, since the two
  share one set of names on the command line. A typo never passes silently.

```gdscript title=unknown_flag_error/unknown_flag_error.spite entry error build=verbos:true
var console = Console()

func UnknownFlagError() {
    console.print("never built")
}
```
```diagnostic
'--verbos' is not a compiler option, and this program declares no setting 'verbos'
```

- A program's own `build.spite` may give a compiler option a different default: `var optimized = true` builds it
  optimized unless `--optimized=false` is given. The compiler reads the whole program before it decides
  anything, so what it produces is a default like the rest: `var build = true` only builds it, and
  `var final_classes = "..."` writes its final classes on every build ([compiler.md](compiler.md#choose-the-outputs)).
  Formatting is not an option: every compile formats first, and `format` is not a field a program may declare.
  `target_operating_system` is the one field only a flag sets ([the rules](../specs/programs.md#build-settings-build)).
- A loaded package may declare `Build` fields too, and the program decides: a field the program's own
  `build.spite` declares keeps the program's default even when a package loaded after it declares the same field.
  An engine's `var plugins_folder = "../../plugins"` is only its default for programs that say nothing. The order
  is a flag, then the program, then the package:

```gdscript title=build_over_package/engine/build.spite
var plugins_folder = "../../plugins"
var engine = "forge"
```
```gdscript title=build_over_package/build.spite
var plugins_folder = "plugins"
```
```gdscript title=build_over_package/build_over_package.spite entry build=engine:ember
var build = Build()
var console = Console()

func BuildOverPackage() {
    load "engine"
    console.print("plugins from", build.plugins_folder)
    console.print("engine", build.engine)
}
```
```output
plugins from plugins
engine ember
```

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
2. the folder of the operating system the program is compiled for (`library/windows/`, `library/linux/` or
   `library/mac/`, each loaded only by name), whose files reopen the classes that system does differently
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
loaded too. What the executable's `main` still does below it (handing over the command line, and the teardown
after `Launcher` returns) is listed in [the rules](../specs/programs.md#constructors-and-the-entrypoint).

## Configuration is a class

There is no configuration file format. A configuration is an ordinary class whose constructor sets its state,
or, for anything that should differ between runs or builds, a field of `Environment` or `Build`, reopened in
the program's own folder. Both are Spite, so a setting can be computed, reflected on, and read in the REPL like
anything else.

---

Next: [Values and types](values_and_types.md), the types, variables and literals a program is built from.
