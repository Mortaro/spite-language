# Spite

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans: one way to do
each thing, no macros, metaprogramming and a real standard library instead of loops. It compiles to C, and the
compiler is written in Spite and compiles itself. The compiler is also its own formatter and linter -- there is
no separate style guide to follow, it rewrites your file to the one true style and refuses a naming problem
outright instead of silently accepting it.

## Status

`manual.md` is the normative reference; every status tag below is taken from it directly.

| Area | Status |
|---|---|
| Lexer, parser, files-as-classes | implemented |
| Variables, values, numeric types | implemented (exact numeric widths marked PROVISIONAL) |
| Functions, `assert` narrowing, operators-as-functions | implemented |
| Control flow (`while` only, no `for`) | implemented |
| Enums, unions, inline types (duck typing) | implemented |
| Metaprogramming: `Symbol` codegen | implemented |
| Metaprogramming: reflection (`.class`/`.attributes`) | partial (`Class.instances`, D6 class-level functions, D10 symbol literals and D11 two-level reflection planned) |
| Codegen values (`$`), generics, compiler flags, tree shaking | implemented (D5/D9's constructor-declared form -- `func Weapon<$a, $b>(...)`, positional call sites -- is planned; manual.md section 9) |
| Memory (reference counting, `drop()`, `copy()`/`deep_copy()`) | implemented (D1; cycles leak by design, weak references planned) |
| Packages, namespaces, monkey patching, `--final_classes` | partial (bundles always linked statically for now) |
| Style: formatter + linter | implemented |
| REPL (`--repl`, `--repl-port`, `spite connect`) | partial (live reload / in-process codegen is milestone 6b) |
| Standard library (`String`, `List<T>`, `Dictionary<T>`, `File`/`Directory`/`Process`/`Program`) | implemented |
| Foreign libraries (`DynamicLibrary`: C, `.dll`/`.so`/`.dylib`) | planned -- see manual.md section 17 |
| Web target (wasm + generated JS glue, `$target`, D13 isomorphic classes) | planned -- see manual.md section 17 |
| Self hosting (`bootstrap/`) | partial -- front end (lexer/parser/AST) done, semantic analysis + C codegen not started |
| Documentation (`docs/`) | done |
| `cookie_clicker` port | not started -- its original source is not in this repository |

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
./spite path/to/thing
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

A file is a class named after it, and its constructor is the entry point, so `greeter.spite`:

```spite
var console = Console()
var names = List<String>()

func Greeter() {
    names.append("ada")
    names.append("grace")
    var people = names.count()
    console.print("greeting", people, "people")
    say_hello_to_everyone()
}

func say_hello_to_everyone() {
    var index = 0
    while index < names.count() {
        console.print("hello", names[index])
        index = index + 1
    }
}
```

```
greeting 2 people
hello ada
hello grace
```

**Put each program in its own folder.** Running a script loads its parent folder (manual.md section 11), so
every `.spite` file beside it becomes part of the same program -- a scratch directory with several unrelated
programs in it will not compile.

**Save as UTF-8 without a byte order mark.** The lexer rejects a file that starts with one, and PowerShell's
`Set-Content -Encoding utf8` writes one by default: use `-Encoding utf8NoBOM`, or an editor set to UTF-8
without BOM.

**The manual describes more of the language than the compiler implements yet.** `conformance/` is the
ground truth: every program in it passes, so anything used there works today. `examples/` is the same,
at program scale. When something in `manual.md` does not compile, that is the compiler being behind,
not you being wrong -- `bootstrap/COMPILER_PLAN.md` lists what is missing.

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
