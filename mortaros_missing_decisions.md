# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into `manual.md` (prose and decision log) and removes the item. Each item links to where the
manual argues it.

## Naming

1. **The two environments' names** (D85). Proposed: `Build` for compile time (`var build = Build()`,
   `build.target_operating_system`) and `Environment` for run time. You said "make better names, we can just
   change later".

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
   where `--final-classes` shows provenance, and function-valued `type` members.

## Proposals built and waiting for a yes or no

7. **D77, the five interpretations** (manual section 12): the rule covers constructor calls too; any call
   anywhere inside an argument counts; text holes are not arguments; calls moved out of `while` conditions and
   the right of `and`/`or` only where harmless; a hoisted variable may reuse the function's name
   (`var file_stem = file_stem(path)`).
8. **D78's narrow form**: only a call with arguments, in every branch, at the start of the branches, counts.
9. **The floor** (section 15, "The floor, named"): what stays C, and D82's form for showing it in
   `--final-classes`.
10. **Rows marked "(proposed by Claude, unconfirmed)"** in the decision log from 2026-09-23 and 2026-09-24:
    `Memory.address_of`/`compare_bytes`/`take_text`, the tree shaker, `nan` printing as `nan`, the REPL's
    command names and output, `Environment`'s sources and their order, `--operational_system` (superseded by
    D86), and the containers row.

## From the remote REPL

11. **D37 drain points, as built.** The remote REPL's commands are answered on the program's thread at its waits
    (manual section 14, "Answered where the program waits"). Confirm the compile error's rule: a `--repl-port`
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

## From D93 and D95 (directories and JSON)

21. **`Directory.Entry`** is the name of the union of `Directory` and `File` that `entries()` answers; folders
    come first, then files, each sorted. Should `files()` and `folders()` go now that `entries()` exists (the
    compiler's own discovery still reads names)? Manual section 15, "System classes".
22. **`Json<T>`'s API**: `Json<Order>().write(order)`, `.read(text)` (`Order?`) and `.read_or_crash(text)`. This
    settles D22's open naming pair as `read`/`read_or_crash`, unless you prefer `to_json`/`to_crashing_json` or
    `parse_json`/`parse_json_or_crash`. Also `Json` itself: JSON is an abbreviation, but it is the format's name.
23. **Reading foreign JSON**: unknown keys are skipped, missing attributes keep their defaults, and a value of the
    wrong kind makes the whole read `null`. The alternative for missing attributes is to fail as well.
24. **The metaprogramming `Json` needed** (manual sections 4, 8, 9): `attribute: Symbol<Label>` for a template
    over another class's attributes; the plural (`show_attributes`) to call a template for every attribute;
    `$value_type == List` / `Dictionary` / `Null` / `Symbol` as compile-time type tests; `$value_type.element_type`
    to name what a container holds; text casting to an enum by name. Each is a new form, so each wants a yes or no.
25. **The number test is ten comparisons** (`$value_type == Int or $value_type == Long or ...`) because a union of
    number types is not allowed (a union's members are classes). A `Number` kind like `List` would read better,
    but it would be a name that is not a class. Which do you prefer, or should unions admit numbers?
