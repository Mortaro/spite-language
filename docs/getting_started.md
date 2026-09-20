# Getting started

## Build the compiler

Spite compiles itself, so the only thing you need is a C compiler. From the repository root:

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
```

That builds `spite` from `bootstrap/seed/spite_compiler.c`, the committed C that the Spite compiler emits for
itself. Compiling and running a program is one command:

```
./spite --file=path/to/file.spite --mode=run
```

`bash check.sh` from the repository root proves the compiler still reproduces itself and still passes every
program in `conformance/`.

## Hello world

```spite title=hello_world/hello_world.spite entry
var console = Console()

func HelloWorld() {
    console.print("Hello, Spite!")
}
```
```output
Hello, Spite!
```

Run it:

```
./spite --file=hello_world.spite --mode=run
```

Building and running are the same command -- there is no separate "compile" step to remember. The file's class
name (`HelloWorld`, from `hello_world.spite` PascalCased) is constructed and that's the whole program: no
`main`, no entry point declaration.

## The command line

The complete, current command-line reference is [compiler.md](compiler.md). The shortest useful forms are:

```bash
spite file.spite                              # build and run
spite file.spite --optimized                  # build and run with C optimization
spite file.spite --mode=c > file.c            # print generated C
spite file.spite --mode=build --output=file.exe # build without running
spite file.spite --environment=server         # supply $environment at compile time
```

A `--name=value` flag that matches no `$name` used anywhere in the program is a compile error -- there is no
silent typo. Planned flags such as `--development`, `--repl`, and formatting commands are not available yet.

## Automatic formatting and lints

**The compiler is the formatter.** Every `spite file.spite` run first rewrites every `.spite` file belonging to
the program (the entry folder's own files, plus every `load(...)`-ed root, recursively) to the one true style,
printing `formatted <path>` to stderr for each file it touched, before compiling the up-to-date text. Style is
not configurable: 4-space indentation, K&R braces, one blank line between file-level declarations, minimum
necessary parentheses, list/call bodies broken past 120 columns. `--no-format` skips this (useful for a script
that asserts diagnostic positions against a fixture kept deliberately unformatted). Run it standalone with
`spite format <path>` (rewrites) or `spite format --check <path>` (lists what would change, changes nothing,
exits 1 if the list is non-empty) -- this is the check every sample on these documentation pages passes.

What the formatter **will** rewrite: indentation, brace placement, blank lines, spacing around operators,
unnecessary parentheses, and whether a list/call is one line or many. It never reorders code or drops a
comment -- if it cannot prove a rewrite is lossless, it leaves the file untouched and reports an internal
formatter error instead of guessing.

What the compiler **refuses** to auto-fix: naming. Renaming a symbol can change what a program means to
someone who searches for its old name, so these are compile errors instead, each with a suggested fix:

- Non-`snake_case` variables/attributes/functions/parameters, non-`PascalCase` classes/types/enums/unions,
  non-`snake_case` enum values and file/folder names.
- Single-letter names, always -- no exceptions.
- Abbreviations, checked word-by-word against a denylist (`msg` -> `message`, `cfg` -> `configuration`, and
  around fifty more -- see manual.md section 12 for the full table). `id` is explicitly allowed.

```spite title=lint_error/lint_error.spite entry error
var console = Console()
var msg = ""

func LintError() {
    console.print(msg)
}
```
```diagnostic
'msg' abbreviates: write 'message' instead of 'msg'
```

```spite title=single_letter/single_letter.spite entry error
var console = Console()
var x = 0

func SingleLetter() {
    console.print(x)
}
```
```diagnostic
is a single letter: give it a name that says what it holds
```

There is no `--no-lint` -- manual.md section 12 puts it plainly: "the compiler already **is** the linter." Fix
the name; there is no flag to silence it.
