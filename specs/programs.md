# Programs: the entry, the launcher, and settings

The specification of [Programs: the entry, the launcher, and settings](../docs/programs.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Constructors and the entrypoint

There is no `main`: a program is a folder, its entry file is the file named after the folder (`kal/kal.spite`,
class `Kal`), and the program is run by constructing that class; when the constructor returns, the program
ends with exit code `0`. A program is named only by its folder: `error: 'game/game.spite' is a file, and
a program is named by its folder: spite game`, and a folder with no entry file named after it is an error naming
the file looked for. No other file may reopen the entry class
([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)).

The entry constructor takes **no arguments**: `'Kal' is the entry constructor, and it takes no arguments:
a program reads its run-time settings through Environment() (declared in its environment.spite, given on the
command line beside the compiler's flags), what its build decided through Build(), and the raw command line through Arguments(), anywhere`
(`diagnostics/entry_constructor_arguments`).

```gdscript kal.spite
var console = Console()

func Kal() {
    var arguments = Arguments()
    var some_key = arguments.some_key
    if some_key {
        console.print(some_key)
    }
}
```

**`Arguments()` is the raw command line, anywhere**: what
the program was given (when the compiler runs it, the settings its `Environment` declares and every argument
after the folder that is not a flag), with `count()`, `get(index)`, and `.player_name` answering the value of
`--player-name=...` as a `String?`, `null` when that flag was not given; the name is kebab-case on the command
line and snake_case in code, like a setting's (`conformance/stage6/program_arguments`). A bare `--` is an
ordinary argument to it. The compiler answers it in any function; its only cost is the slot
`main` fills with the process's arguments.

**Nothing starts a program behind its back.** Running
one is itself Spite: `launcher/launcher.spite`, at the root of the repository (class `Launcher`), is the class
the executable's `main` constructs, and its constructor, shown in [How a program is loaded](../docs/programs.md#how-a-program-is-loaded),
is the whole of how a program is loaded:

- The compiler reads the launcher first and follows its `load` lines in order, instead of walking the standard
  library by a rule of its own: `library/`, then the folder of the operating system the program is compiled for
  (`library/windows/`, `library/linux/` and `library/mac/` are loaded only by name), then the program's folder,
  `build.program`, the folder named on the command line. Its paths are relative to the repository, which the
  compiler finds beside its own executable.
- A `load` in the launcher may use text and the `Build` fields the compiler knows before reading anything
  (`target_operating_system`, `operating_system`, `program`, and the ones given as flags), read through a `var`
  of the launcher bound to `Build()`. Anywhere else a `load` names its folder with literal text: `a
  program's 'load' names its folder with text, like 'load "engine"': the compiler reads every loaded folder before
  it compiles, so the folder cannot be computed`; it may sit under an `if` on a `Build` field
  ([`load` is a bundle boundary](../docs/packages.md#load-is-a-bundle-boundary)).
- Loading the program's folder is what runs it: that one `load` constructs the program's entry class, runs a
  `--repl` loop on it when asked, and releases it; every other `load` compiles to nothing.
- `main` keeps only the floor: it hands `argv` to `Arguments()`, puts standard output in binary mode on Windows,
  constructs `Launcher`, and after `Launcher` returns releases the singletons and class objects and prints the
  `--debug-memory` balance.
- `Launcher` is library code for reflection (`Spite.Class.instances` does not list it), and `--final-classes`
  prints it with the rest of the program.

**Configuration is a class.** There is no configuration file format: a configuration is an ordinary class whose
constructor sets its state, or a field of `Environment` or `Build`.

## Program settings: `Environment`

`Environment` is a standard library singleton (`library/environment.spite`), and a program
**reopens** it ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)) with a
file of its own named `environment.spite` to declare the settings it has, one `var` per setting. `Environment()`
works anywhere. Only the declared fields are read: nothing the program does not name is loaded, which makes it a
deterministic replacement for packages like Node's dotenv.

What follows is how it is compiled:

- **Where a value comes from.** Each field is read once, when the singleton is first made, from the first of:
  the program's own command line, `--serve=true` or, for a `Boolean`, bare `--serve` (what the program
  receives, see [Command line](compiler.md#command-line)); the process environment variable named by the field
  in upper case, `SERVE`; the declared default. The command line wins because it is the more deliberate of the
  two. An argument that names no field is left alone for `Arguments` to read.
- **A setting is written like a compiler flag**: kebab-case on the command line, snake_case in code,
  `--worker-count=4` for `var worker_count = 1` (`conformance/stage6/kebab_setting`). The compiler refuses the
  snake_case spelling before anything is read (`error: '--worker_count' is written '--worker-count': flags and
  settings are kebab-case on the command line, and the field they set is 'worker_count'`), and so does the built
  program run directly: `--worker_count=4` for a declared `worker_count` stops it as it reads its settings,
  before anything else runs, with `error: '--worker_count' is written '--worker-count': a program's setting is
  kebab-case on the command line, like the compiler's flags, and it sets Environment.worker_count`, exit code 1.
  The check is one pass over the arguments when `Environment` is first made, and only in a program that
  declares a setting.
- **One set of names.** A setting and a compiler flag are written the same way, so an `Environment` field named
  like a field of `Build` (the compiler's options, or one the program's `build.spite` or a package declares) is
  a compile error naming both (`diagnostics/setting_named_like_flag`): rename one.
- **The default's literal is the type.** A setting is declared with nothing but a literal default: `false`/`true`
  is a `Boolean`, a whole number (negative too) an `Integer`, `""` a `String`. A type annotation, or any other
  default, is a compile error naming the three forms: `'Environment.workers' is a setting, read from the
  program's command line (--workers=value) or its environment (WORKERS), so it is declared with only a literal
  default, and the literal is its type: 'var workers = false' (Boolean), 'var workers = 0' (Integer) or 'var
  workers = ""' (String)` (`diagnostics/environment_setting`). Text that is not a value of the setting's type
  (`--serve=maybe`, `--workers=many`, a bare `--name` for a `String`) is a compile error when it is given to the
  compiler (`diagnostics/setting_value`), and **crashes** when the singleton is made in a program run directly
  (a malformed setting is a bug in how the program was started, and there is nothing sensible to continue
  with).
- **How the fields get filled.** The compiler prepends one assignment per declared field to `Environment`'s
  constructor, `serve = boolean_setting("serve", serve)`, where `boolean_setting`, `integer_setting` and
  `text_setting` are ordinary Spite functions in `library/environment.spite`. The reading itself is Spite; the
  compiler only writes the calls, the way it writes a Symbol codegen function
  (`conformance/stage6/environment_settings`).
- **The compiler passes a setting on.** `spite program --serve=true` with `serve` declared in `Environment`
  checks the value while compiling and gives `--serve=true` to the program when it runs it; nothing about the
  setting is written into the build. There is no `--` separator between the compiler's flags and the program's
  (`diagnostics/separator_flag`).
- **Run-time cost**: one pass over the command line and one environment lookup per declared field, once,
  when `Environment()` is first made. A program that never makes `Environment()` has none of its code: the
  class is tree-shaken with the rest of the unused library.
- **A `--hot-reload` build reads the settings again after a reload**: a reload that changes
  `environment.spite` moves `Environment` to its new attributes and reads every setting again, as when the program
  starts, from the command line it was started with, then the environment, then the declared default. To read
  the declared default again, the compiled reading passes the default itself in such a build
  (`serve = boolean_setting("serve", false)`) rather than the field. See
  [repl.md](../docs/repl.md#what-a-reload-can-change).

## Build settings: `Build`

`Environment` (above) is read when the program **runs**; `Build` (`library/build.spite`, a singleton) is decided
when it is **compiled**. Every compiler option is a `Build` field
with a literal default, and a program adds its own the same way it adds `Environment` settings, by reopening
`Build` in a file named `build.spite`. What follows is how it is compiled:

- **Every field is a constant.** A `--name=value` naming a `Build` field sets it; a field nobody
  sets keeps its declared default: the program's own `build.spite` if it reopens it, else a loaded package's,
  else `library/build.spite`.
  Either way the value is written into the program: `build.serve` compiles to `true`, a condition on it is decided
  while compiling, the branch not taken is never generated, and nothing is read when the program runs. The field
  still exists on the `Build` singleton with that value, for reflection.
  **In a `--hot-reload` build**, a field the program or a package declares is read by the program's own code
  from the `Build` singleton while it runs instead, and both branches of a condition on it are compiled, so a
  reload that changes `build.spite` swaps the new value in; the standard library's reads of the compiler's own
  options stay constants ([repl.md](../docs/repl.md#what-a-reload-can-change)).
- **The program decides its own `Build`**: a field the program's own `build.spite` declares keeps the
  program's default even when a loaded package's `build.spite` declares the same field, although the package is
  merged later; this is the one exception to "a later root replaces" ([packages.md](packages.md#packages-namespaces-and-loading)).
  The order is a flag, then the program, then the package, and a flag sets a field only a package declares as it
  sets any other; a field only one side declares is unchanged (`conformance/stage6/build_precedence`). A field
  that decides a `load` is still read before any package is loaded, so it must be the program's or the library's.
- **Flags are kebab-case**: `--repl-port=4000` sets the field `repl_port`, and a flag written with `_` is
  an error before anything is read: `'--repl_port' is written '--repl-port': a flag is kebab-case, and it sets
  the Build field 'repl_port'` (`diagnostics/underscore_flag`).
- **The compiler's options are fields**: `check`, `build`,
  `final_classes` and `optimization_report`, the path `executable_path`, and `optimized`, `development`, `repl`, `repl_port`,
  `hot_reload` and `debug_memory` ([Command line](compiler.md#command-line)). The compiler reads them from the
  program's resolved `Build`, after the whole program is read, so a program whose `build.spite` says
  `var optimized = true` is built optimized unless `--optimized=false` is given.
- **Formatting is not an option**: every compile formats first and nothing turns it off, so `--format`
  and a `format` field in a program's `build.spite` are compile errors (`diagnostics/format_flag`,
  `diagnostics/format_setting`).
- **`target_operating_system` comes from the flag alone**: the launcher needs it to know which library folder is
  part of the program before reading the rest. A `var target_operating_system = ...` (or `operating_system`) in
  a program's `build.spite` naming another system than the one the compile decided is an error naming the flag
  (one naming the same system, as `--final-classes` prints every `Build` field, is accepted): `'Build.target_operating_system' cannot be declared by a
  program: the standard library for the target system is loaded before the program, so the target is given to
  the compiler, --target-operating-system=linux (or windows, or mac), and is the system doing the compiling
  otherwise` (`diagnostics/target_in_build`). The system folders of `library/`
  still set it.
- **A `Boolean` may be given bare**: `--optimized` is `--optimized=true`. Any other bare flag is an error
  naming the value it needs.
- **Nothing passes silently.** A value that is not of the field's type is a compile error: `'--workers=many' was
  given to the compiler, but 'Build.workers' is an Integer: a whole number, like --workers=3`
  (`diagnostics/build_setting_type`); a flag naming an `Environment` field is the error above; a flag naming no
  field is an error listing the fields `Build` has: `'--serve' is not a compiler option, and this program's Build
  declares no setting 'serve' (it declares run, executable, ...)` (`diagnostics/unknown_compiler_flag`).
- **Two operating systems**: `operating_system` is the system doing the compiling (the compiler supplies
  it, and giving it is an error), and `target_operating_system` is the one the program is compiled for, which
  defaults to it. `--target-operating-system=linux` loads `library/linux/` (the launcher,
  [above](#constructors-and-the-entrypoint)) and folds, so `if build.target_operating_system == "windows" { }`
  keeps one branch. Each system's folder reopens `Build` with `var target_operating_system = "linux"`, which is
  only the default a compiler that does not fold `Build` sees (the seed while bootstrapping). The compiler
  learns which system it runs on from its own `build.target_operating_system`, folded when it was built.
- **`program`** is the folder named on the command line, which the launcher loads; the compiler supplies it and
  giving it is an error.
- **`--final-classes` prints `Build` with its declared defaults**, not the values one build folded, so running the
  printed program takes its flags again instead of, say, printing its classes forever.
- **Run-time cost**: none. `Build` holds nothing at run time, since every field is folded, so in a
  production build it is one static object with no code behind it, and in an inspectable build an ordinary
  singleton ([Singletons](classes_and_files.md#singleton-rules)); choosing and writing the outputs is the
  compiler's work, not the program's.

---

Next: [Values and types](values_and_types.md).
