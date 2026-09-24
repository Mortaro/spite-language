# Spite documentation

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans. There is one way
to do each thing, no macros, and metaprogramming instead of loops wherever possible. [`manual.md`](../manual.md)
(repository root) is the normative reference -- if anything here disagrees with it, the manual wins. These
pages are the learning path: read them once, then keep `manual.md` and [for_ai_writers.md](for_ai_writers.md)
open while you write.

## Pages

- [getting_started.md](getting_started.md) -- build the compiler and run hello world.
- [compiler.md](compiler.md) -- every compiler option, its behavior, and a command-line example.
- [classes_and_files.md](classes_and_files.md) -- files are classes, constructors, classes are references by default.
- [values_and_types.md](values_and_types.md) -- numeric types and the casting rule, `String`, `T?` and
  `assert` narrowing, enums, unions, inline types and duck typing.
- [functions_and_operators.md](functions_and_operators.md) -- operators as functions, setter/getter interception.
- [control_flow.md](control_flow.md) -- `while` is the only loop, and how to avoid it.
- [metaprogramming.md](metaprogramming.md) -- Symbol codegen, reflection, codegen values (`$`), generics, tree shaking, program settings (`Environment`).
- [memory.md](memory.md) -- the current ownership rules, with do/don't examples.
- [packages.md](packages.md) -- `load`, namespaces, monkey patching, final classes.
- [repl.md](repl.md) -- local and remote REPL, the wire protocol, a worked debugging session.
- [concurrency.md](concurrency.md) -- `Concurrent` and `Parallel`, why there is no `async`/`await`, the waits the
  compiler turns into suspensions, and how the REPL answers at them.
- [standard_library.md](standard_library.md) -- task-oriented: read a file, walk a directory, run a process, group things in a `Dictionary`.
- [json.md](json.md) -- `Json<T>`: any class to JSON text and back, and how it is written with the metaprogramming.
- [for_ai_writers.md](for_ai_writers.md) -- a dense one-page cheat sheet. Paste this into an AI's context.
- [self_hosting.md](self_hosting.md) -- status of the Spite-in-Spite bootstrap compiler.
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) -- places the compiler's real behavior differs from what you might expect.

Every Spite code block on these pages is real: it is extracted, compiled, and run (or checked to fail with the
stated diagnostic) as part of `bash check.sh`. If you see it here, it works.

## Five minute tour

A `.spite` file is a class. `hello.spite` is the class `Hello`; there is no `main`, a program runs by
constructing the entry file's class.

```spite title=tour_hello/tour_hello.spite entry
var console = Console()

func TourHello() {
    console.print("Hello, Spite!")
}
```
```output
Hello, Spite!
```

Every other `.spite` file next to the entry file is another class. Classes hold `var` fields (their state) and
`func`s (their behavior); an `enum` declared in a file is namespaced under it (`Person.Job`, though `'knight'`
alone is usually enough). Build a `List<T>` of them and iterate with `while` -- there is no `for`:

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

That last line is metaprogramming, not a loop: `sum_age()` is generated because `Person` has an `age` field
(see [metaprogramming.md](metaprogramming.md)). This is the whole idea of Spite: reach for a standard library
method with the field's name baked into it before reaching for a loop.

What's next: [getting_started.md](getting_started.md) to build the compiler and run this yourself, then
[values_and_types.md](values_and_types.md) for the type system's sharp edges, then
[for_ai_writers.md](for_ai_writers.md) to keep at hand while writing.
