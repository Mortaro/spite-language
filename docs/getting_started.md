# Getting started

## Build the compiler

Spite compiles itself, so the only thing you need is a C compiler -- `cc`, `clang` or `gcc`; on Windows, the
clang that ships with Visual Studio is found on its own. From the repository root:

```bash
bin/spite examples/hello
```

`bin/spite` builds the compiler from `bootstrap/seed/spite_compiler.c` into `.spite/` the first time it
runs (and again whenever that file is newer), then compiles and runs the program. The seed is the C the Spite
compiler emits for its own sources, so building it by hand is one command too:

```bash
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite -lm
```

The compiler writes C and builds it with the command in the `CC` environment variable, or the first of `cc`,
`clang` and `gcc` it finds. `bin/spite` picks that command itself -- `SPITE_CC` if it is set, else the first C
compiler it finds, Visual Studio's clang included -- and hands it to the compiler as `CC`. `bash check.sh` proves
the compiler still reproduces itself and still runs every program in `conformance/`, `examples/`, `tests/` and
these pages ([self_hosting.md](../design/self_hosting.md)).

## Hello world

A program is a folder, and its entry file is the file named after the folder. Make `hello_world/hello_world.spite`:

```gdscript title=hello_world/hello_world.spite entry
var console = Console()

func HelloWorld() {
    console.print("Hello, Spite!")
}
```
```output
Hello, Spite!
```

and run it:

```bash
spite hello_world
```

A file is a class, named by its file name: `hello_world.spite` is the class `HelloWorld`. There is no `main`: the
program runs by constructing the entry class, so its constructor -- the function named like the class -- is the
whole program. Building and running are the same command; there is no separate compile step to remember. The
executable is built into `.spite/` in the folder you ran `spite` from, `.spite/build/hello_world/hello_world.exe` (no
`.exe` on Linux and macOS), never beside the source, so it can be run again without the compiler ([compiler.md](compiler.md#choose-the-outputs) has the other outputs). It carries
only what the program uses: the standard library classes hello world never reaches are not in it, and there is
no runtime beneath it ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).

## A second class

Every other `.spite` file in the folder is another class. Classes hold `var` fields (their state, each with a
default) and `func`s (their behaviour); an `enum` declared in a file is namespaced under it (`Person.Job`, though
`'knight'` alone is usually enough). A `List<T>` holds them:

```gdscript title=tour_classes/person.spite
enum Job {
    'knight'
    'mage'
}

var age = 0
var job: Job = 'knight'

func Person(new_age: Integer, new_job: Job) {
    age = new_age
    job = new_job
}
```
```gdscript title=tour_classes/tour_classes.spite entry
var console = Console()

func TourClasses() {
    var party = List<Person>()
    var knight = Person(22, 'knight')
    party.append(knight)
    var mage = Person(19, 'mage')
    party.append(mage)
    party.each(introduce)
    var sum_age = party.sum_age()
    console.print("total age", sum_age)
}

func introduce(person: Person) {
    console.print("member", person.age, person.job)
}
```
```output
member 22 knight
member 19 mage
total age 41
```

Neither walk over the party is a loop anyone wrote. `each(introduce)` calls a function of yours on every member
([collections.md](collections.md#passing-a-function-for-each-element)), and `sum_age()` exists because `Person`
has an `age` ([collections.md](collections.md#member-templates-loops-you-do-not-write)). This is the whole idea
of Spite: reach for a function with the member's name in it before reaching for a loop. Both are written out by
the compiler as plain loops where they are used, so they cost exactly what the loop would.

## What the compiler will do to your file

Every compile formats the program's own files first, and may rewrite them: the compiler is the formatter, there
is one style, and nothing turns it off ([style.md](style.md)). `spite format` formats files without compiling
them ([compiler.md](compiler.md#formatting)). The compiler will also refuse things other languages accept -- an
abbreviated name, a variable nobody reads, a blank line inside a function, a call passed straight into another
call, an object constructed and thrown away -- each with an error that says exactly what to write instead. There
are no warnings: read the error and do what it says; there is no flag to silence it.

## Where to go next

Read [classes_and_files.md](classes_and_files.md), [programs.md](programs.md) and
[values_and_types.md](values_and_types.md) next, then [failure.md](failure.md): together they are most of the
language. Keep [for_ai_writers.md](../design/for_ai_writers.md) at hand while writing -- it is the whole language on one
page -- and [compiler.md](compiler.md) for every command-line option.
