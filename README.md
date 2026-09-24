# Spite

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans: one way to do
each thing, no macros, metaprogramming and a real standard library instead of loops. It compiles to C, and the
compiler is written in Spite and compiles itself. The compiler is also its own formatter and linter -- there is
no separate style guide to follow, it rewrites your file to the one true style and refuses a naming problem
outright instead of silently accepting it.

## Status

`manual.md` is the normative reference, and `docs/` teaches the language as it is today: every titled program
in it is compiled and run by `bash check.sh`.

| Area | Status |
|---|---|
| Files as classes, `singleton` and `generic $name` header lines, enforced file order | implemented |
| Values, numbers as classes (`this`, `from_type` casts), `T?` narrowing, `assert` and `crash` | implemented |
| Functions as values, variadic `...args: List<T>`, operators as functions | implemented |
| Enums, unions, shapes (`type`), `value == Class` tests | implemented |
| Metaprogramming: `Symbol` templates, `Symbol<Class>`, compile-time type tests, fused member-template chains | implemented |
| Reflection (`Spite.Class`, `Spite.Attribute`, `Spite.Function`, `Spite.Namespace`), read-only | implemented |
| Memory: reference counting, `Memory` (heap and stack), `TypedMemory`, tree-shaken output | implemented |
| `Build` (compile time) and `Environment` (run time), the visible launcher | implemented |
| Standard library in Spite: `String`, `List`, `Dictionary`, `Json<T>`, `File`, `Directory`, `Process`, `Program`, `Console`, `Socket` | implemented |
| Concurrency: `Concurrent` (fibers, hidden async IO) and `Parallel` (threads) | implemented on Windows; `Parallel` safety rules open |
| Foreign libraries (`DynamicLibrary`), one folder per operating system | implemented; Linux and macOS folders compile but have never run |
| REPL: `--repl`, `--repl_port`, `spite connect` | implemented; live reload planned |
| Self hosting | done: the compiler is Spite, and the only hand-written C is `bootstrap/source/generation/prelude.spite` |
| Web target, isomorphic classes, live reload | planned -- see manual.md sections 14 and 17 |

Decisions waiting on Mortaro are collected in [`mortaros_missing_decisions.md`](mortaros_missing_decisions.md).

## Build and run

Spite compiles itself. `bootstrap/seed/spite_compiler.c` is the committed fixpoint C, so building the compiler
needs nothing but a C compiler.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
./spite examples/hello
```

Check the compiler: `check.sh` builds the seed, requires generation 2 and generation 3 to be byte identical,
runs every program in `conformance/` requiring exact output and balanced allocations, and reports whether the
committed seed is current (`bash check.sh --update-seed` refreshes it). It uses the first of `cc`, `clang` or
`gcc` it finds, or whatever `CC` names.

```
bash check.sh
```

## Write your own

Build the compiler once, then point it at a file:

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
export CC=cc                     # the compiler shells out to this to build the C it emits
./spite path/to/folder
```

`--mode=c` prints the generated C instead of running it.

On Windows the C compiler usually lives inside Visual Studio rather than on `PATH`, and its path
contains spaces, so use the short form:

```
CL="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang.exe"
"$CL" -O2 -Wno-parentheses-equality -Wno-deprecated-declarations bootstrap/seed/spite_compiler.c -o spite.exe
export CC="$(cygpath -d "$CL") -Wno-deprecated-declarations"
./spite.exe path/to/thing
```

A file is a class named after it, a program is a folder, and the file named after the folder is the entry:
its constructor runs the program. So `greeter/greeter.spite`:

```gdscript
var console = Console()
var names = List<String>()

func Greeter() {
    names.append("ada")
    names.append("grace")
    var people = names.count()
    console.print("greeting", people, "people")
    var everyone = names.join(" and ")
    console.print("hello", everyone)
}
```

```
greeting 2 people
hello ada and grace
```

**Put each program in its own folder.** `spite greeter` loads the whole `greeter` folder (manual.md section 11),
so every `.spite` file in it is part of the same program.

**Save as UTF-8 without a byte order mark.** The lexer rejects a file that starts with one, and PowerShell's
`Set-Content -Encoding utf8` writes one by default: use `-Encoding utf8NoBOM`, or an editor set to UTF-8
without BOM.

**`docs/` is the ground truth for what works today**: every titled program in it, and every program in
`conformance/` and `examples/`, is compiled and checked by `bash check.sh`. `manual.md` also records what is
decided but not built yet, marked as such.

## Where to look

- [`AGENTS.md`](AGENTS.md) -- how to work in this repository: gitmoji commits, how decisions are recorded,
  and the conventions for shared files when more than one agent is running.
- [`SPITE.md`](SPITE.md) -- things that cause Mortaro spite, with what to do instead. Read it before proposing
  a language feature or a way of working; it is the point of the project.
- [`manual.md`](manual.md) -- the normative language reference. When anything else disagrees with it, it wins.
- [`docs/`](docs/README.md) -- the learning path: a five-minute tour, one page per topic, and a dense cheat
  sheet meant to be pasted into an AI's context (`docs/for_ai_writers.md`). Every Spite code block in `docs/`
  is compiled and checked as part of `bash check.sh`.
- [`PLAN.md`](PLAN.md) -- implementation milestones, decisions made where the manual was silent, and what is
  left.
- [`bootstrap/COMPILER_PLAN.md`](bootstrap/COMPILER_PLAN.md) -- the compiler's own plan and progress log:
  what it implements today, and what it does not.
- [`examples/`](examples/) -- idiomatic sample programs the end-to-end test suite also runs.
