# Implementation status

What the [docs](../docs/README.md) describe but the compiler does not build yet, or builds only in part, page by
page, with what was said on each page before its status was moved here. The docs teach the language as decided
and say nothing about status; this page is where status lives. When something here is built, delete its line;
when a page gains a rule that is not built yet, add it here.

## [classes_and_files.md](../docs/classes_and_files.md)

### One instance per argument values
- Unconfirmed (proposed by Claude), built that way: the arguments are literals the compiler reads while compiling (strings, numbers, `true`, `false`, enum values), so each distinct list is its own static slot; any other argument is an error (`diagnostics/singleton_arguments`).
- The REPL prompt still refuses a singleton with arguments (`'World' is a singleton, so it takes no arguments`), so an instance such as `Channel(1)` cannot be reached from the prompt yet.

### Singleton rules
- Unconfirmed, proposed by Claude, now stated as rules on the page: a generic singleton has one instance per set of codegen values; local-binding exception covers every value class plus `List`, `Vector`, `Dictionary` and generic singletons whose codegen values come from the function's own codegen or Symbol; unchecked uncalled functions for inline singletons; teardown "made" means constructor finished; circle error texts; once-per-thread first-fetch lock; empty singletons may be made again at exit; reopening may add the `singleton` line.
- Unconfirmed: the dropped-construction error message text (`a constructed object must be kept and used`).

## [programs.md](../docs/programs.md)

### Program settings: `Environment`

- Unconfirmed (proposed by Claude, not decided by Mortaro), built as the page describes: the members of
  `Arguments()` (`count()`, `get(index)`, `.name`), the details of how a program is started (launcher, `main`), the
  order of value sources and the error texts of the `Environment` settings. From the kebab-case settings
  decision: the arguments after the folder that are not flags go to the program; a flag naming no field of
  `Build` or `Environment` stays a compile error, so a raw `--name=value` read only through `Arguments` reaches a
  program only when it is run directly; `.player_name` on `Arguments` reads `--player-name`; a bare `Boolean`
  setting; a setting's value checked while compiling; `--` an ordinary argument to a program run directly.

### Build settings: `Build`

- Unconfirmed (proposed by Claude, not decided by Mortaro), built as the page describes: most of the `Build` details
  (constant folding, the `target_operating_system` error, the error texts, the `--final-classes` behaviour).
- The last paragraph of the page said `Build` is the home of the isomorphic split and of the target field read in
  [Other environments](../docs/targets.md#other-environments), which was still planned and is gone from this page;
  the `$target` variable no longer exists.

## [values_and_types.md](../docs/values_and_types.md)

### Symbol codegen and enums (Rules in full: Enums in full)
- Not built: calling a template with a symbol written out, `person.set_attribute('age', 2)`. Today it is the error "'Person' does not define 'set_attribute'"; a template is reached only through the names it answers. The page no longer mentions it.
- Not built: environments as a reopenable, walkable enum (D180). The page now says "Environments are an enum too" in the decided tense.
- Not built (D386): a numbered enum's number as its identity in binary files and on the network. `BinaryFormat`
  writes an enum value as its position in the enum, which is its number only when no `=` is written; writing the
  number needs a way for library Spite to read a value's number, which is not decided. Bindings (F1) are not built.
- Proposed by Claude, unconfirmed (D???): an enum value's number fits an `Integer`, may be negative, and a hot reload
  cannot renumber a value the running program has; the error texts.

### Numeric types
- PROVISIONAL: the exact width mapping of the ten numeric types was never explicitly confirmed by Mortaro; revisit if a different mapping is wanted. Removed from the page, including the "(proposed by Claude, unconfirmed: the exact width mapping)" note.

### Numbers are classes, and `this`
- Open: both the assignment cast (`var half: Float = count`) and the call form `count.to_float()` are allowed "for now"; the call form may be limited later. Page says both are allowed.
- Unconfirmed proposals by Claude (removed from the page, still awaiting Mortaro): names of the bitwise functions (D117) and their rules, names of the maths functions and constants and their lowering, names of the bit-reinterpretation functions (D215), the `type` keyword allowed as a parameter name, the error texts for `from_` declarations, the overflow and narrowing messages, the names `wrapping_subtract` and the wrapping functions' reading (D359), the reach of text casting to every place a `String` is wanted (D223), the lone-hole error wording, the enum-from-text cast, the generic walk of enums, the shape-member behaviour, the class-test forms for generic classes and codegen values (D123).
- Exact error texts still quote decision numbers: the `from_` declaration and `from_` call errors contain "(D293)" (lines 925 and 928); the compiler text must change with the page.

### Every number fits `Number`
- Built: `type Number`, every number class fitting it, `$value_type == Number`, a generic constrained by it
  (`conformance/stage6/number_type`), and a parameter `value: Number` with operators, compiled per number class,
  with a run-time switch over both sides' classes where the function runs as written
  (`conformance/stage6/number_parameter`).
- Unconfirmed proposals by Claude: the run-time rule for an operator on `Number` in a build that compiles the
  function as written (the right side is turned into the left side's class and must fit exactly, or the program
  halts).
- NOT BUILT: an operator through a `type` that requires its function, on a class instance (not a number) whose
  class is known only at run time, in a function that runs as written: it halts with "was given a value of a class
  it is not compiled for" instead of calling the class's operator function.
- Unconfirmed proposals by Claude: the member list (`remainder` joined it once `%` on `Float` and `Double` compiled
  to `fmodf`/`fmod`), and a `type`'s own name in a required signature standing for the class that fits.

### Numeric types (REPL and text reading)
- No status facts removed beyond the above.

### Inline types and duck typing: a function taking a `type` is compiled per class (D321)
- Built: a copy per class for a call whose argument's class is known while compiling, and a `switch` over the
  shape's closed set for a value read at run time, with the function as written compiled under another name and
  shaken out (`conformance/stage6/shape_copies`, `conformance/stage6/number_parameter`, `benchmarks/shape_calls`).
- Built: a call through a shape, on a value of a class no case was compiled for, halts naming the shape and the
  function instead of answering a default.
- Built: a `type` that requires no attributes is a tagged value (a class id and the object or the plain value), so a
  number, `Boolean` or enum stored as one allocates nothing (`conformance/stage6/tagged_values`).
- NOT BUILT: text and a `Symbol` stored as a `type` are still heap boxes; a `type` with attributes (a row) and a
  union stay pointers.
- NOT BUILT: the closed set tested for a run-time value is every class the program admits to the `type`, not the
  classes that reach that particular spot.
- NOT BUILT: a nullable `type` parameter, a parameter the function assigns to, a function that waits, and the
  functions of the library's containers keep the version as written; in a program that runs a `Concurrent`, a
  run-time value reaches the version as written.

## [failure.md](../docs/failure.md)

### Nothing fails silently: still open

Moved whole from the old "Still open" list under the rule (each is a bug under D244, recorded so it is not mistaken
for a design):

- A `Concurrent` or `Parallel` handle whose function answers a `Boolean?` is accepted as a condition by `if`,
  `while` and `crash` and tests only that a value came back, so a joined `false` runs the `if`'s block (the
  narrowing of a joined handle skips the "a `Boolean?` cannot be a condition" check; `assert` asks for `true`).
- Reference cycles leak without a word unless the program runs with `--debug-memory`, which prints the allocation
  balance (memory.md, "Cycles leak").
- A `--hot-reload` build's watcher thread keeps running while the singletons are destroyed at exit: `start()` now
  waits until it is watching, so it no longer asks for `HotReload` after the teardown, but a file change landing
  during the teardown would still run `compile_changes()` on the destroyed `HotReload`, and so would a reload
  still waiting for the library before it to be swapped in.
- A `--hot-reload` program takes the size and modification time of its files when it starts, not when the build
  read them, so a file saved while the build was still compiling looks compiled: `wait_reload` answers at once and
  the program runs the code before the save until the next save reaches the watcher. The build should record each
  file's size and modification time beside its hash in `.reload_start`, or the watcher should compile once when it
  starts (proposed by Claude, unconfirmed).
- A REPL `exit` answered at the same wait as the command before it ends the program before its loop sees that
  command: on Linux, concurrency.md's `frame_loop` session (`program.running = false`, then `exit`) lost its
  `stopped` line in 3 of 5 runs, so `check.sh` fails its wire replay intermittently. The program's last output is
  dropped without a word; `exit` should let the program reach its next wait first.
- A `Concurrent` polled for `finished` under `resume_only_when_asked()` without `run_ready()` never ends
  (concurrency.md, "Choosing where Concurrents resume").
- A Windows `__fastfail` (`0xC0000409`) ends the program without Spite's report or frames ("What a native fault
  reports"): the system ends the process without asking it, so only something outside the process could report it.
- A write to the attributes of a copy that nothing reads afterwards is lost without a word: a function answers
  `values[row].copy()`, the caller sets `layout.width` on it, and the copy dies. Proposed by Claude, unconfirmed: a
  compile error when an object only this function holds (escape analysis already proves a function's result fresh)
  has its attributes written and then dies unread, unpassed, unreturned and unkept; checked while compiling, it costs
  nothing at run time.
- A reload that moves objects to new attributes (repl.md, "Changing a class's attributes") moves them while the
  program's own threads may run: the swap pauses the scheduler's tasks, not a thread the program started itself, so a
  thread reading an object of the class while it moves could read its old attributes. The move should wait for every
  thread to reach a point where it holds nothing, as the swap of code does for the main loop.

Also open, each a bug under D244, found cataloguing the compiler's proofs (proofs.md):

- **A function that lends a list element (D269) is not checked for a guard `assert`** (D106), as other functions are.
- **A frame buffer's uses are matched by name.** Placing an allocation in the frame accepts `read_value`,
  `write_value`, `release_value` and `swap_values` on any receiver, not only `TypedMemory`'s, so a program's own
  `write_value` that keeps the address would pass (memory.md, "Placement: the compiler decides where memory lives").
- **`absolute()` of the smallest signed value** answers that value itself (`Integer.smallest.absolute()` is
  negative): it is a supplied macro with no line to name, so it is not yet checked like `-value` is (D359).
- **A change of signedness at the same width or wider** (`var bits: UnsignedInteger = count` with a negative
  `count`) keeps the bits unchecked: D162's "wider" leaves signedness out, and the hashes read words this way.
  Whether it should halt, with a named function for reading the bits, waits for Mortaro (D359; proposed by Claude).
- **A `while true` that can never leave** ends its function's paths for the missing-`return` check, and nothing
  reports it outside a locked singleton function: a hang.
- **Two threads writing one number attribute of an instance they share** is refused only when the instance is a
  local handed to the `Parallel` (concurrency.md, "A task may keep what was handed to it"); `Parallel(own_function)`,
  a parameter, an attribute, or a local used before the `Parallel` still race, the result whichever write lands last.

### Nothing fails silently: the rule

- The bold paragraphs of this section were tagged `[implemented]` (an `if` that only returns the default is an `assert`;
  an `if` that leaves proves the rest; `assert` narrows every link of a chain; copy-to-narrow; a check that proves
  nothing; `while value` narrowing; call-effect proofs; `Every [] answers T?`; `first()`/`last()`/`remove_first()`
  answering `T?`; comparing a `T?`). Nothing in them was recorded as not built.
- Stated on the page as plain rules but recorded in the source as "proposed by Claude, unconfirmed" (not decided by
  Mortaro): the count, bound, bound-past-the-index, kept-count and proven-before-the-loop `[]` proofs; any index
  without a call is a path; how a call's effects are followed (receiver classes, every function of that name,
  `append`/`prepend`/`insert` growing a borrowed `Vector`); a list literal proving its indices; `first()`/`last()`
  answering `T?`; narrowing each side of an `and`; the message texts for a check that proves nothing; how the
  guard-`if` condition is turned around (De Morgan); "an `if` that leaves proves the opposite of its condition" (the
  whole rule); `while value` narrowing; the scope of the copy-to-narrow check.
- Open idea, removed from the page: a class that declares `count()` and whose `get_at` answers `null` exactly
  outside `0..count()` could earn the same proofs as the library's collections later (proposed by Claude,
  unconfirmed; today a read through a program's own `get_at` is narrowed only by what the program writes).

### Failure: three outcomes and no others

- The heading was tagged `[partial]`. Not built, from the decided design: the call chain in a crash report, and the
  default each failed `assert` returned (D25); an assert's values in the ring (D33); the column of a `.crashes` line,
  which is written as `0`; crash ids that compare across targets other than native (targets.md: other targets are not
  built, so "the same id on the server bundle and the browser bundle" is decided, not built); the wrong-target compile
  error (D20) is planned (targets.md).
- Not built, proposed by Claude and unconfirmed (D26 refinement), removed from the page: the trace would record
  predicate asserts only. A narrowing assert firing is routine control flow (thousands an hour on a server), and on
  concurrent work the last few would come from unrelated requests, reading as a causal chain that does not exist; a
  per-site count would cover them instead. Today a failed narrowing `assert` (`assert found`) enters the ring like a
  predicate one.

### assert is control flow

- Unconfirmed (proposed by Claude), stated as rules on the page: the `assert` default-answer message, the choice of
  `-1` in it, and that `String` counts as a value; the missing-`return` check, its reach and its message; D245 (which
  types answer what) was decided by Claude under D244.
- `--trace-asserts`: the field's name (`trace_asserts`) is provisional under D214; the rest of the design is proposed
  by Claude, unconfirmed.

### `crash`

- D297's report contents beyond the condition's operands (parameters, locals and attributes in scope, item 226 of
  `mortaros_missing_decisions.md`) are proposed by Claude, unconfirmed; Mortaro only confirmed that the condition's text
  is not in the report ("probably not, location + memory is enough").

### What a native fault reports

- The Linux handler (`sigaction`, alternate stack, frame-pointer walk) runs in `check.sh` on Linux, with gcc and
  clang: `native_fault_foreign`, `native_fault_stack`, `native_fault_illegal` and `native_fault_heap` compare against
  their `expected_output.linux.txt` (a signal number where Windows has an exception code, and the shell's own line,
  such as `Segmentation fault`, as the compiler's run of the program reports the death). The macOS handler is
  compiled but has not been run. On Linux and macOS a corrupted heap is found by the C library, which prints its own
  message and aborts (`SIGABRT`), so `native_fault_heap`'s Linux output pins glibc's `free(): invalid pointer`
  (still open above).
- The design and the field names of the `spite.fault` line are Claude's proposal, unconfirmed, field names
  provisional under D214.
- check.sh compares the fault reports with the offset after `+0x` left out and also builds `native_fault_foreign`
  `--optimized` from four translation units; removed from the page as a maintainer note.

### What a crash reports

- Removed as maintainer notes: check.sh builds a program whose pool thread fails an `assert` without end while the
  program's thread crashes, and requires one crash line, at most 32 assert lines, the `earlier=` count and exit status 1
  within a minute. The ring-count fix history (a 1.4 GB log in five minutes, found by a game port, A73) was
  dropped.
- History removed: the migration of `List.get_at` (an index outside the list now halts, as `Vector.get_at` did), and
  of `JsonReader.read_symbol` and `BinaryFormat.read_symbol` (a placeholder symbol written down after a failure).
- `--optimized` builds carry only a site's id: other targets are not built (targets.md).

## [functions_and_operators.md](../docs/functions_and_operators.md)

### Functions are values

- Not built: reading a function value's `.owner` (`Spite.Function<...>` does not say what class the owner is, so there
  is no type to give it; reading it today is the error "'Spite.Function<...>' has no attribute 'owner'", naming the
  value's Spite type).
- Not built: calling a function found through reflection (a *reflected* function) with arguments.
- Not built: reopening `call_function` to trace or count calls. `call_function` is supplied by the compiler and is not
  declared in `library/spite/function.spite`, so a program's `spite/function.spite` that declares one compiles and is
  never called.

### Use the operator, not its function

- Built (D315, D365). Unconfirmed (proposed by Claude, not decided by Mortaro): the error wording; a union receiver
  covered only when every member offers the operator; an `equals` that `==` would not call exempt; only calls
  written in the source are checked (a call the compiler writes for a member template is not).
- Unconfirmed (proposed by Claude, not decided by Mortaro), built as described in failure.md: the narrowed read
  local of D365 is any `var` whose value is a path reading through a `[]` anywhere (`grid[row].cells[column]` too),
  not only a `Dictionary` key. A `.` makes the next word a member name in the lexer, so it holds everywhere a word
  follows a `.` (namespaces, type paths, codegen paths), not only after a value.

### Operators

- Unconfirmed (proposed by Claude, not decided by Mortaro), built as described on the page: the `get_at` and `set_at`
  details, the read half of getter interception mirroring the setter half, a setter answering a write with no
  attribute of its name (from a game port's alert A101), the bare-name exclusion from the direct-call rule, the
  variadic "pass the elements" error, a number, `Boolean` or enum value boxed to fit a `type`, and the reserved
  signature error for `drop`, `to_string` and the other compiler-called functions.

## [control_flow.md](../docs/control_flow.md)

### Control flow in full
- No implementation status was stated on this page beyond `[implemented]` tags (all removed); nothing is unbuilt.
- Unconfirmed proposals by Claude, removed from the page and still awaiting Mortaro: the "statement ends with its line" messages (D244), the value-`switch` case syntax, other kinds and messages (D239), the threshold and message of the `if`-chain-to-`switch` error (D239), the exact shape of the "while a member template already says" check and which callees count for `f(item)` forms (D171), and the choice that a local `--repl` build gets no check point in `while` (D174 names `--repl` builds; D147 argues none).
- Removed history: before the end-of-line rule, `while ... { ... } console.print(total)` compiled as two statements; every chain in the compiler and `library/` was converted to a switch (44 in all).

## [style.md](../docs/style.md)

### Unused is an error
- Unconfirmed (proposed by Claude), stated on the page as rules: an attribute whose declaration statement already failed is not also reported unread; the exact list of what counts as a read and the exceptions; which folders the public-attribute check covers (program folder, standard library, private attributes everywhere); a read of an unused singleton binding from another class counts.

### One call per line, and nothing said twice
- OPEN question for Mortaro: the markup feature (targets.md, Markup) nests tag calls (`html.div({ class: "card" }, html.h1(title), ...)`), which the no-call-as-argument and no-constructor-as-argument rules forbid as written. It is undecided whether a tag counts as a function or as a constructor.
- Unconfirmed (proposed by Claude), stated on the page: the readings of the no-call-argument rule (holes in texts, receivers, conditions), the constructor-argument readings (receiver, whole `var`, `Parallel`/`Concurrent` function values), and the exact narrow reading of the repeated-branch rule.

### Formatting
- Unconfirmed (proposed by Claude), stated on the page: every concrete formatting rule beyond the decision that the compiler is the formatter; placement of `union` beside `enum` and before `type`.
- Removed check.sh details: check.sh requires every file outside `diagnostics/` to be formatted, compiles `diagnostics/blank_line` from a copy and checks the copy came back formatted, and keeps every documentation program not marked `error` formatted.

### Naming and abbreviations: compile errors, not auto-fixed
- Unconfirmed (proposed by Claude), stated on the page: the one-underscore-between-words rule (implemented 2026-09-24).
- Unconfirmed (proposed by Claude), built: the second table of abbreviations (`cnt`, `buf`, `idxs`, ...), which the
  `'windows'` naming rule does not read backwards because Win32 spells those words out in full.

## [memory.md](../docs/memory.md)

### The floor: `Memory.Address`, `Memory.Heap` and `TypedMemory<T>`
- Not built: the heap asking the operating system for pages itself, and `copy_to` and `compare_bytes` as plain Spite through `DynamicLibrary`. Today `Memory.Heap`'s four functions (`allocate`, `resize`, `free`, `live_allocations`) are bodies the compiler supplies over the C library's `realloc` and `free` (with the live-allocation counter), and `copy_to` and `compare_bytes` are written in place as `memmove` and `memcmp`. `library/memory/heap.spite` declares only `singleton`, so the "zero hidden code" goal is not reached for the heap yet. (Was decided as D178 and D147.) The heading was tagged "implemented; OS pages planned".

### Allocators: `.memory.allocator`
- Only partly built (heading was "implemented for objects; a list's buffer and a vector's block planned"). Works for objects. Not built: a list's buffer following its list's allocator (a list placed in an arena keeps its buffer of references on the heap); likewise a `Vector<T>`'s block of items stays on the heap wherever the vector object is (a class reading its own allocator, open item 175, is unresolved); `Memory.Frame` (open item 173 in mortaros_missing_decisions.md); `reset()` on an arena (no `reset()` because resetting under live objects is the undecided safety rule of item 173); reading `.memory.allocator` back. The page states the list-buffer and vector-block facts as plain behaviour ("stays on the heap") and does not mention `Memory.Frame`, `reset()` or reading the allocator back.

### Where a value lives: `.memory` (overview section, "Choosing an allocator")
- The overview previously said a `Vector`'s block of items "stays on the heap for now" and pointed to "what is not built yet"; both now state only the plain fact (see the allocators note above).

### Placement: the compiler decides where memory lives
- Heading was "implemented; the rule proposed by Claude, unconfirmed". The frame placement of buffers, frame placement of objects, and the `DynamicLibrary` lending rule in `library/` are built but their rules were marked unconfirmed by Mortaro (proposed by Claude, decided under D205/D214, D211). Confirmation pending.

### The memory model
- `Weak<T>` implementation (the table keyed by address, boxes, the per-class freeing check) and the rule that a `Weak` stays on the program's own thread (compile error at `Parallel`) are built but were marked proposed by Claude, unconfirmed (D197 itself is decided). `copy()`/`deep_copy()` names were "proposed by Claude, unconfirmed". The rule that an argument a caller already holds is passed uncounted (D270) is built, unconfirmed.

### Borrowed items of a `Vector<T>`
- All of the readings in this section were marked proposed by Claude, unconfirmed (error texts, `reserve` counting as growing, the reopened-`Vector` check, what a call can reach, rows, walked rows, sparse walked rows, the local a template changes, lent list elements, `Items<T>` items, arguments a plural fills, items lent to the caller and to a call, walked lines naming the class's full name). Only the decision that a `Vector` read is a borrowed item (D204) and that plain values leave `Vector`/`Items` (D225) are Mortaro's.
- The names `attribute.index` and `fits_vector()` are provisional (D214). The `Items<T>` name is provisional (collections.md).
- Open: a mention of D144 and D110 errors (a singleton written inline or held as an attribute) was reworded to "the usual errors for a singleton made inline leave it out"; a maintainer should check that wording against those decisions.

## [metaprogramming.md](../docs/metaprogramming.md)

### Symbol codegen
- Every spelling in the rules below is provisional or unconfirmed by Mortaro where the old page said "proposed by Claude, unconfirmed" or "name provisional" (D214): the plural-of-plural forms, `attribute.index`, `attribute.camel_case_name`, `attribute.pascal_case_name`, `Symbol<Row>` over a `type`, the class-text reading (`"{attribute.class}"`, D109 reading applied to text), the `Symbol<Spite.Class>` walk, `package_folder()`, `has_state()`, `argument_class()`, `returned_text()`, `function_waits` (spelling and reading), `function_writes_parameter`, `any_attribute_fits_vector`, `argument_count`, `fits_vector`, the folded `assert`/`crash`, `$T == Enum`, "what survives folding is compiled", the codegen-name reads (`element_type`, `value_type`), `$component_type.name`, the passed-symbol helper, the folder range matching no folder, how the enum for a pattern hole is found (D180 is Mortaro's; the lookup is unconfirmed), the generic-constraint details (D175 is Mortaro's; the details are unconfirmed), and the omitted-values inference and `$name()` default (D236).
- Bug (found 2026-10-02): `--final-classes` prints a plural walk over another class (`show_attributes(gadget)`
  calling `show_attribute(attribute: Symbol<Gadget>, gadget: Gadget)`) twice, so the printed program fails with
  "declares 'show_attributes' twice".
- Not built (D229, decided by Mortaro; spelling proposed by the game engine package, provisional): `$system_type.function_runs_in_pieces("<phase>_each")`. It would fold like `has_function`, true when the function with everything it calls is safe to run in pieces in D35's terms (writes no attribute of its own class, touches its arguments only through their own attributes, otherwise only locals and singletons made safe; reads of shared state must not lock per row, D184's read-only and single-owner forms). Nothing in the compiler answers it yet.
- Not built: constraining a `generic` line by a union ("one of these classes"). The page lists it only as an error ("a constraint names a `type`").
- History removed from the page: the plural walk replaced the compile-time `for instance.attributes`; `Symbol<Spite.Class>` used to mean the attributes of `Spite.Class`; the template-function clash error was an argument mismatch at line 0 (fixed 2026-09-27); `attribute.class == Entity` folded only for class-typed attributes (fixed 2026-09-27); the `.class` comparison fold used to compare against a made class object, answer `false` and leak it (check.sh greps `conformance/stage6/walked_class_fold`'s C for any class object); a `crash` that folded was once compiled as a value and raised "'$slot_type' is a type here, so it cannot be used as a value" (was D5/D9/D87/D104 history of the `generic` line replacing the `generics` header and the constructor list; the alternatives are in the decision log).
- Mortaro's 2026-09-27 decision removed from the page: "every attribute.class should be possible to figure out at compile time after we resolve generics and monomorphise" (built as proposed by Claude, unconfirmed).
- Mortaro's D123 quote removed: "function generics can ONLY come if the class has a generic declared for it".
- Mortaro's D175 quote removed: "generics being possibly unused code is used for tree shake, its on purpose not an error" (D167); the page states the reason in its own words.

### A class's functions, a folder's classes and a name's pattern
- The page no longer teaches the ranges, plural walks, name patterns or name-keyed questions (D316); they still
  compile and are listed under reflection.md below for migration.

### Codegen values (`$`)
- Reordering `generic` lines changes the meaning of positional call sites: a deliberate, accepted trade (Mortaro); the page says "deliberate, accepted trade" without the citation.

### Tree shaking
- Inspectable builds (`--development`, `--hot-reload`, `--repl`, `--repl-port`) keep everything reachable-or-not (D143); no status fact removed, only the citation.

## [reflection.md](../docs/reflection.md)

### Reflection objects
- Section was tagged partial. The part not built: the exact shape of the class object's attribute mapping
  (`Weapon.attributes['damage']`, keyed by symbol, and what it answers for a field the instance has not set) still
  needs design (PLAN.md milestone 10). The page states the decided rule (ordinary indexing at both levels).
- Several readings carried "proposed by Claude, unconfirmed" and are still unconfirmed: the `.functions` contents
  rule, the reach of the private `_` rule, the shape of `value.memory`, the class-as-argument readings
  (`Spite.Class?`, assignment, `return`, codegen values), the stand-in rules (singleton stand-in never runs
  `drop()`, entry class without stand-in and `_unbound_functions`, the class and namespace object lock), the
  "list holds every function once compiling ends" rule, the `attribute.value` readings, and the list of reserved
  names (`class`, `attributes`, `functions`, `instances`, `memory`). Open question about the reserved names list
  decided by Claude under D205.
- "Decided by Mortaro, being implemented" item 9 (open_questions.md) is the source of the `.functions` rule.
- Removed clause: a `load`-ed root "would extend the deferred compile-time class generation" (deferred, not built).

### Reflection known while compiling (D316, D317, D318, D323, D326)
- The pages (metaprogramming.md, reflection.md) now teach only the D316 model. Shown there as untitled snippets because
  they do not compile yet: `call_with`, a function taking `value: Number` (D321) and the `map_is_alive()` error (#228,
  being built on its own branch).
- Still teaching the old forms, to migrate once the compiler reaches them: memory.md's titled engine programs
  (`Symbol<$row_type>` walks, `fill_attributes(...)` plurals, `Symbol<$system_type.phase_each>`), json.md's
  `write_attribute(attribute: Symbol<$value_type>, ...)` and `$value_type.has_function("json_key_{attribute.name}")`
  (a name built from text, which D317 forbids: D320's rename map replaces it, and a map keyed by attribute objects
  needs a dictionary keyed by them, which no dictionary is yet), the library itself (`json_writer`, `json_reader`
  and `binary_format` still walk with `Symbol<$value_type>` plurals: a walk function takes only the element, so
  the output or the object being filled cannot reach it, and how it should is open;
  `library/spite/*.spite` still declares
  `has_function`, `function_waits`, `argument_count`, `fits_vector`, `source_folder`, `name_fits`, `waits()`), the
  diagnostic "write 'member: Symbol<$element_type>'" (collections.md, `diagnostics/plain_symbol_on_list`), and
  `design/for_ai_writers.md`.
- Built: constants, folding, `each`/`map` unrolling, specialisation per reflection argument, bound attributes,
  types from constants, `Spite.Namespace.instances`, enum `.values`, `[]` by name, `Spite.Attribute<T>` template
  spelling, `filter_<member>_<function>(arguments)` on any list, `source_files`/`source_directories`;
  `package_folder` removed.
- Built (D317, D328, D330, D335): a member template whose name holds its parameter in the plural
  (`collect_members(member: Spite.Attribute<$element_type>)`) finds its member by `String.pluralize()`, with the
  singular, two-members and does-not-inflect-back errors (`conformance/stage6/plural_templates`,
  `diagnostics/plural_templates`); a list of a union keeps one member class with `filter_<classes>()`, answering a
  list of that class, and templates an attribute every member has (`Directory` and `File` gained `name`); the
  question templates name `is_<word>` as `<word>`; a `Dictionary` takes the member templates over its values.
  Proposed by Claude, unconfirmed: the plural rule is read only on `List`, `Vector`, `Items` and `Dictionary`
  templates (a class's own templates keep the old plural walk until it goes); a question member (`is_`, `has_`,
  `can_`) is never inflected, and `map_` over one is D369's error; `filter_is_<word>` stays the same template as `filter_<word>`; `health` was
  added to the uncountable words, since metaprogramming.md reads `map_health` as `health`; the error texts.
  Not built: `String.Inflection`, the table a program reopens to add words (the compiler inflects with its own
  copy of `String`, so a program's words would not reach it either).
- Built: the library's member templates take `member: Spite.Attribute<$element_type>` and collect with
  `map_members`, and every `map_<member>` call in the repository is plural; a `List`, `Vector`, `Items` or
  `Dictionary` template named with its parameter in the singular that answers `List<member.class>` is an error
  naming the plural. `Spite.DebugInstance` still walks with a `Symbol<$value_type>` plural. A walk with `each` now specialises its
  function in every build, `Spite` classes included (a trial keeping the value and the parts in fields of the
  singleton printed the same text in a plain and a `--hot-reload` build); what is left is that a walk function
  takes only the element, so the value and the parts reach it only through fields. Proposed by Claude, unconfirmed: a member whose name is already its own plural
  (`name_with_namespaces`, `bump_stars`) is collected by that name.
- Built (D335, D362): `function.accesses`, a dictionary of `Spite.Access` (`is_read`, `is_written`, `target`),
  attributes then arguments, following calls to the same class's functions, on a constant
  (`conformance/stage6/function_accesses`, reflection.md's `access_report`) and on a run-time function
  (`conformance/stage6/run_time_accesses`); a runner finds a component's `pinned_to_creating_thread()` through
  `access.target.class.functions[...]` (`conformance/stage6/pinned_components`). Proposed by Claude, unconfirmed:
  that order; calling a function on an attribute counts as reading it; the union's name `Spite.Access.Target`; the
  run-time table and the functions it leaves out. Not built, halting when asked at run time: a function of the
  standard library (listing them made every class object they mention, and in a `--hot-reload` build its reload
  check then differed); a function value taken through a `type` (its typed call is the shape's dispatcher); every
  function of a `--hot-reload` build (a reload would leave the table stale, and the reload check has no facts for
  what a function reads).
- Not built (D391): `Spite.FunctionDeclaration`. A class's `.functions` is still a list of `Spite.Function`s
  bound to a stand-in, so a walk over `Runner.functions` takes `Spite.Function`, and the program's entry class
  lists none. `Spite.AttributeDeclaration` is built: a class's `.attributes` are declarations with no value,
  folded and at run time (no stand-in is made for them any more), an instance's are bound `Spite.Attribute`s, and
  each kind given to a parameter of the other is an error.
- Not built: `Spite.Call` (D391, D393), which replaces `function.call_with`.
- Run time only through the old tables: `.owner` answers only on a constant; read on a run-time object it is "has
  no attribute" (it is the instance on a bound member, which waits for the split between declarations and bound
  members). `.element_type` and `.value_type` answer only on a constant too.
- A function whose body needs the reflection parameter's run-time value (passes it on to something that is not a
  reflection parameter of its own class, calls a function of it) is not specialised and walks at run time as
  before. A parameter of an enum type is not specialised (rule 5 of the proposal names enums too; open question
  in the R5 report: which calls it would cover).
- The old mechanisms still work side by side: `Symbol<...>` templates and plural walks, `Symbol<$T.f>` argument
  walks, name patterns, folder walks, `$T.has_function(...)` and the other name-keyed questions, `source_folder()`.
  Unconfirmed (proposed by Claude): specialisation names `<function>_for_<member>`, a constant local being a
  local never assigned again whose run-time value no statement needs, and a folded `assert` on a constant never
  being an "always holds" error.

### Functions of `Spite.Class`, and why there are no static functions
- The three override rules (the functions a class file can override are exactly those `Spite.Class` declares; an
  ordinary instance function whose name collides is a diagnostic naming `Spite.Class`; the override is evaluated at
  compile time and must fold) are NOT built. Today a function a class file declares is always an instance function:
  a class's own `has_function(name)` answers on its instances with no diagnostic, while `Gadget.has_function(...)`
  still answers `Spite.Class`'s; a class's `to_string()`, which `Spite.Class` also declares, is how its instances
  print. With `is_singleton()` now the `singleton` line, no class file overrides a function of `Spite.Class` today.
- Open, blocking the rules above (found 2026-10-02): `library/spite/class.spite` declares the reflection getters
  (`get_name`, `get_attributes`, ...), the old questions (`has_function`, `function_waits`, ...) and `to_string()`
  and `to_debug()`, so "a class file defining one of those names overrides it" would turn every class's own
  `to_string()` (how its instances print) and every `get_name()` getter into a class-object override that must fold.
  Which of `Spite.Class`'s members are hooks needs Mortaro: only those a reopening of `Spite.Class` adds; all but
  the members every object has and the reflection getters (those becoming reserved names like `attributes`); or a
  spelling that marks a hook.
- Reopening `Spite.Class` is only partly built. A program's `spite/class.spite` adds and replaces its functions,
  but that changes only what the class objects answer: `is_singleton()` returning `true` does not make
  `List<Integer>()` a singleton, since the `singleton` line decides that. A replacement that no longer reads
  `_singleton` gives "the attribute '_singleton' is never read: remove it" in `Spite.Class`, a private field of the
  library the program cannot remove; workaround: read it (`return _singleton or true`). `--final-classes` names
  no root yet for a changed default, though the page says it does.

### Removed from the page (not status)
- Pointers to conformance and diagnostics test names (`conformance/stage6/...`, `diagnostics/...`) and the
  sentence saying the rules win and the page has a bug to fix were dropped as maintainer notes.
- History dropped: the `Before, ...` notes on the function list order, on a class object's `.attributes` of a list
  (two C functions of one name, a game port's `--repl-port` build), and on a class test on a `Spite.Class` being
  silently `false`; `is_singleton()` being the first override via `console.spite`.
- The design hint "err toward more detail" for people designing reflection members was dropped.

## [packages.md](../docs/packages.md)

### `load` is a bundle boundary

- Not built: bundle splitting and lazy loading. Today every root is linked statically into the one executable, and a `load` line itself compiles to nothing at run time (except the launcher's `load build.program`). Splitting bundles into dynamic libraries, per-bundle tree shaking, and loading a bundle lazily when a `load` inside an `if` runs are decided but not built. The compiler records each root as a bundle (name, whether its `load` sits inside an `if`) in the program model for this to build on. Note the page also says a `load` under an `if` is decided at compile time, so lazy loading of a bundle named in an `if` and compile-time folding of that `if` are both described; the decision log should reconcile them.

### Two versions of one repository

- D334 (one package pinning two commits reads them as two loads in load order) was decided by Mortaro on 2026-09-30 and built; the error for a commit read into two different versions was proposed by Claude, unconfirmed.
- D296 (two versions are two libraries) was decided by Mortaro on 2026-09-30, built as proposed by Claude, unconfirmed.

### Final classes

- Not built: which root supplied each declaration is not printed. The folder is written and recompiles, but no `#` comment names a root yet, and an instantiated Symbol codegen function and a used `List<T>`/`Dictionary<T>` helper signature do not appear; only the source classes are declared.

### Packages, namespaces and loading

- Only `library/windows/` runs today; `check.sh` holds the `library/linux/` and `library/mac/` folders to compiling, by writing the compiler out once with each.
- Not built: a bundle linked anywhere but statically into the one executable (see `load` is a bundle boundary).
- Not built: the decided end state for compiler-supplied bodies (D147, D178). The bodies of the supplied members (`Console`'s raw writes, `Memory.Heap`, `Memory.Address`, `DynamicLibrary`'s opening and lookup, `TypedMemory`, number casts, the REPL hooks, `HotReload` facts, a `Concurrent`'s state machine, `ThreadPool`, `Scheduler`) are still C or generator-written, and printed bodiless. The compiler's own reopening is Spite source in `bootstrap/source/generation/prelude.spite`, beside the C each body is; a body written per instantiation (`TypedMemory<Integer>`, a number cast) is written by the generator.
- Provisional: the name `source_folder` (D228).
- Removed decision bookkeeping, all built: D7, D8, D38, D76, D80, D82 (Mortaro's quote about `--final-classes` showing the actual content of a class), D130, D155, D166, D177, D180, D181, D182, D186, D193, D195, D205, D211, D227, D244, D283, D293, D176, D178, D81, D147; and the "proposed by Claude, unconfirmed" labels on: the folder-name check scope, name resolution (relative namespaces, folder entry class namespace, nearest type, same-named types, namespaced generics), the unknown-type error and its suggestion (a `type` attribute of an unknown type crashed the compiler when the shape needed a default, fixed 2026-09-26), absolute `load` paths, `load` under an `if`, `source_folder`, the git load spelling and readings, the read-only copy's file comparison by size and modification time, and the bodiless `func` form.
- `check.sh` builds: a program that loads a folder by its absolute path; a program from a local repository pinned to a commit that is no longer its `HEAD`, from a second compile with no git work, with `--hot-reload`, and fails it on an unknown commit, without git on the `PATH` and after editing the copy.

## [concurrency.md](../docs/concurrency.md)

### (page-wide summary callout, removed)

- Windows runs all of it; the Linux and macOS folders (`library/linux`, `library/mac`) are only held to compiling and have never run.
- HTTP is not built.
- Cancelling a `Concurrent` is not built.
- D184's per-thread forms are not built (a buffer per thread for state that is only appended to, and splitting state that each thread touches its own part of). Its reader-writer form is built (D266, "Reading functions share the lock").
- A `Concurrent` made on a thread that is not the scheduler's is not supported as a concurrent: it runs on the spot instead.

### Concurrency: `Concurrent`, `Parallel` and hidden waiting

- Section was tagged implemented on Windows. The names and mechanism are decided; several details were only "proposed by Claude, unconfirmed": the written-handle-type rule covering only the declaration that starts the work (handle element types in `List<Parallel<Integer>>()` and parameters are still written), `finished`, `finished_value()` (name provisional, D216), `class`/`attributes`/`functions` staying a handle's own members, comparing handles (nothing has needed a way to compare the handles themselves), the debug text of handles, default-made handles, the `Concurrent` holding its function only while it runs, `Atomic<T>` (names provisional, D205/D214), and all of the ThreadPool, Lock, ThreadSlot and ThreadLocal shapes.
- `Concurrent` is described as for "IO, sleeps, database calls later": no database classes exist yet.
- Compiler-supplied members still have bodies written in C inside the compiler: `ThreadPool.entry_address()` and `address()`, `Concurrent._start_frame()` and `_frame_result()`, `Scheduler.step_frame(frame)` and `release_work(frame)`. D147 decides that no compiler-supplied function stays bodiless and no Spite body holds C; turning them into Spite over the backend's primitives is not built. The page now states the rule as if done.
- Thread pool, not tested: the Linux and macOS folders compile but have never run; a `Parallel` made on two non-worker threads at once before the pool has started (each could start it; the program's thread and a `Concurrent`'s helper are the only candidates).
- Waits that run the loop in place (right side of `and`/`or`/`==` on a nullable, through a function value, a union dispatch or a constructor, a `Concurrent` dropped inside a `Concurrent`): open as `mortaros_missing_decisions.md` item 179.
- Helper thread per blocking call versus each system's readiness (IOCP, epoll, kqueue): open for sockets, `mortaros_missing_decisions.md` item 180.
- `resume_only_when_asked()` has no way back ("proposed: nothing has needed a way back").
- A loop that polls `finished` and never calls `run_ready()` is not detected by the scheduler (documented, not caught).
- Untested gap: a REPL client's `exit` while a helper thread is blocked reading the console may wait on the C runtime's lock on that stream at process exit.
- `parallel_each_`: D35's race rule is built but still unconfirmed by Mortaro. D35's open questions (shared state across threads, cross-element reads) stay open. A list of a `type` or union is an error "for now" because the element's class is not known.
- Singleton thread safety was tagged `[partial]`. Built: forms 1 to 4 (nothing, nothing when unchanging, atomics, own lock), reader-writer lock (D266), counted-loop coarse lock (D265), lock skipped while no task is in flight (D267), loop-that-cannot-end and locked-wait errors (D211, D264), attribute-never-changes read in place (D271, unconfirmed). Not built: D184's buffer per thread for state only appended to, splitting state each thread touches its own part of; D183's compile-time check at `return` that a singleton hands out only numbers, text, copies or other safe singletons; guarding in a `--hot-reload` build; library singletons (`Console`, input, `Clock`) doing better by hand (which D183 allows).
- Locked-wait check (D264) does not see: a handle that escapes the function and is read by another locked function; calls whose class is unknown (function value, union, unknown receiver); work reaching the singleton only through its attributes from outside; `parallel_each_` passes; generic singletons (`Column<T>`). The exact wording of the error was proposed, unconfirmed.
- Not built: taking no lock at all where one `Parallel`'s work is provably the only thread touching a singleton while it runs (D207's handover extended to singletons). The game engine package also asked for no lock at all while every `Parallel` reaching a singleton only reads it, with writes between stages; not provable at compile time, so the reader-writer form was built instead.
- Reads in a row overlap: `Directory` listing is not a waiting call yet (no helper thread), so it is not overlapped.
- Task-may-keep-what-was-handed-over (D207): implemented, conditions and error text unconfirmed.
- Reads-in-a-row rule (D134's IO half) implemented; Mortaro's decision was "all our IO classes should use it", the reading of the rule unconfirmed.
- Removed wording: the page said "`check.sh` holds its C" for `conformance/stage6/singleton_reads` and `unshared_locks`; those conformance programs exist and their generated C is checked.

## [standard_library.md](../docs/standard_library.md)

### What belongs in the standard library
- WebSocket and TLS are decided as standard library members and are not built. Built: TCP and UDP over IPv4 and
  IPv6, HTTP/1.1 with kept-alive connections, SHA-256,
  HMAC, Argon2id, secure random bytes, base64, base64url, DEFLATE, zlib, gzip.
- The names of the hashing, password, random, base64, compression, UDP and HTTP/1.1 classes and functions were
  proposed by Claude and are unconfirmed by Mortaro.

### List a directory
- Built (D295, D330): `Directory.files()` and `Directory.folders()` are gone; a listing keeps one kind with
  `entries().filter_files()` or `filter_directories()`, and the compiler's discovery, the watchers and
  `scripts/docs_corpus` read names through `map_names()`.

### Read and write a file
- `File.map()` and `MappedFile` names and members are proposed by Claude, unconfirmed (D205/D214). Linux and macOS
  mapping (`mmap`) is held to compiling only; only Windows runs.

### Watch files and folders
- The quiet period is still open (mortaros_missing_decisions.md 166).
- Linux (`inotify`) and macOS (`kqueue`) watchers are held to compiling by check.sh; only Windows runs.

### Run a process
- `working_directory`, `environment_variables` and `run_attached()` names are proposed, unconfirmed.

### `Clock`
- The `Clock` names are proposed, unconfirmed.

### `Socket`
- Names proposed by Claude, unconfirmed. WebSocket is to grow from `Socket` and is not built.
- IPv6 (proposed by Claude, unconfirmed): `listen_everywhere` listens on `::` with IPv4 mapped in, falling back to
  `0.0.0.0`; `listen_locally`/`connect_locally` stay `127.0.0.1`; a name's IPv4 addresses come before its IPv6
  ones, so `listen_at("localhost")` keeps listening on `127.0.0.1` and `connect` tries IPv4 first (a refused `::1`
  costs Windows two seconds). Only the Windows build runs it; Linux and macOS (`IPV6_V6ONLY` 26 and 27, `AF_INET6` 10 and 30,
  the macOS `sin6_len` byte) are held to compiling.
- `Socket.drop()` now closes an open connection when its last reference goes; until now an unreferenced `Socket`
  kept its system handle open.
- The address helpers `Socket` shares with `UdpSocket` (`any_address`, `resolved_addresses`, `address_text`,
  `first_readable`, ...) are public functions of `Socket` with no reason for a program to call them (D241).

### `UdpSocket`
- Names proposed, unconfirmed. Sending to a name with both an IPv4 and an IPv6 address picks the IPv4 one (proposed by
  Claude, unconfirmed: a datagram cannot tell which one answers).
- `receive()` is not one of the compiler's waits: inside a `Concurrent` it blocks the program's thread.

### HTTP
- Names proposed, unconfirmed. Not built: TLS (HTTPS), WebSocket, request bodies sent chunked to the server.
- Keep-alive (proposed by Claude, unconfirmed): the numbers `idle_limit` 5000 ms, `largest_waiting` 64 and
  `largest_idle` 8, `HttpRequest.version`, and sending a request again once over a new connection when a kept one
  answered nothing (which can repeat a `POST` the server took before closing). The server reads one request at a
  time, so a client slow to send its request holds the others up.

### Bytes: base64, compression, hashes and passwords
- Class and function names proposed, unconfirmed. Removed clause: the `Argon2` defaults are those of the C# library
  the Unreal game stored its accounts with (64 MiB, 3 passes, 1 lane).

### System classes
- Only Windows runs today; the Linux and macOS folders of `library/` are held to compiling by check.sh.
- Open question 11 (open_questions.md): whether a `type` writes a required function as `to_string(): String`
  (current) or as `to_string: Spite.Function<String>` (Mortaro's original wording). Page states the current form.
- D147, not built: even `_write_output`, `_write_error`, `_write_held`, `_flush` and `_flush_output` should be Spite over a few named primitives.
- Proposal, unresolved (mortaros_missing_decisions.md): the REPL should show values as their `to_debug()` too; it
  keeps its own display (text unquoted, `Name {...}`, `List<String>(...)`) today.
- Names proposed, unconfirmed: the line-at-once flush of `print`, `error`, `debug`; `to_bytes()`; the Maths member
  names and constants; the `Console.debug` format details; the `Directory.Entry` name; all `Socket` readings.
- Not built: `fused_multiply_add`; waits for targets that name their processor.
- Not built (D346): choosing the flushing per destination. Every line is still flushed as it is printed, to a
  terminal, a file or a pipe alike; large buffers for files and pipes need a flush wherever the program waits (a
  sleep, a socket, a watcher, a `Concurrent`, a child process), or a server's log would hold its last lines, and
  the list of waits is K2's. Built: the three guarantees (a line is handed over whole by each thread, so threads
  never split each other's lines; everything is flushed before an exit, a crash report and a read of input) and
  `Console.flush()` leaving the public surface (D383).
- Proposed by Claude, unconfirmed (D???): `error` flushing the output first so the two streams keep their order,
  `Process.run_attached()` flushing first, and the private names `_write_held`, `_flush` and `_flush_output`.

### Pure Spite: dissolving the runtime
- Partial (the section was tagged partial). Still supplied by the compiler rather than written as Spite (D147,
  D178 target: zero hidden code): `Console`'s `_write_output`, `_write_error`, `_write_held`, `_flush`, `Program`'s and `Process`'s `_flush_output`; `Memory.Heap`'s `allocate`,
  `resize`, `free`, `live_allocations`; `Memory.Address`'s `copy_to`, `compare_bytes`, `text(length)`;
  `String.sum` and `String.code_at`; `TypedMemory<T>`; the number classes' bit operations; `DynamicLibrary`'s
  opening, closing and symbol lookup; the entry points and frames of `Concurrent`, `ThreadPool`, `Scheduler`;
  `HotReload`'s compiler hand-off; `Spite.Attribute`'s and `Spite.Function`'s dispatch. The goal is each of these as
  Spite (allocation from the operating system's pages, copying and comparing through `DynamicLibrary`) so no Spite
  source needs C. The reads, writes and atomics at an address stay language primitives.
- Per element type the compiler still writes four one-line functions a generic class cannot, plus `deep_copy`.
- Not built: the WebAssembly target and its web shim (imports from `external js` declarations, drain loop as a
  switch over an opcode table).
- Only `library/windows/` runs; Linux and macOS folders compile only.
- Removed anchor `pure-spite-dissolving-the-runtime--planned` (an explicit `<a id>` kept for old links).

## [collections.md](../docs/collections.md)

### Standard library metaprogramming
- Heading was tagged partial; the page did not say which part is missing. Known gaps found in the section: a
  `String`'s or a number's function can be passed to a form but cannot be held as a value yet (the compiler's error
  text says "cannot be held as a value yet"; that quoted error text is still in the page).
- `sort_by_`, `find_by_` and a program's own templates are not built for `Vector` or `Items` (only each_, map_,
  filter_, count_, any_, all_, sum_, parallel_each_).
- The "a `while` that only does what a template does" rule (control_flow.md) does not look at a `Vector`'s loops yet.
- Names still provisional (proposed by Claude, unconfirmed by Mortaro): `Items`, `remove_swapping`, `remove_where`,
  `truncate`, `swap`, `first()`/`last()` answering `T?`, `reserve`. The page used to say "name provisional".
- Proposed by Claude, unconfirmed, removed as bookkeeping but the behaviour stays on the page: how the templates are
  written, chains fused by one generated function, passed-function details (library-class functions bound as
  values), the dictionary key-kind rules, the Vector and Items readings.
- D369 is built: `map(function)` on a list, an `Items` or a `Vector` is an error naming `map_<member>()` and a
  read-only attribute; `map_` over a `Boolean` member names `filter_`, `count_`, `any_` and `all_`; a class-qualified
  function value (`Monster.is_alive`) names an instance's function. The error texts are proposed by Claude,
  unconfirmed. A `while` collecting what a passed function answers is no longer reported by the loop rule.

### Member templates over an enum value
- Built (D295): `filter_<value>`, `count_<value>`, `any_<value>`, `all_<value>` and `remove_where_<value>` over a
  named enum's values, on a class, a union and a dictionary's values, with the three errors
  (`conformance/stage6/enum_value_templates`, `diagnostics/enum_value_templates`, collections.md's `post_stages`).
  A union member answered by an attribute in one class and a getter in another is read through each (it was a C
  compile error, "'Dog' has no member named 'mood_'"). Not decided, so not built: `count_<classes>()`,
  `any_<classes>()`, `all_<classes>()` and `remove_where_<classes>()` by member class on a list of a union (D330
  names only `filter_<classes>()`).
- Open question for Mortaro: `Spite.Namespace`'s `.classes` and `.namespaces` have the same shape (one node's
  children split by kind) and may get the same treatment; undecided.
- Rule details (which templates, how a name is read, error texts) are proposed by Claude, unconfirmed.

### Vector<T>
- Not built, removed from the page: an allocator's effect on the block of items. `velocities.memory.allocator =
  arena` places the `Vector` object in the arena but the block of items stays on the heap, because a class cannot
  read its own `.memory` yet. Open: the proposal (mortaros_missing_decisions item 175) that `memory.allocator` is
  readable inside a class, so the block follows the object, is unresolved.
- The class named `Vector` in `examples/vectors`, `conformance/stage6/operators` and `benchmarks/small_allocations`
  was renamed `Displacement`; page no longer records this.

### Deep copy (Dictionary<T> rules)
- `deep_copy()`: a union or a `type` shape is shared rather than copied "for now", and a self-referring structure is
  still unsupported. The page states both as the rule without the "for now"/"still".

### Rules in full
- The page no longer says its rules were moved from the language manual when D193 dissolved it, nor that a `D`
  number is a row of the decision log.

## [json.md](../docs/json.md)

### JSON is reflection, not a library
- The section was tagged implemented; nothing is missing. All four classes, the schema hash, the camelCase and
  PascalCase keys and `json_key_<attribute>()` are built.
- The page links to targets.md "The wire format", which was tagged planned: the wire format is not built. The
  binary classes are the building block for it.

### The binary format
- Little-endian everywhere: the page said "WebAssembly when that target is built". The WebAssembly target is not
  built; a big-endian target would need byte swapping in `BinaryOutput` and `BinaryInput` (still stated on the page).
- The schema hash name `schema()` is provisional (proposed by Claude, unconfirmed); the page keeps a note "the name
  is provisional" in the rules. The handshake that would carry the hash is not built (it belongs to the wire format).

### Reading input you did not write
- Member names `attribute.camel_case_name`, `attribute.pascal_case_name`, `json_key_<attribute>`, `append_to`,
  `read_memory`, `reserve` were proposed by Claude and never confirmed by Mortaro (names are his to pick later).

### Rules in full
- Left in place, a compiler error quote containing a decision number: the `there is no 'Json': it is two classes
  (D208), ...` message (`diagnostics/json_split`). Change it together with the compiler.
- Removed history: the old `Json` class was split into `JsonWriter` and `JsonReader` by D208; `Vector<Byte>` was
  folded into `List<Byte>` by D225.

## [time.md](../docs/time.md)

### (page introduction, removed paragraph)

- The model is D127's and the type names are Mortaro's (D158, D160). The rest of the shape (members, ambiguity rules, where the zones come from) is proposed by Claude and waits on Mortaro. Open: the constructors, `now()` and the lenient reading of text, as `mortaros_missing_decisions.md` items 118, 120 and 121.

### Where zones come from

- The Linux and macOS path (TZif files under `/usr/share/zoneinfo`, `library/tzif_reader.spite`) is written but has never run on Linux or macOS; `check.sh` only holds it to compiling (it writes `conformance/stage6/daylight_saving` for each system). `conformance/stage6/zone_files` runs the TZif reader on Windows over three test files checked with Python's `zoneinfo`.

### What time does not cover

- Not built: calendars other than ISO 8601's, leap seconds (a leap second reads as the second before it), formatting patterns and localized names, zone abbreviations (`EST`). The page heading was "Not here yet".

### Time: one stored instant, zones for presentation

- Section was tagged implemented on Windows; the shape is proposed by Claude and unconfirmed (see the first bullet for the open items). Only the model (D127) and the type names are decided.

## [game_maths.md](../docs/game_maths.md)

### Game maths
- The whole section was tagged implemented; nothing is missing. Open point only: the names and conventions (layout,
  column-major, Vulkan clip space) were proposed by Claude and never confirmed by Mortaro (decision D214 says he picks
  names later). The page states them as decided.

## [foreign_libraries.md](../docs/foreign_libraries.md)

### Each operating system reopens what it changes

- Only `library/windows/` runs today. `check.sh` holds the Linux and macOS folders to compiling, by writing the compiler out once for each.

### What the compiler supplies

- Not built: the bodies of the members the compiler supplies (opening a library, finding a symbol, `Memory.Heap`'s allocation) are still C written in the compiler, declared in Spite without a body. The page states the decided end state (decisions D147, D168, D178): these bodies become Spite over the few operations each backend lowers. Not built for these three.
- Not built: the naming rule and the calls as reopenable Spite (`missing_function` and `missing_attribute`), a naming rule of your own, reading a header's types through reflection, and C's variadic functions.

### Foreign libraries

- Not built: a C `enum` status answered as a Spite enum the binding writes, with Spite names mapped to C numbers and an unlisted C value crashing at the boundary (D351, replacing D272's enum made from the header), with D272's result-must-be-used and no-comparison-with-a-number checks. Today a call answers an `Integer`, and `crash result == 0` compiles, which is the bug these close; a binding maps the number to its enum by hand, as the page's `Packer` does.
- Not built: reading a header's types as Spite reflection (`user32.Input`, a PascalCase read, is the C type `INPUT`); `missing_function`/`missing_attribute` as reopenable Spite; a user-written naming rule; C's variadic functions. Callbacks are built.
- Not built: the `DynamicLibrary` class as the page shows it (`symbol_name`, `missing_function`, `missing_attribute`, `_open`/`_call`/`_resolve`/`_close`). What exists (D81/D82): `library/dynamic_library.spite` is the `singleton` line, `file_name` and `handle`, a constructor that stores the file and calls `open_library(file)`, and a `drop()` that calls `close_library(handle)`. `open_library`, `close_library` and `find_symbol(name, wanted_by)` are the compiler's reopening, declared without a body, their C written in the compiler: the stopgap D147 rejects, not yet replaced. The naming rule and the calls themselves are the compiler's, and every call through a `DynamicLibrary` value is a foreign call, since `remove` and `exit` are names both a class and a C library could have.
- `--final-classes` writes no resolved-name comments for bindings (the original D4 design printed each binding with its resolved name and library as a `#` comment; D34 would reject it). How to show the mapping is open question 10.
- The decisions D4, D8, D34, D70, D71, D314 (one instance per distinct literal arguments) and the "proposed by Claude, unconfirmed" labels on the naming-rule argument errors, `_as_long`, the header include rule, the `_Static_assert`, `Type.size` (and its error text), the argument-width rule, the native-fault store, and the reflection members exemption were removed from the page; all are built.
- Not built: naming the offending field in the error for a `type` with a `String`, `List<T>`, `Dictionary<T>` or class field at a foreign call; the error names only the type.

### Callbacks

- Names are provisional (D214): `ForeignCallback`, `where_context`, `'no_context'`, `'context_first'`, `'context_last'`, `address`, `context`. Decided by Claude under D205 (D231 to D234), not confirmed by Mortaro.
- Not built: checking a trampoline's widths against the header's declaration (C has no way to name a declared function's parameter types, and the compiler reads no header itself); a `Boolean` as C's one-byte `bool` (workaround: write a `Byte`); an enum value or a struct passed by value to a callback; a function value handed to C inside a `type` (workaround: write the `ForeignCallback`'s `address` into a `Long` attribute instead).
- Not built: the check at `return` that D183 leaves unbuilt for a `Parallel` is unbuilt for callbacks too.
- The page says the handover a `Parallel` allows (D207's handover) is not offered for callbacks; the page's wording "the handover a `Parallel` allows" paraphrases D207, check it.
- `check.sh` greps `examples/hello`'s C for any trampoline, `ForeignCallback` class or `SPITE_THREADS` (the cost claim).

### What foreign calls do not cover

- Section was titled "Not designed yet": a C union past its first member (positional layout reaches the first member only; the rest needs C's own field names); varargs, structs returned by value, and `#define`s that are function-like macros rather than constants.

## [targets.md](../docs/targets.md)

### Whole page
- Nothing on this page is built. The page opened with "None of it is built yet"; every section was tagged
  planned. Built: only the binary packing (`BinaryWriter`/`BinaryReader`, which a program can already send over a
  `Socket`), see json.md. Not built: the `--target=native|web` flag and the `Build` target field, wasm output and the
  JavaScript glue module, `platform_web/*` reopenings, `Html` and `Html.Node`, `Spite.Class.targets()` and the
  wrong-target check, the `ServerContext`/`ClientContext` bundle split and generated stubs, routes, the markup
  builder, and isomorphic classes sending the wire format on their own.
- Only operating-system choice is built: `load "library/{build.target_operating_system}"` and a `load` under an `if`
  on a `Build` field fold while compiling (foreign_libraries.md).

### Other environments
- Open: the `Build` target field's name is not decided; `build.target` is a placeholder (proposed by Claude,
  unconfirmed). The page now writes it as if it were the name.
- The page said `$target` program variable ended and compiler options became `Build` fields (D76, D85).

### Isomorphic classes: where a function lives
- Open: how the client stub's signature is spelled (a function returning `T` answers `T?`) is not decided.

### Html: the browser as a library
- Open, removed from the page: the markup builder below was sketched as `html.div(...)`, which collides with the
  `Html` class. It needs another name (`Markup`? `View`?); Mortaro has not picked. The Markup section's example
  still writes `html.div(...)`.

### The wire format
- Proposals by Claude, unconfirmed, removed from the page: a schema hash in the handshake, so an old client meeting a
  new server crashes with a clear report instead of misreading bytes; and `--development` decoding payloads to JSON
  on demand. Not built, not decided.
- Not built: isomorphic classes sending the packed bytes themselves (the packing is built).

## [compiler.md](../docs/compiler.md)

### Choose the outputs
- Proposed by Claude, unconfirmed (D348 built): `--check` with `--build` is an error; `--executable` went with `--run` (`--build` is the one way to build without running); `--check --final-classes=folder` writes the classes and nothing else; a build removes the old executable before compiling, and when the program cannot be read it removes the one at the command line's path or the default one, so a program whose own `build.spite` moves `executable_path` keeps its old executable after a read error.
- Not done: the committed seeds predate `--check` and `--build`, so `bin/spite` cannot build a program until they are written again (`check.sh` puts generation 2 in its place meanwhile); `bootstrap/build.spite` and the old branch of `check.sh`'s `compile_compiler` go with the next seed ([self_hosting.md](self_hosting.md#the-seeds-own-flags)).

### Inspect merged classes
- Not built: `--final-classes` output does not show which root supplied each declaration. It cannot be a comment (a comment is only ever a link to a markdown heading), so it needs a form of its own (open question 10, open_questions.md). The paragraph saying so was removed from the page.

### Rules in full
- Removed the history note that these rules were moved whole from the language manual when D193 dissolved it, and the maintainer note that a disagreement between teaching and rules means the page has a bug.

### Outputs, Flags and settings, and other rules (decision status removed)
- Decided by Mortaro, recorded only as decisions: D143 (inspectable versus production builds), D188 (kebab-case flags), D190 (every compile formats first), D283 (outputs in `.spite/`, "the compiler shouldn't write intermediate files beside the source"), the default build's `-O0`.
- Proposed by Claude and still unconfirmed by Mortaro (the page now states them as the rules): the spellings of the output fields (D128, D129); the `.spite/build/<path>` and `.spite/elsewhere/<name>_<number>` layout; `.spite/` never being part of a program (D283); the unit count rule (768 KiB per unit, power of two, at most 64, which D349's "chosen from measurements" keeps as measured on one machine); `optimized` as `-O3` with `-flto=thin`/`-flto=auto`; the readings of the flag rules (kebab form in messages); the inspectable-build readings; where a program runs; the launcher passing arguments untouched (`cygpath -m`, `MSYS2_ARG_CONV_EXCL`; fixes a bug found converting a game's data); how many errors are listed (A94, from a game port); the formatting readings of D190. D260 (unique `<name>_<number>.c` and the "C compiler reported success but no executable" error) was decided by Claude under D244.
- Removed the history that `.spite-cache/` was the name of `.spite/` before D283 (the compiler still skips an old `.spite-cache/` folder; the page now says "an old `.spite-cache/` folder, which can be deleted").
- Removed check.sh mentions: it proves the fixpoint by comparing the single-file C; it runs every program of `conformance/`, `examples/` and the docs pages with `--debug-memory` and requires the allocation balance; it uses `--target-operating-system=linux` to hold the Linux folders to compiling; it proves the `--final-classes` output runs as the same program; it runs `conformance/stage6/working_directory` from another folder and a copy from inside it, and `conformance/stage6/launcher_arguments` through `bin/spite` with `--prefixes=/Game/Legacy/`, `/usr/share` and `a b` beside the compiler's flags; it compiles the deliberately unformatted `diagnostics/` inputs from a copy so formatting lands on the copy; `.spite/` also holds check.sh's work folders in the language repository.
- Removed "the one the corpus runs" (the default `-O0` build is the one the corpus uses).

## [repl.md](../docs/repl.md)

### REPL and live reload (introduction)
- The old "What is built" box said live reload runs on Windows, and Linux and macOS are only held to compiling (their watchers, `.so`/`.dylib` libraries and `Program.executable_path` are untested at run time). The same applies to the whole page.
- The old page said typing new Spite code at the prompt is not built; it has since been built as `eval` and `run` (needs `--hot-reload` and `--repl-port`).

### What a reload can change
- Changes to whether a class fits an `Items`' own memory are refused (naming the class). Decided direction: nothing may need a restart, so moving these is to be built.
- The rows for library code, `environment.spite`, `build.spite` and a deleted file are built (D333); how settings are read again after a reload was proposed by Claude, unconfirmed.

### Nothing needs a restart
- Decided (Mortaro, D280): nothing needs a restart; every refusal is a gap to close. Built: step 1 (attributes move), step 2 (dependents rebuilt through a whole-program compile, every function slotted, D333), step 3 (enums keep their numbers), step 4 (`environment.spite`, `build.spite`, D333), and a change to a value class's functions. Not built: moving objects of a class that starts or stops fitting an `Items`' own memory.
- The rename map (D329) is built as D333 describes; the prompt's `reload {Class.attributes['old']: "new"}` form is read as text by the REPL (proposed by Claude, unconfirmed) until the prompt evaluates maps and `attributes[...]`.
- The rules of step 1 were proposed by Claude and are unconfirmed by Mortaro; the enum rule (D302) was decided by Claude under D205, not by Mortaro.

### Command language
- Decisions for the prompt's features (singletons by name D286, breakpoints D303, `bytes` D291, `eval`/`run` D305, `describe`/`enums`/`memory`/union walking/assignment D301) were decided by Claude under D205, not Mortaro. The command names `break`, `breaks`, `clear`, `where`, `locals`, `continue`, `bytes`, `eval`, `run`, `describe`, `enums` and the three `Spite.Attribute` members (`held_memory`, `stored_memory`, `buffer_memory`) are provisional (D214).
- Not built: `Dictionary<T>`'s `keys()` and `has(key)` at the prompt, and assigning an object made at the prompt (`follower = Circle(3)`); workaround for the second: `run follower = Circle(3)`.
- A text literal assigned at the prompt has no escapes yet.
- Reading `Spite.Class.functions` anywhere in the library changes how every program is compiled (a bug recorded in failure.md under "nothing fails silently"), which is why `describe` uses `class_descriptions()` instead.
- The wire format being JSON is not a promise (D96); the format may become whatever an AI client reads best, binary included.

### Live reload in detail
- Decisions D111 (only what changed is rebuilt), D112 (`--hot-reload` as its own flag), D211 (helper-thread compile, reflection follows the reload, watched loaded folders), D283 (library directory `.spite/build/`, pinned checkouts not watched). Mechanisms proposed by Claude, unconfirmed: `wait_reload`, `spite reload` command, compile-only-changed-classes, `Concurrent` not overlapping in a `--hot-reload` build (D176), the manifest contents, one reload at a time (a compile waits for the library before it to be swapped in, also while the program is stopped at a breakpoint).
- Linux and macOS are untested at run time: only held to compiling by `check.sh`, written the same way as Windows'.
- History removed: `spite reload` was a `--mode` before D128; a no-change reload used to compile the whole program twice (90 seconds for the large game); class ids were once seeded by display name, so an enum named like a class (`JsonSymbols.Color` beside the library's `Color`) took that class's id; a changed file declaring none of the program's classes used to be answered `unchanged` and silently not applied; the compile error for a loop that never waits and `diagnostics/remote_loop_never_waits` were removed with D174; the REPL used to listen before `HotReload` started, and since a file read is a wait it answered commands while `HotReload` read its files, so a `reload` found no state (a fault at `address=0x8`) and a `wait_reload` took a save made then as compiled; a reload used to compile against the files of the code before a swap still on its way, and to read `.reload_files` while the swap rewrote it, so it compiled a change twice or took no file as compiled and answered `wait_reload` before the watcher had seen the save.

### `--repl-port=<port>`
- `check.sh` replays every `wire` block in `docs/` (via `scripts/docs_corpus.spite`): builds with `--repl-port`, sends each `$ spite connect ... --command` line, compares each answer with the line under it, requires exit code 0 after `exit` and the program's output to equal its `output` block. Removed from the page as a maintainer note.
- Check-point design (D174) was decided by Mortaro; the mechanism was proposed by Claude, unconfirmed. `check.sh` also checks that a build without the flags emits byte-identical C (busy loop and `examples/dungeon`).

### Breakpoints
- `check.sh` stops a ticking loop, reads its locals and lets it go (D242 is the decision).

## [testing.md](../docs/testing.md)

### How testing works
- Nothing else is unbuilt. The decisions behind it (D46 testing is a package that crashes, D49 `Spite.Class.instances`) were removed from the page as bookkeeping.

## [optimizations.md](../docs/optimizations.md)

### Whole page

- The page had a Status column and "Built" / "Planned" parts. Every optimisation on the page is built except: the planned forms of "Thread safety for singletons, the rest of the plan", most of "Copies that cost nothing" (only the pieces listed below are built), and the three items of "Other optimisations". The partly built ones are listed below. The old "Planned" intro said: decided by Mortaro, not built yet; when one is built it moves up into the built part in the same change.
- The old "Adding one" section (removed, maintainer note) said: every optimisation the compiler starts making is added to this page in the same change, with what it does, when, whether it is built and what a user could notice (D185, D102); an unremovable cost is written down; one that contradicts the rules on another page is recorded in `mortaros_missing_decisions.md`.
- Every optimisation whose old status line read "proposed by Claude, unconfirmed" still awaits Mortaro's confirmation: tree shaking of classes, slots and statics; native symbol lookup only when reached; reads in a row overlap (and its scheduler cost); the lock forms (padding, unlocked calls to itself, locked writes from outside, read-only held objects, no lock for functions that touch no changing state); the thread-safety forms and their order and the one-touch rule; the skipped lock while no task runs (D267); the readers' side (D266); the counted loop's single lock (D265); held arguments (D270); reading a singleton's unchanging attribute in place (D271); text joined in one piece; discarded defaults; function values describing arguments on demand; list templates reading uncounted; numbers written in place into text; dictionary hashing; number-keyed dictionaries (decided by Claude under D205, readings unconfirmed); reading through a `type` uncounted; borrowed rows in the frame (plus the Symbol walk, sparse rows, lent list elements, lent arguments, lent items passed to calls); `Items` storage; short text (the size 15 and the layout); proven reads (D225, D277); the walked `crash` read; frame objects (decided under D205/D214); parallel translation units and the object cache; `-O3` with ThinLTO; identical function folding (how, and function-value equality).
- The default `-O0` build was "Mortaro's choice to keep" (recorded as a decision, not a reason).

### Deciding conditions at compile time

- An `assert` or `crash` whose condition is a `Build` field or a class test on a value is not folded: it is tested at run time. So a `crash` on a `Build` field that is false halts when it runs instead of being a compile error like a folded `crash`. Whether it should fold is still open (D250; `failure.md`, "nothing fails silently").

### Hidden async/await as compile-time state machines

- Open question (`mortaros_missing_decisions.md` item 180): whether sockets should move to the system's own readiness (IOCP, epoll, kqueue) instead of a thread per system call in flight. The page describes the current design (one event, a helper thread per call).
- Open question (item 179): a `Concurrent` whose function cannot be a state machine runs to its end when started, including any function of a `--hot-reload` build (called through a slot a reload swaps); the question is whether that should change.
- Proposed by Claude, unconfirmed: the order in which waits inside an expression run, and which waits fall back to running the event loop in place.
- The fibers that came before the state machines, and each system's code for creating and switching them, are gone.

### Singletons a `Parallel` reaches take a lock

- Not built: the check that a locked singleton's functions hand out nothing they own (the page states it as a rule).

### Thread safety for singletons, the cheapest safe form

- Built: nothing because no `Parallel` reaches it, nothing because it never changes, atomics for counters and flags, and the lock as the fallback. The remaining forms are not built (see the "rest of the plan" section below).
- `--hot-reload` builds are not guarded at all yet: no lock and none of these forms is applied there (the page lists `--hot-reload` as excluded for the lock and the forms).

### Thread safety for singletons, the rest of the plan

- Not built: state that is only appended to (a log, a command queue) gets a buffer per thread merged in order.
- Not built: state each thread touches its own part of is split per thread.
- Not built: the compile-time check at `return` that a singleton's functions hand out only numbers, text, copies or other singletons made safe the same way.

### Copies that cost nothing

- Built: a copy only used as a value is a frame slot (classes of numbers, `Boolean`s, enum values, singletons); an allocator set right after construction; leaving out the count on a frame object passed by name to a program function that never assigns the parameter (D270).
- Not built: frame objects of classes that hold text, lists or other objects (their attributes let go at the end of the frame); an attribute object laid inline in a frame-held object where the attribute is never shared.
- Not built as general rules: a copy used only once passed by value instead of allocated; a copy that is never changed sharing the original when cheaper; every object that never escapes laid inline or in registers; reference counting left out wherever ownership is provable (only the frame-object and held-argument cases above exist).

### Identical functions are folded into one

- Built (D296, D340). Proposed by Claude, unconfirmed: the normalisation details (layout equality by attribute order and type, numbered temporaries, texts by content, a site shared by two versions or instances reporting the first one met); function values of folded functions comparing equal, with the alternative that folding keeps a function apart when the program compares function values; no folding in `--hot-reload` builds and the REPL.
- Not folded yet: a function that differs only in which class of another layout it passes around by reference (a `List<A>` and a `List<B>` of two unrelated classes whose items are only retained and released); a boxed text constant (`spite_lit_N_box`) compares by its name, not its text; a site of a foreign callback still writes its place into the function.

### Other optimisations

- A list's buffer in its list's allocator (D154): today a `List` given an allocator is made there, but its buffer of references still comes from the heap, and so does a `Vector<T>`'s block of items. Not built.
- An appended item made in place: today `var slow = Velocity(1.0, 0.5)` then `velocities.append(slow)` makes an ordinary object, copies its attributes into the vector's block and lets the object go, so filling a vector allocates once per item for a moment. Writing the constructor's attributes straight into the block is not built.
- A build report of what could not be optimised (D36) is not built.

### Arithmetic is checked in every build

- Not built: leaving out the overflow check where a proof bounds the operands other than a counter stepped by one
  under a `<` or `>` and constants (an index already bounded, a `bits_and` mask put into a `Byte`, an attribute).

### Objects that never leave their function live in the frame

- Objects of classes holding text, lists or other objects are not placed in the frame yet (see "Copies that cost nothing").

### A loop over plain values reads its count once and its items unchecked

- Open question (`mortaros_missing_decisions.md` item 210): a `Float` expression with a decimal literal is worked out in `double` precision in the C, which halves the vector width. In `float` the two benchmark loops would take about 100 and 70 microseconds, but some results would change in their last bits. Not done; waiting for Mortaro.

### Allocation is the C library's, counted only where read

- Built and taken out again (2026-09-25): keeping each freed object of up to 256 bytes on a per-thread free list for reuse. Not a clear, repeatable gain on the game engine package; `benchmarks/README.md` has both sets of numbers. The page keeps the measurements as a description of what the compiler does not do.

### A `--hot-reload` build, reloads

- Maintainer note removed: a fact a future change to the generator starts consulting across classes must be recorded in the reload manifest too, or checked by `SPITE_RELOAD_CHECK` (a fast reload that misses one would swap in code compiled against a stale fact). `check.sh` compares the result of a reload with a whole compile file by file (`SPITE_RELOAD_CHECK`).

### Links and anchors

- Outgoing anchors that change when other pages are cleaned: `memory.md#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed`, `memory.md#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-a-vectors-block-planned`, `metaprogramming.md#codegen-values---implemented`, `collections.md#vectort--implemented`, `standard_library.md#maths--implemented`, `values_and_types.md#numeric-types--implemented-provisional`, `failure.md#reading-with--answers-t`. `failure.md#nothing-fails-silently--the-rule` was removed with the status sentence.
- `check.sh` greps of the C, removed as maintenance notes: `examples/hello` carries no struct, allocate or singleton slot of unused library classes; `conformance/stage6/singleton_stateless_calls`, `singleton_lock_calls`, `singleton_forms`, `coarse_locks`, `held_arguments`, `singleton_attribute_reads`, `lent_list_elements(_parallel)`, `division_by_zero`, `counted_loops` are read by `check.sh` from the generated C.

## [proofs.md](../docs/proofs.md)

Every proof entry on the page was marked "Built" except the ones listed here. The "Status" line was removed from every
entry, and so was every "Compiler today" line (a difference between the rule on the page and the compiler); both are
recorded below.

### A note on the whole page

- The intro said the page is the catalogue of every proof "built, partial or planned" (Mortaro, 2026-09-28, D276), and
  that an entry and the page that teaches it differ, the page decides (D193). Both remain as plain statements, without
  the status words or decision numbers.
- Section "Adding a proof" was removed (a note to maintainers): a change that makes the compiler prove something new,
  or changes what an existing proof accepts, drops or refuses, updates this page in the same commit (D276): the entry,
  its fallback, its status, and the program that shows it; the rule itself goes on the page that teaches that part of
  the language; a proof that is also an optimisation is described on optimizations.md too (D185); a proof that
  contradicts a rule on another page is recorded in `mortaros_missing_decisions.md` for Mortaro.
- The page has no "Rules in full" section, so the closing link follows the last proof section.
- Links to other pages with status words in their anchors are kept as they were and need fixing when those headings
  change: `values_and_types.md#numeric-types--implemented-provisional`,
  `memory.md#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed`,
  `memory.md#borrowed-items-of-a-vectort--implemented`, `metaprogramming.md#codegen-values---implemented`,
  `json.md#json-is-reflection-not-a-library--implemented`.

### Arithmetic that does not fit halts

- Built for a local counter stepped by one under a `<` or `>` and for constants; other range facts (bounded
  indexes, masks, attributes) do not remove the check yet (see optimizations.md, "Arithmetic is checked in every
  build").

### A list's templates read their elements uncounted

- Partial, a known hole: the proof does not consider the `drop()` functions that run when an object is let go, so a
  `drop()` that removes from the list being walked could free a lent element in use. The proof for "A list element
  lent to a row" (D269) does check `drop()`; this one should. Listed as still open in failure.md ("Nothing fails
  silently").

### A lock that would wait forever is an error

- Partial: refused only where the compiler can prove it. Not seen (and stated on the page as the fallback): a handle
  that escapes (stored, switched on, put in a literal), work reached through a function value, a union or an unknown
  receiver, a `parallel_each_` pass, and generic singletons.

### Which calls suspend a `Concurrent`

- Built on Windows only (the status read "Built on Windows"); no statement was made about other targets.

### Proving what is proven is an error

- Built; nothing missing. (Status cited D279: every fact the compiler holds.)

### A proven read halts outside its list

- Built. The entry's status said "proposed by Claude, unconfirmed" (D225, D244): the halting behaviour itself is not
  a decided rule.

### A buffer freed in its block lives in the frame

- The compiler accepts `read_value`, `write_value`, `release_value` and `swap_values` by name on any receiver, not
  only `TypedMemory`'s, so a program's own `write_value` that keeps the address would pass. Still open in failure.md.
- The memory.md placement rule it links was "the rule proposed by Claude, unconfirmed".

### Objects that never leave their function live in the frame

- Not built: frame storage for a class holding text, lists or objects, their attributes let go at the end of the frame
  (see "Frame objects holding text, lists or objects" below). The fallback line said "for a class holding text,
  lists or objects (planned)".

### An `if` that leaves proves the rest

- "Leaving" here is narrower than "every path ends in a `return`", which also counts `if`/`else` and `while true`.

### A call keeps a proof it cannot change

- A proven divisor is kept as a list-like fact, so a call that may shrink any list (through a function value) undoes
  it too.

### A copy made to narrow is an error

- D63 says "which is then narrowed", in any way; the compiler recognises only a condition that is exactly the bare
  name, so a copy narrowed by `if not copy { return }`, `while copy` or `assert copy and ...` is not caught. Still open
  in failure.md.

### Every path ends in a `return`

- `while true` ends a path whether or not its body can leave, and nothing reports such a loop outside a locked
  singleton function; statements after a `return` in the same block compile without an error. Both still open in
  failure.md.

### A guard `assert` answers only "nothing"

- The lint for an `if` that only returns the default recognises only the literal defaults `null`, `false`, `0`, `0.0`,
  `""` and a bare `return`, so `if ... { return List<T>() }` in a function answering a `List` is not refused, and a
  function that lends a list element (D269) is not checked for a guard `assert`. Still open in failure.md.

### A switch covers every case

- The union message reads "has no case for File: every member is covered, so a member added later cannot be
  forgotten", which says the opposite of what it means. Still open in failure.md. (This is compiler error text; another
  agent changes the message.)

### Conditions decided while compiling

- A `Build` field in an `assert` or `crash` is not folded (only codegen questions are), so the rule "a `crash` that
  folds false is a compile error" (D250) does not apply to it. Still open in failure.md.

### Whether a function writes a parameter

- A function whose body the compiler supplies counts as writing only when its name is on a fixed list (`resize`,
  `free`, `copy_to`, `write_value`, ...) or starts with `write_`; any other supplied function answers "does not
  write", a silent `false`. Still open in failure.md. This analysis decides no memory layout; the memory proofs use
  their own.

### Whether a class keeps state

- A supplied function outside the fixed list above answers "does not write", so the same silent `false` applies.

### Planned proofs (section removed)

The section listed proofs that are not built. Each, with what it said:

- A foreign function's status is handled while compiling (D272, decided by Mortaro; design proposed by Claude,
  unconfirmed): a C enum result becomes a Spite enum that must be switched over. Not built: today a call answers an
  `Integer` and `crash result == 0` compiles (foreign_libraries.md, "Foreign libraries", partial).
- An overflow check left out where a range fact other than a counter's bound or a constant bounds the operands
  (optimizations.md, "Arithmetic is checked in every build").
- A write to a copy that dies unread is an error (proposed by Claude, unconfirmed): escape analysis already proves a
  result fresh (failure.md, "Nothing fails silently", still open).
- Frame objects holding text, lists or objects, their attributes let go at the end of the frame (optimizations.md,
  "Copies that cost nothing").
- A singleton hands out only safe values (D183's check at `return`), and D184's per-thread forms (optimizations.md,
  "Thread safety for singletons, the rest of the plan").
- Whether a function runs in pieces (D229, `function_runs_in_pieces`): decided, and on no page and not in the
  compiler.
- A read overlaps the statements after it until its name is used (D211, item 149): decided, not built.
- A class compiled for the wrong target (D20): planned (targets.md).
