# Getting started

## Build the compiler

Spite compiles itself, so the only thing you need is a C compiler -- `cc`, `clang` or `gcc`; on Windows, the
clang that ships with Visual Studio is found on its own. From the repository root:

```bash
bin/spite examples/hello
```

`bin/spite` builds the compiler from `bootstrap/seed/spite_compiler.c` the first time it runs (and again whenever
that file changes), then compiles and runs the program. The seed is the C the Spite compiler emits for its own
sources, so building it by hand is one command too:

```bash
cc -O2 -Wno-parentheses-equality bootstrap/seed/spite_compiler.c -o spite
```

The compiler writes C and builds it with the compiler named by the `CC` environment variable, or the first of
`cc`, `clang` and `gcc` it finds. `bash check.sh` proves the compiler still reproduces itself and still runs every
program in `conformance/`, `examples/`, `tests/` and these pages ([self_hosting.md](self_hosting.md)).

## Hello world

A program is a folder, and its entry file is the file named after the folder. Make `hello_world/hello_world.spite`:

```spite title=hello_world/hello_world.spite entry
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
whole program. Building and running are the same command; there is no separate compile step to remember.

## A second class

Every other `.spite` file in the folder is another class. Classes hold `var` fields (their state, each with a
default) and `func`s (their behaviour); an `enum` declared in a file is namespaced under it (`Person.Job`, though
`'knight'` alone is usually enough). A `List<T>` holds them:

```spite title=tour_classes/person.spite
enum Job {
    'knight'
    'mage'
}

var age = 0
var job: Job = 'knight'

func Person(new_age: Int, new_job: Job) {
    age = new_age
    job = new_job
}
```
```spite title=tour_classes/tour_classes.spite entry
var console = Console()

func TourClasses() {
    var party = List<Person>()
    party.append(Person(22, 'knight'))
    party.append(Person(19, 'mage'))
    var index = 0
    while index < party.count() {
        console.print("member", party[index].age, party[index].job)
        index = index + 1
    }
    var sum_age = party.sum_age()
    console.print("total age", sum_age)
}
```
```output
member 22 knight
member 19 mage
total age 41
```

That last line is not a loop anyone wrote: `sum_age()` exists because `Person` has an `age`
([collections.md](collections.md#member-templates-loops-you-do-not-write)). This is the whole idea of Spite:
reach for a function with the member's name in it before reaching for a loop.

## What the compiler will do to your file

The first time you run a program, the compiler may rewrite its files: it is the formatter, and there is one
style ([style.md](style.md)). It will also refuse things other languages accept -- an abbreviated name, an unused
variable, a blank line inside a function, a call passed straight into another call -- each with an error that
says exactly what to write instead. Read the error and do what it says; there is no flag to silence it.

## Where to go next

Read [classes_and_files.md](classes_and_files.md), [programs.md](programs.md) and
[values_and_types.md](values_and_types.md) next, then [failure.md](failure.md): together they are most of the
language. Keep [for_ai_writers.md](for_ai_writers.md) at hand while writing -- it is the whole language on one
page -- and [compiler.md](compiler.md) for every command-line option.
