# Testing

A test is a package that crashes. There is no framework: no `expect`, no matchers, no reporter and no summary.
A test is an ordinary function whose name starts with `test_`, in a class whose name ends with `Tests`, and
`crash` is the assertion -- when a test is wrong, the program halts with the condition, the values of its
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

`tests/tests.spite` finds every test itself, through reflection: `Spite.Class.instances` is every class in the
program, and each class's `.functions` its functions ([reflection.md](reflection.md)). Nothing registers a test,
and there is no manifest to keep in step. A runner of the same shape, with its tests in a class of their own (the
entry class lists no functions, [below](#rules-in-full)):

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

func TestPackage() {
    var classes = Spite.Class.instances
    var index = 0
    while index < classes.count() {
        if classes[index].name.ends_with("Tests") {
            run(classes[index].functions)
        }
        index = index + 1
    }
    console.print("every test passed")
}

func run(functions: List<Spite.Function>) {
    var index = 0
    while index < functions.count() {
        var function = functions[index]
        if function.name.starts_with("test_") {
            function.call_function()
            console.print("passed", function.name)
        }
        index = index + 1
    }
}
```
```output
passed test_trim_removes_the_spaces_around_text
passed test_split_keeps_empty_pieces
every test passed
```

`call_function()` calls a function found through reflection; today it can call one that takes no arguments,
which is what a test is.

The first failing test stops the run, on purpose ([why](#testing--implemented)). A project's tests are a program of their own, beside the
project, that `load`s the code it tests (`load "../game"`); the repository's `tests/` tests the standard library,
which every program loads anyway.

## Tests prove there are no leaks

The repository's own runner calls each test twice and crashes when the second call leaves any allocation behind,
through `program.live_allocations()`, so every test is a leak test as well. `check.sh` runs the package with
`--debug-memory` and requires the whole run to print nothing and balance:

```bash
spite tests --debug-memory
```

Nothing of this is in a program that is not a test: the runner is the test package's own entry class, and the
reflection it reads (`.instances`, `.functions`) is generated only because it reads it (D177).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Testing  **[implemented]**

- **A test is a package that crashes, not a framework** (D46, decided by Mortaro). There is no `expect`, no
  matcher vocabulary, no reporter and no summary. A test is ordinary Spite shipped beside the project as a program
  that `load`s it, and an expectation is `crash <condition>`, which halts on a false condition and reports the
  condition's source text with every operand's value ([What a crash reports](failure.md#what-a-crash-reports)).
  The testing story needs no language feature of its own: `crash`, `load` and reflection are the whole of it.
- **Discovery is reflection.** `Spite.Class.instances` is every class in the program (D49) and a class object's
  `.functions` its functions, so a runner finds each `test_` function of each class named `...Tests` without
  registration. The names are the repository's convention, not a rule of the language: the runner decides them.
  The program's entry class has no stand-in instance, so its `.functions` is empty
  ([reflection.md](reflection.md)): tests go in a class of their own.
- **`call_function()`** calls a function found through reflection, and today only one that takes no arguments:
  for a function with parameters it silently does nothing, so a test takes none. A value a test returns is
  dropped.
- **Fail-fast is deliberate** (D46): the first failing `crash` ends the run. A person wants every failure at once
  to batch the work; an AI wants one precise failure to fix before running again, since a list of mostly
  cascading failures invites shotgun fixes. The crash report is the test output.
- **Every test is a leak test** in the repository's runner: each test runs twice, and the second run must leave
  `program.live_allocations()` where it was; `check.sh` runs `tests/` with `--debug-memory` and requires no output
  and balanced allocations.
- **A test runs on a stand-in at its defaults**: walking `.functions` over `Spite.Class.instances` runs no
  constructor, so a test class's constructor is not a set-up step, and loading another program to test it does
  not run that program ([reflection.md](reflection.md)). A test that needs state gets it from an attribute's default (`var creatures = [Creature("rat", 3, true), ...]`
in `tests/member_tests.spite`) or makes it in its own body.
