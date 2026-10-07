# Testing

The specification of [Testing](../docs/testing.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## How testing works

- **A test is a package that crashes, not a framework.** There is no `expect`, no
  matcher vocabulary, no reporter and no summary. A test is ordinary Spite shipped beside the project as a program
  that `load`s it, and an expectation is `crash <condition>`, which halts on a false condition and reports the
  condition's source text with every operand's value ([What a crash reports](../docs/failure.md#what-a-crash-reports)).
  The testing story needs no language feature of its own: `crash`, `load` and reflection are the whole of it.
- **Discovery is reflection.** `Spite.Class.instances` is every class in the program and a class object's
  `.functions` its functions, so a runner finds each `test_` function of each class named `...Tests` without
  registration. The names are the repository's convention, not a rule of the language: the runner decides them.
  The program's entry class has no stand-in instance, so its `.functions` is empty
  ([reflection.md](../docs/reflection.md)): tests go in a class of their own.
- **A test runs on an instance the runner makes**: the runner calls the walked class (`tested()`, as any walk of
  classes makes an object, [reflection.md](../docs/reflection.md)), which runs the test class's constructor, and runs each
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
  test it does not run that program's entry ([reflection.md](../docs/reflection.md)).

---

Back to [the specification](README.md).
