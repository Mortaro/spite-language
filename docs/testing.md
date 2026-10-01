# Testing

A test is a function that crashes when it is wrong. There is no framework: no `expect`, no matchers, no reporter
and no summary. `crash <condition>` is the assertion, the same `crash` that guards the program itself
([failure.md](failure.md)). A run that ends without a crash passed.

## Writing a test

A test is an ordinary function whose name starts with `test_` and that takes no arguments, in a class whose name
ends with `Tests`. In Spite a file is a class, so a test file is a file named `..._tests.spite`. Each fact the test
checks is one `crash` line:

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

Write no message: the report of a failed `crash` already names the line and shows the values
([below](#when-a-test-fails)). A test that needs state makes it in its own body, or reads it from an attribute's
default ([rules](#rules-in-full)).

## The test package

Tests run as a program of their own: a folder beside the project, holding the test files and a runner that
`load`s the code it tests (`load "../game"`). The runner is thirty lines of Spite, and you write it once. It finds
every test itself through reflection: `Spite.Class.instances` is every class in the program, and each class's
`.functions` its functions ([reflection.md](reflection.md)), so nothing registers a test and there is no manifest
to keep in step. This one also takes a word on the command line and runs only the tests whose name holds it:

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
    var arguments = Arguments()
    var only = ""
    if arguments.count() > 0 {
        only = arguments.get(0)
    }
    var classes = Spite.Class.instances
    var index = 0
    while index < classes.count() {
        if classes[index].name.ends_with("Tests") {
            run(classes[index].functions, only)
        }
        index = index + 1
    }
    console.print("every test passed")
}

func run(functions: List<Spite.Function>, only: String) {
    var index = 0
    while index < functions.count() {
        var function = functions[index]
        if function.name.starts_with("test_") and function.name.contains(only) {
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

`call_function()` calls a function found through reflection that takes no arguments, which is what a test is.
The tests go in classes of their own because the entry class has no `.functions` to walk
([rules](#rules-in-full)).

## Running tests

A test package is a program, so `spite` runs it like any other:

```bash
spite test_package            # every test in the package
spite test_package split      # only the tests whose name holds "split"
spite test_package --debug-memory   # every test, and count every allocation and free
```

A folder of tests is a test package: give each area its own (`tests/physics`, `tests/save_files`), each with the
runner, and run one with `spite tests/physics`. The runner above prints a line per test and `every test passed`
at the end, and the program exits with status 0. The repository's own tests are run with `spite tests`.

## When a test fails

The first `crash` that fails ends the run. Change the test above to expect two pieces and run it again:

```gdscript
func test_split_keeps_empty_pieces() {
    var text = "a,,b"
    var pieces = text.split(",")
    crash pieces.count() == 2
}
```
```text
passed test_trim_removes_the_spaces_around_text
spite.crash	59bbba06	.../test_package/text_tests.spite:9	TextTests	test_split_keeps_empty_pieces	pieces.count()=3	text=a,,b
```

The program exits with status 1, and the last line is the report, tab-separated
([What a crash reports](failure.md#what-a-crash-reports)): the file and line of the `crash` (the full path),
the class and the function, so the test that failed, then every part of the condition with the value it had
(`pieces.count()=3`) and the other values in scope (`text=a,,b`). The condition itself is not repeated: it is the
source line the report names. Every `assert` that failed before the crash follows as a `spite.assert` line,
which is the trail of how the program got there.

To fix it: open the line the report names, compare the condition with the values beside it, and decide which is
wrong, the code or the fact the test claims. Fix that, then run only that test (`spite test_package split`) until
it passes, then the whole package once.

## Why a crash is the best test

- **The run stops at the first broken fact.** Nothing after it runs, so a run is as short as it can be and one
  bug never cascades into a page of failures that are only its echoes. There is one thing to fix, then run again.
- **There is no prose to write or read.** No assertion message, no matcher name, no expected-and-actual
  paragraph: the condition is the line of code, and the values are printed beside it.
- **The report is the memory at the failure.** It names the exact line and shows the values there, which is what
  you would otherwise add prints or a debugger for. An AI reads the one line and has the cause, or the next
  question to ask.
- **Tests and code share one mechanism.** The `crash` that guards production code is the test's `crash`, so a
  fact a test checks can move into the code, or out of it, unchanged, and a failure in the code under test reports
  exactly like a failure in the test.
- **What the compiler can prove is not a test.** A read of a value that may be null, an index that may be past
  the end, a wrong type or a value never used is a compile error ([failure.md](failure.md)), so the tests other
  languages write against those do not exist in Spite. A test is left only the facts that need the program to run.

This matters most when an AI writes the code: it runs the tests far more often than a person, and every run ends
in one line that points at the cause, not a list to triage.

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
- **`call_function()`** calls a function found through reflection that takes no arguments, so a test takes
  none. A value a test returns is dropped.
- **Fail-fast is deliberate**: the first failing `crash` ends the run. A person wants every failure at once
  to batch the work; an AI wants one precise failure to fix before running again, since a list of mostly
  cascading failures invites shotgun fixes. The crash report is the test output.
- **Every test is a leak test** in the repository's runner: each test runs twice, and the second run must leave
  `program.live_allocations()` where it was, and a run of `tests/` with `--debug-memory` prints nothing and
  balances its allocations.
- **A test runs on a stand-in at its defaults**: walking `.functions` over `Spite.Class.instances` runs no
  constructor, so a test class's constructor is not a set-up step, and loading another program to test it does
  not run that program ([reflection.md](reflection.md)). A test that needs state gets it from an attribute's default
  (`var creatures = [Creature("rat", 3, true), ...]` in `tests/member_tests.spite`) or makes it in its own body.

---

Next: [Optimisations the compiler makes on its own](optimizations.md), what the compiler does to make a program
faster and smaller without being asked.
