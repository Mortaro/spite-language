# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into `manual.md` (prose and decision log) and removes the item. Each item links to where the
manual argues it.

## Naming

1. **The two environments' names** (D85). Built as: `Build` for compile time (`var build = Build()`,
   `build.target_operating_system`, reopened in `build.spite`) and `Environment` for run time. You said "make
   better names, we can just change later".

## Open questions still open in manual.md

2. **Open question 15: whether `while` goes.** Proposal: the index loop over a list (199 of 259 loops) becomes an
   error naming the member template; `while` stays for loops over state.
   D94 review: [mortaros_review_while_and_else_if.md](mortaros_review_while_and_else_if.md). Of 472 loops, 31 are
   replaceable today and the exact-shape rule there catches about 10; most index loops pass the element to a
   function of the caller, which no template expresses.
3. **Open question 20: whether a nested `if`/`else` is an error.** Proposal: an `if` with an `else` inside a
   branch of another `if` with an `else` is an error naming "extract a function".
   D94 review: [mortaros_review_while_and_else_if.md](mortaros_review_while_and_else_if.md), section 3 (16 cases,
   with the `else if` chains and a proposed `switch` over an enum).
4. (Answered: D100, `from_type`.)
5. **Open question 16: two versions of one dependency.** Needs D38 (git dependencies) first.
6. **Open questions 1, 3, 6, 8, 9, 10, 11**, the older ones: `= null` on a generic field, right-to-left casting
   in comparisons, `_` meaning private and unused, an unrelated `get_x()` intercepting `.x`, `${` in text,
   where `--final_classes` shows provenance, and function-valued `type` members.

## Proposals built and waiting for a yes or no

7. **D77, the five interpretations** (manual section 12): the rule covers constructor calls too; any call
   anywhere inside an argument counts; text holes are not arguments; calls moved out of `while` conditions and
   the right of `and`/`or` only where harmless; a hoisted variable may reuse the function's name
   (`var file_stem = file_stem(path)`).
8. **D78's narrow form**: only a call with arguments, in every branch, at the start of the branches, counts.
9. **The floor** (section 15, "The floor, named"): what stays C, and D82's form for showing it in
   `--final_classes`.
10. **Rows marked "(proposed by Claude, unconfirmed)"** in the decision log from 2026-09-23 and 2026-09-24:
    `Memory.address_of`/`compare_bytes`/`take_text`, the tree shaker, `nan` printing as `nan`, the REPL's
    command names and output, `Environment`'s sources and their order, `--operational_system` (superseded by
    D86, and spelled `operating_system` now), the containers row, and the D91/D105 rows (a `List` template's symbol names the element's member;
    how a chain fuses).

## From the remote REPL

11. **D37 drain points, as built.** The remote REPL's commands are answered on the program's thread at its waits
    (manual section 14, "Answered where the program waits"). Confirm the compile error's rule: a `--repl_port`
    build is rejected when none of the program's own code waits and it has a `while` loop -- which also rejects
    a loop that does end.
12. **`Socket` is new public library surface** (`library/socket.spite`), and the REPL's port is fixed at build time.

## Variadic arguments

13. **Whether `Console.print` takes `...values: List<Printable>`** (D90, manual section 5 "Variadic arguments").
    It fits the mechanism, but it needs a `type Printable` that every printable value satisfies, and today the
    generator decides printability itself: numbers, `Bool`, `String`, `Symbol`, enum values, `Spite.Class` and
    `Spite.Namespace` print, and any other class is an error naming its attributes. Proposal (Claude):
    `type Printable { to_text(): String }`, with numbers and `Bool` answering it once D83 makes them classes,
    so a class prints once it declares `to_text()`.
14. **Whether a generic line can name a constraint**, `generic $sub_type: Openable` (open question 12's own
    proposal). D87 decided the lines and not this half.

## From hidden async/await (D99, D103)

15. **The names `Concurrent` and `Parallel`** for D103's split (manual section 15, "Concurrency"):
    `Concurrent(function)` runs on a fiber of the program's thread and is for work that waits, `Parallel(function)`
    runs on a thread of its own and is for work that computes; both answer `wait()` and join when dropped.
16. **The mechanism: stackful fibers plus a helper thread per blocking call**, chosen over a state-machine
    transform and over threads for everything (the decision-log row argues it). Built on Windows; the Linux and
    macOS folders (`makecontext`/`swapcontext`) are only compiled.
17. **What a `Parallel` function may touch.** Nothing is checked yet, and with reference-counted fields a race can
    free a value another thread is reading. Options: D35's syntactic rule (it reaches only its own instance and its
    locals, which rejects `Parallel(file.read)` because `File` reaches `Memory` and its library through fields);
    that rule with singletons allowed; or running a `Parallel` on a deep copy of its instance.
18. **Inferring codegen values from constructor arguments** (`Concurrent(file.read)` without `<String?>`), which
    D35's own example needs and D9 did not foresee.
19. **SPITE.md's line on `async`/`await`** says the handle "joins on first use -- no `.wait()` to remember". As
    built, dropping the handle joins it and `wait()` is how the result is read; there is no implicit wait when the
    value is first used. Say which you want.
20. **A thread pool for `Parallel`**, and whether `parallel_each_` templates (D35 item 3) come before the engine
    needs them.

## Build, the launcher and the entry (D85, D86, D89, D97; manual sections 3, 9 and 13)

14. **The launcher's name and place**: `launcher/launcher.spite`, class `Launcher`, at the repository root beside
    `library/`. It is library code for reflection (`Spite.Class.instances` leaves it out).
15. **Loading the program's folder runs it.** The launcher's `load(Build().program)` constructs the program's
    entry class; every other `load` still compiles to nothing at run time. And a launcher `load` may use `Build`
    fields (`"library/{Build().target_operating_system}"`), where every other `load` takes a literal.
16. **The C left in `main`**: handing `argv` to `Arguments()`, binary standard output on Windows (`_setmode`), and,
    after `Launcher` returns, releasing singletons and class objects and printing the `--debug_memory` report.
    Moving them into Spite needs a way for Spite to receive `argv` and to run code after the program ends (a
    `Launcher` that releases what the program left?) -- which is a language question.
17. **Every compiler option is a `Build` field, and the flags follow the field names**: `--final_classes=folder`
    (no bare form), `--repl_port=4000` (no space form), `--format=false` (no `--no-format`), no `--file=`. A `Bool`
    field may be given bare (`--optimized`), which is a second spelling of `--optimized=true` -- keep it?
18. **`mode` and `format` come from the flag alone**, because the compiler needs them before it reads the program;
    a program's `build.spite` can still declare them, but only its own code sees the value.
19. **A path to a `.spite` file still names an entry** (`spite bootstrap/spite_compiler.spite`), because the
    compiler's own entry is not named after its folder. The alternative is renaming the compiler's entry to
    `bootstrap/bootstrap.spite` (class `Bootstrap`) or moving it into a folder of its own.
20. **An unset `Build` field folds to its default** rather than being read at run time, so no `Build` value is
    ever read when the program runs. D84's words were "the others are runtime"; D85 moved run time to
    `Environment`, which is how this reads it.
