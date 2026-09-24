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

13. **(Answered by D109: yes, with `to_string()`. Built 2026-09-24, manual section 15 "System classes".)**
    **Whether `Console.print` takes `...values: List<Printable>`** (D90, manual section 5 "Variadic arguments").
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

## What the standard library offers

21. **Go's standard library against Spite's**, package by package, with a suggested order of what to add:
    [mortaros_go_standard_library_comparison.md](mortaros_go_standard_library_comparison.md).

## From D93 and D95 (directories and JSON)

22. **`Directory.Entry`** is the name of the union of `Directory` and `File` that `entries()` answers; folders
    come first, then files, each sorted. Should `files()` and `folders()` go now that `entries()` exists (the
    compiler's own discovery still reads names)? Manual section 15, "System classes".
23. **`Json<T>`'s API**: `Json<Order>().write(order)`, `.read(text)` (`Order?`) and `.read_or_crash(text)`. This
    settles D22's open naming pair as `read`/`read_or_crash`, unless you prefer `to_json`/`to_crashing_json` or
    `parse_json`/`parse_json_or_crash`. Also `Json` itself: JSON is an abbreviation, but it is the format's name.
24. **Reading foreign JSON**: unknown keys are skipped, missing attributes keep their defaults, and a value of the
    wrong kind makes the whole read `null`. The alternative for missing attributes is to fail as well.
25. **The metaprogramming `Json` needed** (manual sections 4, 8, 9): `attribute: Symbol<Label>` for a template
    over another class's attributes; the plural (`show_attributes`) to call a template for every attribute;
    `$value_type == List` / `Dictionary` / `Null` / `Symbol` as compile-time type tests; `$value_type.element_type`
    to name what a container holds; text casting to an enum by name. Each is a new form, so each wants a yes or no.
26. **The number test is ten comparisons** (`$value_type == Int or $value_type == Long or ...`) because a union of
    number types is not allowed (a union's members are classes). A `Number` kind like `List` would read better,
    but it would be a name that is not a class. Which do you prefer, or should unions admit numbers?

## From D106 (assert guards)

27. **Library asserts in the crash trace.** Since D106 made `assert` the only way to write a default-returning
    guard, library functions that fail a guard routinely (`String.matches_at`, `TextBytes.slice`) fill the
    32-entry trace ring a crash prints. Either the ring skips asserts in `library/`, or those functions are
    written as plain expressions the way `TextBytes.equals` now is.

## From D82, D83, D88, D92, D98, D100 and D101 (the compiler's reopening, numbers, reflection, memory)

28. **A `func` without a body** is how a member the compiler supplies is written, in its reopening and in
    `--final-classes` (`func allocate_bytes(bytes: Long): Long`); anywhere else it is an error. It borrows the
    shape a `type` uses for a member without a body. The alternative is a marker of some other kind; say if you
    want one. Manual section 11, "What the compiler supplies is a reopening too".
29. **`_` now means private, enforced**: a `_name` is read, written or called only inside its own class. D88
    needed it (otherwise `klass._name = ...` undoes the read-only getters), and section 2 already said `_name` is
    private. Open question 6 (whether `_` means private *and* unused) is still yours.
30. **`this` in every class**, not only numbers (`registry.append(this)`), with `this.member` an error. D83 said
    "if needed"; say if it should stay number-only.
31. **`value.memory` is shadowed by an attribute named `memory`**, and most of the standard library holds
    `var memory = Memory()`. Either rename those attributes (`heap`?) or give the reflection another name.
    Also the names: `Spite.Memory`, its sections `'heap'`, `'stack'`, `'constant'` (`static` is a C word).
32. **Stack memory is `memory.allocate_stack_bytes(bytes)`**, gone when the calling function returns. The lifetime
    rule is C's `alloca`: nothing stops a program from keeping the address, and each call inside a loop takes
    more of the frame. A safer form would be a region a function declares (a typed local the compiler sizes);
    that is syntax, so it waits for you.
33. **`TypedMemory<$value_type>`** is the name of what reads and writes values of any type in raw memory
    (`read_value`, `write_value`, `release_value`, `value_bytes`), one shared instance per type.
34. **`from_type` is an instance function on the class cast to** (as D100 wrote it), so calling it by hand reads
    `0.0.from_int(count)`; the compiler calls it for every number cast. A cast written in Spite inside a reopened
    `from_type` would call itself, so a reopening can only replace it with the same C cast. A class-level form
    (`Float.from_int(count)`, D6's class object) would read better.
35. **Printing an integer** with `console.print` writes the digits directly rather than calling `Int.text()`, so a
    program that reopens `Int.text()` changes interpolation but not printing. It is the faster path; say if a
    reopened `text()` should win everywhere.
36. **Unions of numbers**: item 26 asked whether unions should admit numbers. With D83 every number is a class in
    `library/`, so a union of number classes is no longer a union of names that are not classes.

## Build, the launcher and the entry (D85, D86, D89, D97; manual sections 3, 9 and 13)

37. **The launcher's name and place**: `launcher/launcher.spite`, class `Launcher`, at the repository root beside
    `library/`. It is library code for reflection (`Spite.Class.instances` leaves it out).
38. **Loading the program's folder runs it.** The launcher's `load(Build().program)` constructs the program's
    entry class; every other `load` still compiles to nothing at run time. And a launcher `load` may use `Build`
    fields (`"library/{Build().target_operating_system}"`), where every other `load` takes a literal.
39. **The C left in `main`**: handing `argv` to `Arguments()`, binary standard output on Windows (`_setmode`), and,
    after `Launcher` returns, releasing singletons and class objects and printing the `--debug_memory` report.
    Moving them into Spite needs a way for Spite to receive `argv` and to run code after the program ends (a
    `Launcher` that releases what the program left?) -- which is a language question.
40. **Every compiler option is a `Build` field, and the flags follow the field names**: `--final_classes=folder`
    (no bare form), `--repl_port=4000` (no space form), `--format=false` (no `--no-format`), no `--file=`. A `Bool`
    field may be given bare (`--optimized`), which is a second spelling of `--optimized=true` -- keep it?
41. **`mode` and `format` come from the flag alone**, because the compiler needs them before it reads the program;
    a program's `build.spite` can still declare them, but only its own code sees the value.
42. **A path to a `.spite` file still names an entry** (`spite bootstrap/spite_compiler.spite`), because the
    compiler's own entry is not named after its folder. The alternative is renaming the compiler's entry to
    `bootstrap/bootstrap.spite` (class `Bootstrap`) or moving it into a folder of its own.
43. **An unset `Build` field folds to its default** rather than being read at run time, so no `Build` value is
    ever read when the program runs. D84's words were "the others are runtime"; D85 moved run time to
    `Environment`, which is how this reads it.

## Found while auditing docs/ against the manual (2026-09-24)

Behaviour that does not match the manual. The language was not changed; each is pinned or noted in
`docs/KNOWN_ISSUES.md` so a fix shows up there.

44. **Reading `.functions` makes an instance.** `Gadget.functions` on a class with functions builds its
    `Spite.Function` values bound to a default `Gadget`, which stays alive, so `Gadget.instances.count()` is one
    more than the program made. Section 8 says `.instances` is every live instance; is the owner of a reflected
    function meant to be an instance at all (D40's `.owner` is not built)? Repro: `docs/KNOWN_ISSUES.md` item 1.
45. **A program class named like a nested library type hides it.** A program with its own `entry.spite` makes
    `library/directory.spite` resolve `Entry` to the program's class instead of `Directory.Entry`, and the program
    stops compiling ("a Directory cannot be used where a Entry is needed"). Sections 7 and 11 say a name resolves
    in the using class's own namespace first. Repro: `docs/KNOWN_ISSUES.md` item 2.
46. **Singletons with arguments.** Section 8 ("Singletons") says one instance per distinct literal argument list;
    the compiler rejects any singleton constructor with parameters (`diagnostics/singleton_arguments`) except
    `DynamicLibrary`'s, which keeps the per-argument-list behaviour. Which is the rule, and is `DynamicLibrary` the
    exception or the rule?
47. **The manual under-reports crashes.** Section 5 ("What a crash reports", "Current implementation") says the
    crash ids, the `<output-name>.crashes` map and the assert trace are not built; all three are
    (`conformance/stage5/crash_report` prints ids and `spite.assert` lines, and every build writes a `.crashes`
    file). `docs/failure.md` describes them as built. The manual paragraph wants updating.
48. **A bare `crash` is formatted to `crash false`.** Section 5 describes bare `crash` as the form for an
    unreachable branch; the formatter rewrites it, so formatted code never shows it. Keep bare `crash` (and teach
    the formatter), or make `crash false` the one form?
49. **Diagnostics name the file they are in.** Section 12's "known limitation" (every diagnostic reported
    against the entry file's path) no longer holds: errors name the real file (`registry.spite:3`). Only the
    manual needs the line removed.

## Visible storage and placement (D107, D108; manual sections 4, 10 and 15)

50. **How `String` declares its storage**: private attributes read by name -- `_bytes: Long` (the address of
    its characters), `_length: Long`, `_section: Spite.Memory.Section` (`'heap'`, or `'constant'` for a literal)
    and `_capacity: Long` -- and the compiler writes the C layout from them. The three it needs are a naming
    convention checked with an error, not a keyword. Keep the names? Is `_section` the right way for the compiler
    to record where it placed a text?
51. **How a number names its width**: `var _memory = Memory().allocate_bytes(4)` in `library/int.spite` -- an
    allocation through `Memory` like any other, which the compiler places in a register, so it is never a field;
    the compiler checks the 4 against `int32_t`, and `.memory.bytes` reads it. A header keyword (`memory 4`, the
    way `singleton` is a line) was the alternative; this form uses no new syntax. It still says only the width:
    signed, unsigned or floating is still the class's name. Should the file say that too, and how?
52. **`String(bytes, length)` is a public constructor** that takes ownership of bytes `Memory` handed out. It is
    how `Memory.text` and `+` make a `String` in Spite; it is also something a program can call wrong (bytes not
    from `allocate_bytes`, or without room for the 0 after them). Keep it public, or make constructors of
    library value classes private (`_String`?) once there is a way to say that?
53. **Placement in the frame**: an `allocate_bytes` a function frees in the same block and only lends to
    `memory`'s own functions gets a 256-byte slot in the frame (the heap past that). Is 256 the right slot, and
    should the rule reach further -- a buffer handed to a function that provably does not keep it, or one whose
    `free` is in a `drop()`?
54. **`allocate_stack_bytes` is removed**, since D108 gives the choice to the compiler and it was a second way to
    allocate. For an ECS that wants control, what stays is the layout: one allocation, offsets, `TypedMemory`.
    Is anything else wanted -- say, a hint that a structure is short-lived, which the compiler may ignore?
55. **A singleton holding nothing is not an object at run time**: `Memory` is one static instance, never
    counted or freed, which every program now allocates once less for, and without which `String.drop()` could
    reach a `Memory` the program's exit had already released. `Memory.instances` would not list it. Fine as a
    hidden optimisation (D36)?
56. **(Answered by D109: `to_string()`.)** **Item 13's `to_text()`** would be `to_string()` after D107 ("a value turns into text with `to_string()`"):
    the `Printable` proposal should use that name.

## A function of the caller for each element (D113; manual section 8)

57. **A caller function that needs more than the element** -- `print_statement(statement, depth)`, the typical
    loop of D94's review (143 of them stay `while`). Nothing is built. Options (proposed by Claude,
    unconfirmed): (a) leave them as `while`, which is where they are now; (b) the template's own arguments go to
    the function after the element, so `statements.each_print_statement(depth)` calls
    `print_statement(statement, depth)` -- no new syntax, and each extra argument is evaluated once before the
    loop, but `find_by_(value)` already takes an argument, so it would need a rule for which one is the value
    (say: `find_by_` takes none extra); (c) move `depth` into an attribute so the function takes the element
    alone, which the review called the esoteric outcome. I would build (b) for `each_` and `map_` first and
    recount; the loop rule would then also name `statements.each_print_statement(depth)`.
58. **When the element has a member and the caller a function of the same name**, the call is an error asking to
    rename one (`'each_is_adult' could call 'is_adult' of each 'Person' or this class's own 'is_adult(Person)'`).
    You asked for "the element's own member first, then the caller's function, or an error if ambiguous": I chose
    the error, because with an order a member added to a class later silently changes what an unrelated caller
    runs (D36's surprise, and D59's one name, one meaning). Keep the error, or let the element's member win?
59. **The loop rule reaches only what is exactly rewritable**, and today that is 2 loops. It leaves a loop whose
    function belongs to another object (`evaluator.process_line(lines[index])` in `examples/calculator`): a
    template calls functions of the caller only. Should `lines.each_process_line()` look at the caller's
    attributes too (here `evaluator`), or does that stay a `while`?

## Singletons bound to a variable (D110; manual sections 8 and 12)

60. **Where the binding lives**: an attribute ("on top") or a local `var` in a function are both accepted, and
    the error names the attribute. Locals are what `String` uses for `Memory` (a value class has no attribute to
    spare) and what an error path uses before `program.exit(1)`. Should a local binding be an error outside
    value classes, so there is one place for it?
61. **`Program` is a singleton now**: D8 and section 15 said so, but `library/program.spite` had no `singleton`
    line, so `Program().exit(1)` made a fresh object each time and was not covered by D110. It has the line now
    and every inline use is bound. Confirm?
62. **A number's storage is two lines**: `var memory = Memory()` and `var _memory = memory.allocate_bytes(4)`
    (item 51 was the one-line form). The binding is never a field of the number. The alternative was to exempt
    `var _memory = Memory().allocate_bytes(4)` from D110, because the compiler reads that line rather than
    running it; rejected so the file an AI reads to learn memory shows the bound form. Keep it?
63. **`Build` is a static object**, like `Memory` in item 55: every field folds to a constant, so it holds
    nothing at run time, and binding it (`var build = Build()` in the launcher and anywhere else) costs no
    allocation. Reading a field of it any way but by name (reflection over its attributes) would see nothing.
    Fine as a hidden optimisation (D36)?

## Live reload (D111, D112; manual section 14, "Live reload and 6b")

64. **A changed attribute or enum is refused, with an error saying to restart.** D111 says a change rebuilds "what
    depends on their layout", but instances already in memory have the old layout. Migrating them -- a new object
    per instance, attributes copied by name, new ones taking their defaults -- needs every live instance and every
    reference to each, which nothing finds today (`Class.instances` is not built, and references would have to be
    re-pointed). Keep refusing, build migration (and how should references be found: a per-class instance list, or
    one level of indirection per object in `--hot_reload` builds), or something else?
65. **The REPL keeps the functions the program started with.** A function added by a reload is called by the new
    code but not listed by `functions` or callable at the prompt until a restart. Should reflection follow reloads?
66. **The program waits while the library compiles** (under a second for a small program; a game drops frames).
    Compiling on a helper thread and swapping at the first wait after it finishes costs a second copy of the
    watcher's flag and nothing else. Worth it now?
67. **Only the program's own folder is watched**, flat on every system like discovery, not the folders it `load`s;
    `reload` picks those up. Watch every loaded root too?
68. **Names**: `reload` and `last_reload` at the prompt, `--mode=reload` for the compiler, and `rebuilt A, B` /
    `removed A.f` / `nothing changed since the code the program runs` as answers. When the watcher swapped a save
    in before `reload` arrived, `reload` answers `nothing changed`, and `last_reload` tells what happened. Keep?
69. **A reload library is never unloaded**, since values it made (its text literals, function values) may still be
    referenced: each reload leaves a small library loaded. Fine for development builds?

## From D114 (compile-time function reflection, for SlopEngine)

70. **The spelling of D114**: `if $system_type.has('run_each')` and `$system_type.run_each.arguments` (each entry's
    `.class` as a type, `.name` as a `Symbol`). The mechanism is yours; the spelling was the SlopEngine session's.
71. **Finding every function named `*_system` across the program** -- the same reflection over the program's
    classes. Not decided.
72. **The spelling of D115** (finding classes by namespace at compile time): `Symbol<Spite.Namespace>`, or a
    `classes` plural over a namespace pattern -- the SlopEngine session's proposals.
73. **The grammar of D116** (ordering by function name): `<phase>_each` / `<phase>_all` plus
    `run_each_before_<phase>` / `run_each_after_<phase>`, and how a template spells a name pattern whose matched
    part it can read.

## From D109 (printing through `to_string()`, `Console.debug`; manual sections 5, 8 and 15)

74. **A number, `Bool`, `Symbol` or enum value passed where a `type` is wanted is boxed**: one small allocation,
    freed like any object, so `print(count)` costs a box and the `String` its `to_string()` makes, and every
    `print` costs the `List` its values arrive in. The self-compile did not slow measurably, and the two corpus
    programs that pin allocations moved from 29 to 50 and from 11 to 17. Is that an acceptable visible cost, or
    should a variadic list the callee only reads live in the caller's frame (the D108 placement rule, one step
    further)?
75. **Interpolation does not call a class's `to_string()`**: `console.print(ticket)` prints a `Ticket` that
    declares one, and `"{ticket}"` is still the error "a Ticket cannot be used where a String is needed", so text
    is written `"{ticket.to_string()}"`. D107 says interpolation calls `to_string()`; should it for a class too?
76. **An enum value answers `to_string()` and `to_debug()` and nothing else**, which is what lets it be
    `Printable` and `Debuggable`. Should an enum be able to declare more, the way a number's class does (a file
    per enum is not a thing yet)?
77. **The cycle rule for `to_debug()`**: an object already being shown further up is written `Name {...}`, found
    by `==` against the objects of its class being shown (identity, unless the class defines `equals`). A tree of
    distinct objects is shown whole however deep. The alternatives were a depth limit, or `{...}` for any object
    of a class already on the way down (which would cut a linked list after one node).
78. **What `to_debug()` writes**: `Name { attribute: value }` and `Name {}`, `[a, b]`, `{"key": value}`, text in
    double quotes with `\"`, `\\` and `\n` escaped, a `Symbol` or enum value as `'name'`, numbers and `Bool` as
    printed, `null`, a `Spite.Class` as its name, a `DynamicLibrary` as its `file_name`. Private `_` attributes
    are left out, because the plural attribute template now skips another class's private attributes instead of
    failing on them, which changes `Json` the same way. Show them instead (a reflection read the plural is
    allowed and nothing else is)?
79. **The REPL could show values with `to_debug()`**: today it shows `Player { name: hero, ... }` (text unquoted,
    nested objects as `Name {...}`, lists as `List<String>(...)`) from `Spite.Attribute`'s text, and the
    documented sessions depend on that. Switching means quoting text and nesting fully in every answer. Want it?
80. **The names**: `Console.debug`, `type Debuggable { to_debug(): String }` beside `Printable`,
    `Spite.Debug<$value_type>` for the text of any value and `Spite.DebugInstance<$value_type>` for walking a
    class instance -- in `Spite` because they are reflection, and so a program cannot reopen them by accident.

## From the SlopEngine bug batches (manual sections 5, 7, 8, 9 and 11)

81. **A Symbol template and another class's private attributes.** `func fill_attribute(attribute:
    Symbol<Target>, ...)` in `Query` cannot read `target._cache`: `_` is private to `Target`, and the template
    belongs to `Query`, so the single call `fill__cache(...)` is the privacy error and the plural skips `_cache`
    (item 78's change). A template is metaprogramming acting on `Target`'s shape, so it could be allowed to see
    what `Target` sees -- or privacy could stay absolute, as today. The rule is unchanged; which one?
82. **Reopening a class from the program root when a loaded folder declares it.** The program's folder is
    merged first and each `load("package")` after it, so a root `monster.spite` reopening `package/monster.spite`
    keeps its new attributes and functions but loses its constructor (and any function both declare) to the
    loaded folder's version: "later replaces" means loaded after, never "the program wins". `docs/packages.md`
    now says so. Should the program root win over what it loads (merge the root last), or should a name declared
    in both be an error unless it is in a folder loaded for the purpose, like `mods`?
83. **Item 75 answered provisionally**: a text hole now calls a class's `to_string()` (`"{ticket}"` works, and
    `"{attribute.class}"` prints the type's name), the way `console.print` does. Keep it, or require the call to
    be written?
84. **The default of a `type` is an object literal** (manual open question 1): `var row: $row_type = null` bound
    to `type Target { health: Health }` now holds `{health: Health()}`, whose `.class` answers `Object`. A `type`
    that requires a function has no default object and stays a null pointer that reads defaults. Should that
    case be a compile error naming the field instead?
85. **A singleton is no longer reference counted** (D36; measured 0.8 s to 0.04 s for two threads fetching a
    generic singleton 20 million times each). Singletons are destroyed at exit in the order they were made,
    reversed; before, the counts decided it. One visible difference: a singleton still referenced by a leaked
    object is destroyed anyway. Fine?
86. **`Clock()`'s names**: `elapsed_nanoseconds()`, `elapsed_milliseconds()` (monotonic) and
    `unix_milliseconds()` (wall clock). Keep them? And should the current date broken into year, month, day,
    hour, minute and second live here too, or is the Unix time enough until something needs a calendar?

## From the Vulkan renderer bugs (manual sections 11, 12, 13, 15 and 17)

87. **A loaded package cannot find its own folder at run time.** SlopEngine locates its shader sources by
    reopening `Build` with `var slop_folder = "../../slop"`, a copy of its `load` literal that breaks when the
    loader moves. Proposal: `Spite.Namespace.folder` -- the folder a namespace was loaded from, as the compiler
    resolved it -- or the `load` path available per namespace some other way. Which, if either?
88. **A loaded folder that is itself a program merges into the loader.** A test program that `load`s a game
    folder, and has its own root `composition.spite` or `plugin.spite`, silently reopens the game's classes of
    the same name (reopening, by design), which cost SlopEngine a debugging round. Master now makes reopening
    the entry class an error; this is the same trap one level down. Proposal: a loaded folder that has an entry
    file (a file named after the folder) keeps its root classes to itself, or merging with it must be asked for.
    Related to item 82.
89. **Where a program runs, and where its build goes.** The compiler now finds `launcher/` and `library/` from its
    own executable (the first folder above it holding `launcher/launcher.spite`, else the working directory),
    the program runs in the caller's folder, and the build still goes to the language repository's
    `.spite-cache/`, so a run leaves nothing in the caller's folder. Should the cache instead live beside the
    program (`game/.spite-cache/`), which is where a user would look for the built executable?
90. **The compiler's own C names use `___`, and a name has one `_` between words.** `allocate`, `make`,
    `retain`, `release` and the rest are ordinary method names now, and `hit__count`, `__strike` and `strike_`
    are naming errors. `init` stays an abbreviation error and `default` a C keyword error, so neither became a
    legal method name. Is the stricter snake_case rule fine?
91. **A foreign call with a header goes through the header's prototype.** C checks the count and converts each
    argument; converting an integer to a pointer (a `Long` handle) is the one complaint silenced, so a `String`
    passed where C wants an `int` is converted silently too. A C function returning `void` reads as `0`. Every
    called function must be declared by the header, so a header that does not declare one is a C error at build
    time. Without a header, integers cross as 64 bits. Fine, or should the header be required whenever a call
    passes anything but a `Long`?
92. **`Memory`'s new widths are named after the types**: `read_short`, `read_unsigned_short`, `read_unsigned_int`,
    `read_float` and their `write_*`, beside `read_int`. `Tiny` and `UnsignedLong` have none, since `read_byte`
    and `read_long` hold the same bits. A variable cannot be named `unsigned_int_bits` (`int` abbreviates), but
    these follow the type names, as `read_int` already did. Keep, or `read_unsigned_integer`?
