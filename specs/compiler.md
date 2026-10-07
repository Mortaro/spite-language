# Compiler command line

The specification of [Compiler command line](../docs/compiler.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Command line

Every command and flag the compiler has. `program` is a folder; every flag is a field of `Build`
([Build options](../docs/compiler.md#build-options), [programs.md](programs.md#build-settings-build)) or of the program's
`Environment` ([programs.md](programs.md#program-settings-environment)), which the program reads when it runs.

```
spite program                           build .spite/build/program/program.exe and run it
spite program --check                   format the program and compile it, writing nothing but its formatting
spite program --build                   build the executable without running it
spite program --executable-path=path    put the executable at path (not with --check)
spite program --final-classes=folder    also write the final classes into folder (inspect merged classes)
spite program --optimization-report=file   also write what could not be optimised, and why, into file
spite program --optimized               a release build: -O3, and link-time optimisation across translation units
spite program --development             an inspectable build: no tree shaking, internals as ordinary objects
spite program --debug-memory            count allocations and frees, print the balance when the program ends
spite program --trace-asserts           print each failed assert as it fails, as well as in a crash's report
spite program --repl                    run it, then answer REPL commands at the terminal (repl.md)
spite program --repl-port=4000          serve REPL commands on 127.0.0.1:4000 while it runs (repl.md)
spite program --hot-reload              swap changed classes into the running program (implies --development)
spite program --target-operating-system=linux   compile for another system: windows, linux or mac
spite program --serve=true              set a Build field the program declares in its build.spite (programs.md)
spite program ada --player-name=x       run it, passing a setting its Environment declares, and any argument
                                        that is not a flag, to the program (Environment, Arguments)
spite connect port                      talk to a program running with --repl-port=port (repl.md)
spite connect port --command="..."      send one REPL command, print its raw JSON answer line, and exit
spite reload program ... --executable-path=running   what a --hot-reload program runs to rebuild itself (repl.md)
```

A `Boolean` flag may be given bare: `--optimized` is `--optimized=true`, and `--optimized=false` stays allowed.
Flags mix freely, and every output that is on comes from the same compile: an `--optimized` build may keep
the REPL (it is then inspectable), and `--build --final-classes=folder` builds the executable and writes the
classes. Building and running are the same command. There is no `--mode`, `--output` or `--format`: `--check` and
`--build` stop the build sooner, `--final-classes` adds its folder, formatting is not an option, and `spite connect`
and `spite reload` are commands of the compiler, not options. There is no `spite format`: every compile formats.

## Naming a program

A program is named only by its folder, and its entry file is the one named after the folder. The
compiler's own entry is `bootstrap/bootstrap.spite`, class `Bootstrap`, so it compiles itself as `spite bootstrap`.
With no folder named, the compiler prints its usage to the error output and exits 1.

- A `.spite` path: `error: 'game/game.spite' is a file, and a program is named by its folder: spite game`.
- A folder without its entry: `error: 'game' holds no program: a program is a folder holding an entry file named
  after it, 'game/game.spite'`.
- An entry constructor with parameters is an error at its line ([programs.md](programs.md#constructors-and-the-entrypoint);
  `diagnostics/entry_constructor_arguments`).

## Outputs

**The whole program first, then the outputs.** The
compiler reads the launcher, the library, the program's folder, its `build.spite` and every `load` before it reads
any option, and then decides what to produce from the program's resolved `Build`, so a program's own `build.spite`
can set every option. The one exception is `target_operating_system`, which the launcher needs to know which
folder of `library/` belongs to the program, so it is read from the flag alone: a `var target_operating_system`
in a program's `build.spite` is an error naming the flag ([programs.md](programs.md#build-settings-build)).
A build makes the executable and runs it. Two `Boolean` fields stop it sooner: `check` (default `false`: compile the
whole program, report its errors and write nothing, neither C nor an executable) and `build` (default `false`:
make the executable and run nothing). Both on is `error: '--check' only checks that the program compiles and
'--build' builds its executable: give one of them`. `final_classes` is the folder to write the program back out to
(`""` is off), beside whatever the build does, since any folder inside the program would be read back as part of
it. Tree shaking and every other whole-program step run before any output is written.
- **No stale executable**: `--check` never writes, moves or removes an executable. Every other build removes
  the executable at its path before compiling, so a build that fails leaves none there, never the one an earlier
  build made; when the program cannot be read, its `Build` is unknown, and the path is the one the command line
  gives or the default. An executable that cannot be removed (still running, on Windows) stops the build with
  `error: '<path>' is left from an earlier build and could not be removed (is it still running?): a build removes
  it first, so a failed build never leaves the old one to run` and exit status 1.
- **The flags `--check` and `--build` replaced** are errors that name them, when the program declares no setting of
  that name: `--run` is `error: '--run' is not a compiler option: 'spite program' builds the program and runs it,
  'spite program --check' only checks that it compiles, and 'spite program --build' builds it without running it`
  (`diagnostics/removed_run_flag`), and `--executable` is `error: '--executable' is not a compiler option: 'spite
  program --build' builds the executable without running it, and 'spite program' builds it and runs it`
  (`diagnostics/removed_executable_flag`).

- **Where outputs go**: `executable_path`, `""` by default, which means the
  working folder's `.spite/`, never beside the program's source, because the compiler should not
  write intermediate files beside the source. The layout: a program folder
  inside the working folder is built into `.spite/build/<its path from the working folder>/`, so `spite game` writes
  `.spite/build/game/game.exe` (`game` when the target is Linux or macOS), and the
  program folder that is the working folder itself is `.spite/build/<name>/`; any other is built into
  `.spite/elsewhere/<name>_<number>/`, the number `(n * 31 + code) mod 1 000 000 007` over the codes of the program
  folder's absolute path from 7. The default paths are relative to the working folder, which is where the program
  runs. A folder a path names is created. `--executable-path` given on the command line with `--check` is
  `error: '--executable-path' says where the executable goes, and '--check' builds none: leave out --check to build
  it`. What describes an executable stays beside it wherever
  it goes: the `.crashes` map ([Failure: three outcomes and no others](failure.md#failure-three-outcomes-and-no-others))
  and a `--hot-reload` build's `.reload_host`, `.reload_files` and `_reload_<n>` libraries. The one intermediate
  is the C the C compiler reads, written to the language repository's
  `.spite/<executable name>_<number>.c`, the number `(n * 31 + code) mod 1 000 000 007` over the codes of
  the executable's whole path from 7 (so two builds of programs named
  alike at once never share a `<name>.c` and never compile each other's program), or, for a build from several
  translation units, its units, header and objects, written to `.spite/objects` under the hash of their
  content (the rules below).
- **A build that ends with no executable is an error**: when the C compiler (or the link of the units)
  reports success and the executable is not at its path, the build stops with `error: the C compiler reported
  success but '<path>' is not there: nothing was built, so nothing runs (another build writing the same
  executable, or a virus scanner, may have removed it)` and exit status 1, instead of a status of 0 with nothing to
  run.
- **`.spite/` is never part of a program**: walking a program's folder or a
  loaded root, the compiler skips every folder named `.spite`, `.spite-cache` or `.git`, so
  running `spite .` inside a program, whose outputs and checkouts land in its own `.spite/`, reads none of them
  back.
- **Final classes** are described in full [on the page](../docs/compiler.md#inspect-merged-classes): a program, not a report.
- **The optimisation report** (`optimization_report`, `""` is off) is one Markdown file at the path given, its
  folder created, written after the whole program is compiled and before the executable is built, with `--check`
  too. It is written only when asked: without the flag the compiler records nothing and the program built is the
  same. It starts with `# Optimisation report` and a sentence naming the program, then has one `## <section>
  (<count>)` per optimisation, in this order: `Lists that hold references`, `Lists not in the frame`, `Objects not
  in the frame`, `Copies not elided`; a section with nothing in it says `None.`. Each entry is one line,
  `- [<path>:<line>](<path from the report's folder>#L<line>): <what> ...: <why>`, the path as error messages
  print it; entries are sorted by path and line, a place reported twice (a generic function compiled per class) is
  listed once, and a place only in functions tree shaking removed is not listed. A list type is listed once, at the
  line that first needed it. An inspectable build adds a sentence saying that nothing is placed in the frame. A
  report that cannot be written is `error: could not write '<path>'` and exit status 1.
- **The executable is built from translation units.** The compiler chooses the number, and no option sets it: the
  largest power of two that is at most the C's size divided by 768 KiB, at most the
  processors (`NUMBER_OF_PROCESSORS` on Windows, `getconf _NPROCESSORS_ONLN` elsewhere, else 4) and at most 64. One unit is the one C file as before.
  A `--hot-reload` build and C holding `#include "` (a foreign library's header) are one file whatever the number.
  The split keeps what the C means: every preprocessor line, type and prototype goes into the header in its order;
  a function at file level loses `static` and goes to the unit `FNV-1a(name) mod units` picks, its prototype into
  the header where it stood; a `static inline` function stays in the header; a variable at file level loses `static`,
  is defined in unit 0 and declared `extern` in the header where it stood; a function or variable inside a
  preprocessor condition goes to unit 0 inside the same conditions. Each unit, the header and the list of objects
  are named by the FNV-1a hash of their text (a unit's hash covers the C compiler's command and flags too), written
  once, and compiled only when their object is missing; the C compiler runs once per missing unit, as many at once
  as there are processors, then links the objects (`@` a file listing them).
- **The object cache cleans itself.** Each build marks every header, unit and list of objects it uses as used
  now, and after linking, when everything in `.spite/objects` passes 1 GiB, removes whole entries (a unit's C and
  object together), the least recently used first, until it is back under 1 GiB, never one the build itself used.
  The cap is the compiler's choice, not a setting: about thirty builds of the compiler itself, the largest
  program it caches. Nothing in the cache needs deleting by hand.
- **`optimized` is `-O3`, and link-time optimisation across units.** A build from several units also passes `-flto=thin` (with `-fuse-ld=lld` except on
  macOS) when `CC --version` names clang, `-flto=auto` when it names gcc, and nothing for another compiler.
- **A build that is not `--optimized` is tuned for the machine that builds it**: compiling and linking a default
  or `--hot-reload` build, and every reload library, pass `-mcpu=native` when `CC -dumpmachine` starts with
  `aarch64` or `arm`, else `-march=native`. An `--optimized` build passes neither, so it runs on any processor of
  its system. Neither is a setting: `--tune-for-this-machine` is `error: '--tune-for-this-machine' is not a
  compiler option: a default or --hot-reload build is tuned for the machine that builds it, and an --optimized
  build, the one shipped, runs on any machine`, and `--translation-units` is `error: '--translation-units' is not
  a compiler option: the compiler chooses how many C files it compiles a program from`
  (`diagnostics/removed_tuning_flag`, `diagnostics/removed_translation_units_flag`).
- The C compiler is the command in `CC` (it may carry arguments), or else the first of `cc`, `clang` and `gcc`
  that runs; none found is `error: no C compiler found. Set the CC environment variable to the command that
  compiles C (for example CC=clang), or install cc, clang or gcc`. `bin/spite` keeps a `CC` already set, and
  otherwise exports it from `SPITE_CC` or the first compiler it finds; `SPITE_CC` wins over `CC`.

## Flags and settings

- **Flags are kebab-case, fields stay snake_case**: `--repl-port=4000` sets
  `Build.repl_port`, `--hot-reload` sets `hot_reload`, and a program's own field `worker_stack_size` is
  `--worker-stack-size=256` (`conformance/stage6/build_settings`). An underscore in a flag's name is an error,
  checked before anything is read, naming the hyphen form: `error: '--repl_port' is written '--repl-port': a flag
  is kebab-case, and it sets the Build field 'repl_port'` (`diagnostics/underscore_flag`); the value after `=` is
  never touched. The rule covers the compiler's flags and the program's settings alike (`--player-name=x` sets
  `Environment.player_name`), and every message that names a flag names the kebab form
  (`diagnostics/underscore_setting`). A program run directly, without the compiler, reads the same kebab spelling
  and refuses the snake_case one ([programs.md](programs.md#program-settings-environment)).
- **One set of names**: an `Environment` field named like a field of `Build` is an error naming both, at the
  setting: `environment.spite:2: error: 'Environment.optimized' and 'Build.optimized' are both '--optimized' on
  the command line: a program's settings and the compiler's flags share one set of names, so one of them needs
  another name` (`diagnostics/setting_named_like_flag`).
- A `--name=value` naming a `Build` field sets it, and is written into the build as a constant. One naming a
  field of `Environment` is checked against the setting's type while compiling (`error: '--volume=loud' was
  given, but 'Environment.volume' is an Integer: a whole number, like --volume=3`, `diagnostics/setting_value`;
  a bare flag only for a `Boolean`) and passed to the program. One that names no field is an error that shows
  both places a setting can belong (`diagnostics/unknown_compiler_flag`): `error: '--serve' is not a compiler
  option, and this program declares no setting 'serve' (its Build declares check, build, ...). To decide it
  when compiling, reopen Build in the program's build.spite with 'var serve = ...'; to read it when the program
  runs, declare it in environment.spite with 'var serve = ...'`.
- A value that does not fit the field is an error naming the flag and the forms it takes, and no file position,
  since the mistake is on the command line: `error: '--optimized=maybe' was given to the compiler, but
  'Build.optimized' is a Boolean: --optimized, --optimized=true or --optimized=false` (`diagnostics/flag_value`).
  `--repl-port` takes 1 to 65535: `error: --repl-port takes a port number from 1 to 65535: spite program
  --repl-port=4000`. `--target-operating-system` takes `windows`, `linux` or `mac`: `error:
  '--target-operating-system=beos' names no operating system the library has a folder for: windows, linux or mac`.
- `--operating-system` and `--program` cannot be given: `error: '--operating-system' cannot be given:
  Build.operating_system is the system doing the compiling (windows). To write the program for another system, give
  --target-operating-system=linux (or windows, or mac)`, and `error: '--program' cannot be given: the program is
  the folder named on the command line, 'spite folder', and Build.program is set from it`.
- **What the program receives**: every setting its `Environment` declares, and every argument after the folder
  that is not a flag, in the order given, verbatim (`arguments.get(0)` is the first, `arguments.player` reads
  `--player=...`, and `Environment` reads the settings it declares); the compiler's own flags and the folder are
  not passed (`conformance/stage6/program_arguments`). **There is no `--` separator**: a bare `--` is an error,
  `error: '--' is not a separator: a program's settings are given beside the compiler's flags, kebab-case like
  them: spite program --optimized --player-name=Bob` (`diagnostics/separator_flag`). A flag or setting given twice is an
  error too, `error: '--optimized' is given twice: a flag or a setting is given once`
  (`diagnostics/flag_given_twice`), so a later one never silently replaces an earlier one.
- Every setting is decided while compiling and costs nothing at run time; only the builds that ask for it
  carry `--debug-memory`'s table, the REPL (`--repl`, `--repl-port`) or live reload (`--hot-reload`).

## Inspectable and production builds

A build with `--development`,
`--hot-reload`, `--repl` or `--repl-port` is *inspectable*: nothing is tree-shaken, and the singletons that hold
nothing (`Build`, `TypedMemory<T>`, [Singletons](classes_and_files.md#singleton-rules)) are ordinary
objects (allocated at first use, listed by `.instances`, destroyed at exit), so the REPL and reflection see the
internals as normal classes; since nothing is shaken, every `DynamicLibrary` looks up every symbol it declares.
Every other build, the ordinary one included and not only `--optimized`, is a *production* build, where those
singletons are static objects and everything unused is tree-shaken, down to the classes nothing reachable uses.
`--optimized` with a REPL flag is inspectable. The optimisations that change only speed (chains as one loop,
placement, text appended in place) apply in both; [optimizations.md](../docs/optimizations.md) says which build each
optimisation applies in (`conformance/stage6/development_internals`).

## What the C compiler says

The compiler reads the C compiler's messages with its output. When the C compiler fails, the compiler prints
them and stops; when it succeeds, they are dropped, warnings included, so a program's output is its own.

## Where a program runs

A program runs in the folder `spite` was run from. The compiler finds `launcher/` and
`library/` from its own executable (the first folder above it that holds `launcher/launcher.spite`) and never
from the working directory, so a relative path a program opens (`File`, `Directory`, a cache folder) is the
caller's. The launcher's `load` paths are relative to that folder. The outputs with no path go into that folder's
`.spite/`, so running a program adds `.spite/` to the caller's folder and nothing else, and nothing beside
the program's source. A program built with `--repl`, `--repl-port` or
`--hot-reload` runs attached to the terminal; any other run's output is printed when it ends, and the compiler
exits with the program's exit code.

## The launcher passes the program's arguments untouched

`bin/spite` is a bash
script, and `bin/spite.cmd` runs it with the bash on the `PATH`, which on Windows is Git for Windows' bash. That
bash rewrites every argument that looks like a POSIX path when it starts a Windows program, so
`spite tool --prefixes=/Game/Legacy/` would reach the program as `--prefixes=C:/Program Files/Git/Game/Legacy/`,
from PowerShell as well, and `MSYS_NO_PATHCONV=1` would break the launcher's own paths instead. So the launcher writes
its own paths as Windows paths itself (`cygpath -m`: the folder, which is the first argument that is not a flag,
`--executable-path` and `--final-classes`) and turns the rewriting off for the compiler it starts
(`MSYS2_ARG_CONV_EXCL="*"`), so every other argument, a program's settings and arguments included, and every
argument of `spite connect`, arrives exactly as typed. A path given to a field a program's own `build.spite`
declares is written as the system writes it (`--assets=C:/game/assets`). On Linux and macOS nothing is rewritten and nothing
changes.

## How many errors are listed

Every error is `<path>:<line>: error:
<message> (in <Class>.<function>)`, each listed once. The errors in files under the program's own folder come first,
at most 25, followed by `and N more in the program` when there were more. Then come the errors in the packages it
loads (the standard library, the launcher and every loaded folder), at most 25 of them together, in the order they
were found, followed by one `and N more in <package>` per package that had more, the package named by its folder, relative to the folder `spite` was run from when it is
inside it (`library` and `launcher` for the language's own). A package whose errors fill the list therefore never
hides one in the program.

## Formatting before compiling

- **Every compile formats first, and nothing turns it off.** After the whole program is read, every `.spite` file of the program's own (its folder
  with every folder below it, and every `load`-ed root, never `library/`) whose formatted text differs from what
  is on disk is rewritten, printing `formatted <path>` to the error output, and the program is read again from
  disk before anything else is produced, so what is compiled, and every line an error names, is the formatted
  file. A file that does not lex or parse is reported by reading the program, before the formatter sees it. A file
  the formatter's own safety check refuses ([Style](style.md#style)) stops the compile with exit
  status 1: `<path>: error: the formatter refuses it, and a program compiles only once formatted: <reason>`.
- There is no switch: `--format` (with any value) is `error: '--format' is not a compiler option: every compile
  formats the program's own files first, and nothing turns that off: '--check' formats and checks a program
  without building it` (`diagnostics/format_flag`), and a `var format` in a program's `build.spite` is
  `<path>:<line>: error: 'Build.format' cannot be declared: ...` with the same advice (`diagnostics/format_setting`).
- **There is no command that only formats.** `spite game --check` formats the program and checks that it compiles,
  building nothing: it is what a linter or a language server runs. No mode checks the formatting without writing
  it, and `format` is not a command: `spite format` names a program folder called `format` like any other word.

---

Next: [REPL and live reload](repl.md).
