# Testing

A test is a package that crashes. There is no framework: no `expect`, no matchers, no reporter and no summary.
A test is an ordinary function whose name starts with `test_`, in a class whose name ends with `Tests`, and
`crash` is the assertion: when a test is wrong, the program halts with the condition, the values of its
operands and the file and line ([failure.md](failure.md#what-a-crash-reports)), which is everything needed to fix
it. A run that prints nothing passed.

```gdscript
func test_append_and_prepend_keep_order() {
    var numbers = [2, 3]
    numbers.append(4)
    numbers.prepend(1)
    crash numbers.count() == 4
    crash numbers[0] == 1
    crash numbers.last() == 4
}
```

## The runner is thirty lines of Spite

A runner finds every test itself, through reflection: `Spite.Class.instances` is every class in the program, and
each class's `.functions` its functions ([reflection.md](reflection.md)). Nothing registers a test, and there is no
manifest to keep in step. A test package is a folder like any program: its tests in classes of their own (the entry
class lists no functions, [below](#rules-in-full)), and an entry class that runs them:

```gdscript title=test_package/text_tests.spite
func test_trim_removes_the_spaces_around_text() {
    var trimmed = "  spite  ".trim()
    crash trimmed == "spite"
}

func test_split_keeps_empty_pieces() {
    var pieces = "a,,b".split(",")
    crash pieces.count() == 3
}
```
```gdscript title=test_package/test_package.spite entry
var console = Console()
var arguments = Arguments()
var ran = 0

func TestPackage() {
    var classes = Spite.Class.instances
    var index = 0
    while index < classes.count() {
        var tested = classes[index]
        if tested.name.ends_with("Tests") {
            run(tested.name, tested.functions)
        }
        index = index + 1
    }
    crash ran > 0
    console.print("every test passed")
}

func run(class_name: String, functions: List<Spite.Function>) {
    var index = 0
    while index < functions.count() {
        var function = functions[index]
        if function.name.starts_with("test_") and chosen(class_name, function.name) {
            function.call_function()
            ran = ran + 1
            console.print("passed", function.name)
        }
        index = index + 1
    }
}

func chosen(class_name: String, function_name: String): Boolean {
    if arguments.count() == 0 {
        return true
    }
    var wanted = arguments.get(0)
    return wanted == class_name or wanted == function_name
}
```
```output
passed test_trim_removes_the_spaces_around_text
passed test_split_keeps_empty_pieces
every test passed
```

`call_function()` calls a function found through reflection that takes no arguments, which is what a test is. The
runner takes one argument, the name of a class of tests or of one test, and runs only that; a name that matches
nothing is a crash at `crash ran > 0`, never a run that passes because it ran nothing. A project's tests are a
program of their own, beside the project, that `load`s the code it tests (`load "../game"`); the repository's
`tests/` tests the standard library, which every program loads anyway, with the same runner.

## Run them

A test package is run like any program, by naming its folder:

```bash
spite test_package                                    # every test in the folder
spite test_package TextTests                          # one class of tests
spite test_package test_split_keeps_empty_pieces      # one test
spite test_package --debug-memory                     # every test, and prove nothing leaked
```

Each folder of tests is a package of its own, so a project with `game_tests/` and `network_tests/` runs one folder
with `spite game_tests` and all of them by naming each. The compile in front of the run is part of the test: a
test that cannot compile reports `path:line: error: ...` and runs nothing.

**When every test passes**, the run prints what the runner prints (here a line per test and `every test passed`)
and exits with status 0.

**When a test fails**, its `crash` halts the program: everything printed so far is flushed, one `spite.crash` line
and the call chain go to the error stream and the program exits with status 1. Break
`test_split_keeps_empty_pieces` by expecting two pieces, and the run says:

```
passed test_trim_removes_the_spaces_around_text
spite.crash	59bbba06	test_package/text_tests.spite:8	TextTests	test_split_keeps_empty_pieces	pieces.count()=3
spite.frame	test_package/text_tests.spite:6	TextTests	test_split_keeps_empty_pieces
spite.frame	bootstrap/source/generation/prelude.spite:1	Spite.Function	call_function
spite.frame	test_package/test_package.spite:19	TestPackage	run
spite.frame	test_package/test_package.spite:5	TestPackage	TestPackage
spite.frame	launcher/launcher.spite:3	Launcher	Launcher
spite.frame	-	-	main
```

The first line is the report, and it is enough: the file and line of the `crash` (`text_tests.spite:8`), the
class and the test, and the value of every part of the condition, then every other text, number and enum in scope
([what a crash reports](failure.md#what-a-crash-reports)); the `spite.frame` lines below it say how the run got
there. The condition itself is not repeated, since it is on the
line named. There is no expected-versus-actual prose: the test says `pieces.count() == 2`, the report says
`pieces.count()=3`, and the line is open in front of you.

**Iterating on a failure** is one loop. Open the line the report names; read the values; decide whether the code
or the test is wrong; fix it; run that one test again by name (`spite test_package test_split_keeps_empty_pieces`)
until it passes; then run the whole folder, since a fix can break another test. The first failure stops the run,
so each run hands you exactly one thing to fix. To look around inside a test while it runs, run the package with
`--hot-reload --repl-port=4000` and set a breakpoint on the test's line ([repl.md](repl.md#breakpoints)).

## Why tests that crash are the best tests

- **A test stops at the first broken fact.** Nothing runs on top of a value already known to be wrong, so a run is
  as fast as the first failure and a failure never cascades into forty more that only repeat it.
- **There is nothing to write but the fact.** No `expect(...).toEqual(...)`, no matcher vocabulary, no message to
  keep true: `crash pieces.count() == 3` is the test, and the compiler writes the report from it.
- **The report is the memory, not a story about it.** It points at the line and shows the values there. A model
  reads `text_tests.spite:8 ... pieces.count()=3` and opens the line, which is cheaper and more exact than reading a
  paragraph of assertion output, and nothing in it was written by a person who might have been wrong.
- **The same `crash` guards production code.** A `crash` in a test and a `crash` in the program are one statement
  with one report, so every guard the program already has is a test the moment a test reaches it, and a habit
  learned in one place is right in the other.
- **What the compiler can prove is not a test at all.** A null not checked, a `switch` missing a value, a type that
  does not fit, a `crash` whose condition is known while compiling: each is a compile error before any test runs
  ([proofs.md](proofs.md)). Tests are left for what only running can show.

## Tests prove there are no leaks

The repository's own runner calls each test twice and crashes when the second call leaves any allocation behind,
through `program.live_allocations()`, so every test is a leak test as well. The whole run under
`--debug-memory` must print nothing and balance:

```bash
spite tests --debug-memory
```

Nothing of this is in a program that is not a test: the runner is the test package's own entry class, and the
reflection it reads (`.instances`, `.functions`) is generated only because it reads it.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules
disagree, the rules win.

### How testing works

- **A test is a package that crashes, not a framework.** There is no `expect`, no
  matcher vocabulary, no reporter and no summary. A test is ordinary Spite shipped beside the project as a program
  that `load`s it, and an expectation is `crash <condition>`, which halts on a false condition and reports the
  condition's source text with every operand's value ([What a crash reports](failure.md#what-a-crash-reports)).
  The testing story needs no language feature of its own: `crash`, `load` and reflection are the whole of it.
- **Discovery is reflection.** `Spite.Class.instances` is every class in the program and a class object's
  `.functions` its functions, so a runner finds each `test_` function of each class named `...Tests` without
  registration. The names are the repository's convention, not a rule of the language: the runner decides them.
  The program's entry class has no stand-in instance, so its `.functions` is empty
  ([reflection.md](reflection.md)): tests go in a class of their own.
- **A test runs on an instance the runner makes**: the runner calls the walked class (`tested()`, as any walk of
  classes makes an object, [reflection.md](reflection.md)), which runs the test class's constructor, and runs each
  test through `Spite.Call(declaration, instance)` and `call.call()`, the one way a declaration is called. The
  program decides what each test's instance holds and when it is let go, and every call is known while compiling,
  so a test compiles and runs like the rest of the program. A test takes no arguments; one that does is the
  unfilled-argument error of `Spite.Call`. A value a test returns is dropped.
- **Running one test, one class or a folder** is the runner's business, through `Arguments()`: the runner on this
  page and the repository's `tests/` run only the class or test named by the first argument, and crash when it
  names nothing, so a mistyped name never passes. A folder of tests is one program, named on the command line.
- **Fail-fast is deliberate**: the first failing `crash` ends the run. A person wants every failure at once
  to batch the work; an AI wants one precise failure to fix before running again, since a list of mostly
  cascading failures invites shotgun fixes. The crash report is the test output.
- **Every test is a leak test** in the repository's runner: each test runs twice, and the second run must leave
  `program.live_allocations()` where it was, and a run of `tests/` with `--debug-memory` prints nothing and
  balances its allocations.
- **The constructor is the set-up**: the instance is made by its constructor, so a test class sets up what its
  tests share there, or in an attribute's default (`var creatures = [Creature("rat", 3, true), ...]` in
  `tests/member_tests.spite`), and a test that needs its own state makes it in its body. Loading another program to
  test it does not run that program's entry ([reflection.md](reflection.md)).

---

Next: [Optimisations the compiler makes on its own](optimizations.md), what the compiler does to make a program
faster and smaller without being asked.
