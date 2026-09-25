# Spite compiler plan

Source of truth for language design is `manual.md`. This file tracks implementation milestones and decisions
Source of truth for language design is `manual.md`. This file tracks what the compiler still has to build.
The compiler is written in Spite and compiles itself, emitting C.

## Layout

```
bootstrap/bootstrap.spite        entry: the compiler's own entry class, Bootstrap
bootstrap/source/                the compiler, in Spite
bootstrap/seed/spite_compiler.c  the committed fixpoint C; a C compiler turns it back into the compiler
bootstrap/COMPILER_PLAN.md       the compiler's own plan and progress log
library/                         Spite.Class, Spite.Attribute and the rest, as ordinary Spite source
conformance/                     programs the compiler must handle exactly, checked by check.sh
examples/                        sample programs, each run by check.sh against its expected output
tests/                           the test package: test_ functions whose only assertion is crash
docs/                            language documentation
```

Everything the compiler does is proved by `bash check.sh`: it rebuilds the compiler from the seed, requires
generation 2 and generation 3 to be byte identical, and runs every conformance program requiring exact output
and balanced allocations.
## Milestones

10. Ruby grade reflection and reopening the standard library. Split in two:
   - **10a. Done.** Reopen standard library classes (manual.md section 16 item 1): the way to try out a
     package before upstreaming it. Reopening `Spite.Class` is D7 (manual.md section 8, "Class-level
     functions") -- a root overrides a class-level default program-wide, a new hook name creates a new hook
     program-wide, and `--final-classes` shows each change with its root, so nothing about it is silent.
     Smaller than 10b (reopening is already implemented for user classes), this mostly needs the "library"
     classes to participate in the discovery pass's merge pass.
   - **10b. Done.** Its design pass lives in `bootstrap/COMPILER_PLAN.md`, "Milestone 10b: the design pass",
     and every step of it is built: `--final-classes` writes the merged program back out as Spite source and
     `check.sh` runs what it printed (done first); the reflection members moved into
     `library/spite/class.spite` as ordinary declared members whose *data* the compiler fills when it writes
     the class object (`is_singleton()`, `functions`, `attributes`); and `Spite.Namespace` (D41) is written
     with every `.namespace` reader migrated in the same change. New conformance:
     `conformance/stage6/namespace_objects` (identity, the parent chain, `.classes`/`.namespaces`) and
     `conformance/stage6/class_attributes` (`Sticker.attributes` beside `sticker.attributes`). One semantic
     still marked proposed-by-Claude-unconfirmed in the decision log: a class-level entry's `.value` is the
     field's declared default.
     Calling by `Symbol` with arguments, defining members from data, and hooks that run when a class is
     reopened move out of this milestone (the design pass places them under compile-time class generation).
     Comprehensive, Ruby grade reflection at compile time (manual.md section 16 item 2): `attributes`,
     `class`, `functions`, `Class.instances`, enumerating and calling functions by `Symbol`, respond-to checks, defining
     members from data, and hooks when a class is reopened -- all resolved at compile time and all emitted as source that
     section 16 item 2 and by milestone 11c below; this entry exists because the manual pointed at a milestone that was
     missing from this list. The design pass it needed is recorded in `bootstrap/COMPILER_PLAN.md`.
     **D12 sets the shape** (manual.md section 8, "Reflection objects"): the whole family lives in the `Spite`
     namespace -- `Spite.Class` (`.name`/`.namespace`/`.attributes`/`.functions`/`.instances`), `Spite.Function`
     (`.name`/`.arguments`/`.returns`), `Spite.Argument` (`.name`/`.class`), `Spite.Attribute`
     (`.name`/`.class`/`.value`) -- and `.class` becomes a real `Spite.Class` everywhere instead of the type name
     as a `String`, which is what the generator emits today. That change is what makes reflection
     composable (identity comparison rather than string matching) and it is what D13 in milestone 12 is built on,
     so it is the part of this milestone with a caller waiting.
     **D6 lands here too** (manual.md section 8, "Class-level functions"): `Spite.Class` becomes a real standard
     library class declaring the class-level hooks, a class file defining one of those names overrides it for its own
     class object, and the override folds at compile time. Spite has no static functions and will not get any, so this
     is the only mechanism for a class-level fact. `func is_singleton(): Bool` is the first hook (D8, decided --
     manual.md section 8, "Singletons"), keyed by a distinct literal constructor argument list and emitted as one
     static slot per argument list. A literal `return` folds without any evaluator, so the hook mechanism and
     `is_singleton()` can both ship ahead of the rest of this milestone.
11. Foreign libraries (manual.md section 17, D4). Split in three:
    - **11a. Done (2026-09-23)** -- see manual.md section 17 for what was built and the choices made. `DynamicLibrary` itself: the class, the `missing_function`/`missing_attribute` hooks
      (the Symbol-codegen segment rule at its limit, where the segment is the whole name, opted into by a reserved
      name), `LoadLibraryA`/`dlopen` + `GetProcAddress`/`dlsym` with the import table built from the tree-shaken call
      sites and resolved once in the constructor, the three built-in naming conventions (`'identity'`, `'windows'`,
      `'camel_case'`), the literal-arguments rule, abort-with-names diagnostics for a missing library or symbol, and
      the resolved-name `#` comments in `--final-classes`. New standard library/reflection members it needs, each
      small and useful on its own: `String.pascal_case()`, `String.camel_case()`, `String.abbreviated()` (the
      the linter abbreviation table read backwards -- one shared table, two directions), `Symbol.kind`
      (`'function'`/`'constant'`/`'type'`), and `Spite.Class.size`.
    - **11b. Done (2026-09-23)** -- see manual.md section 17. The header half: `#include` of the constructor's header argument, constant reads
      (`user32.mouseeventf_leftdown` -> `MOUSEEVENTF_LEFTDOWN`), PascalCase type reads, a scalar-only `type` passed by
      address as a C struct, `Type.size`, the generated `_Static_assert(sizeof(C_TYPE) == sizeof(spite_type))` layout
      check (name derived through the naming rule, used to verify and never to generate), and the diagnostic for a
      `type` carrying a `String`/`List<T>`/`Dictionary<T>`/class field into a foreign call.
    - **11c. Not started, blocked on milestone 10b.** A user-written `symbol_name(symbol: Symbol): String` in place of
      the built-in conventions, which needs a pure compile-time evaluator over `Symbol`/`String`/`Dictionary`. 11a and
      11b ship the three built-in conventions first, so this is an upgrade, not a prerequisite.
    - **Decide before starting 11a:** open question 7 (`singleton`), since `DynamicLibrary` is declared one and two
      classes binding the same library must share one import table. Nothing in milestone 11 blocks on 6b or 8b.
12. **Not started.** The web target (manual.md section 17, "Other environments"): `$target` as a reserved program
    variable, C -> wasm through a C compiler targeting `wasm32-freestanding`, the generated JavaScript glue module,
    `DynamicLibrary` reopened over a JS module's exports, platform roots selected by `load` under a compile-time `if`,
    and `File`/`Directory`/`Process` diagnosed under `--target=web`.
    **D13, the isomorphic split** (manual.md section 17, "Isomorphic classes"): partition every function by the
    class of its first parameter -- `ServerContext` into the server bundle with a route registration and a
    generated network stub on the client, `ClientContext` into the client bundle, neither into both. Reads the
    signature through D12 reflection, comparing class identity rather than matching a name, so it needs milestone
    10b first. The server body is never emitted client-side, which per-bundle tree shaking already gives for free;
    routes generate from class and function name; a `type` shape is the wire format; and a parameter or return
    type that cannot be serialised is a diagnostic naming it. **Blocked on the errors design** (`mortaros_notes.md`
    item 2): a network call fails in ways a local call cannot, and this is where "only errors that stop the
    program" has to be answered concretely. This is the Nullstack-in-Spite milestone.
13. **Done.** D5 + D9, the codegen value form (manual.md section 9): remove the `generics` header line
    (writing it becomes a parse error naming the new form) and move the ordered list into the constructor --
    `func Weapon<$damage_type, $is_magic>(new_damage: $damage_type)`, called `Weapon<Magic, true>(10)`. Every
    codegen value a caller supplies is declared, even a single one; call sites stay positional with no named
    form; a `$name` used but not declared is a flag-fed program variable (`$serve`, `$environment`; superseded by
    D76, milestone 19: that is now an error and settings live in `Environment`), which is
    the rule that separates the two kinds; a class taking codegen values always has a constructor, even one that
    exists only to declare them; and the built-in containers declare theirs the same way (`List<$element_type>`,
    `Dictionary<$value_type>`, `$value_type?`). A wrong-arity or wrong-kind call is an error naming the
    class's codegen values in order, so the message carries what the abandoned named form would have carried.
    Unchanged: the unmatched-`--name=value` typo error, the reserved flag names, and a flag-fed value still being
    mandatory.
    Touches the lexer (retire the `generics` keyword), the parser (`<...>` on a constructor
    declaration), codegen (monomorphization keyed by the constructor's declared order, plus the declared-versus-
    flag-fed split), the formatter (the `generics`-first rule disappears) and the final-class printer.
    Migration: two live files declare `generics` (`examples/arsenal/weapon.spite`) plus
    their call sites; `docs/metaprogramming.md` and `docs/for_ai_writers.md` migrate with it, since
    `check.sh` compiles every sample in them. `examples/dungeon` and `examples/arsenal` keep
    requiring `--serve=`/`--environment=` exactly as they do now, since those are flag-fed and undeclared.
    **Sequencing:** independent of milestones 10-12, but it is a syntax change, so `bootstrap/`'s Spite front end
    (`bootstrap/source/syntax/`) changes with it. Mortaro wants to bootstrap as soon as possible
    (`mortaros_notes.md`, 2026-09-19: get rid of the old code), so a language change that is decided but not
    landed is rework waiting to happen for 8b -- this one is cheap and should go early.
14. **Done, as D70 (2026-09-23)** -- symbols are tree-shakable runtime values from a table the compiler writes, which supersedes the compile-time-only rule below. D10 + D11, symbols and two-level reflection (manual.md sections 7 and 8): a symbol literal
    (`'age'`), legal only where an enum or `Symbol` is expected, with an enum checked as the closed list of
    symbols it accepts and no untyped symbol literal; `Symbol` staying compile-time only, so it cannot be stored
    in a field. Then the two levels: `weapon.attributes['damage']` for the value and
    `Weapon.attributes['damage']` for the `Spite.Attribute`, with a PascalCase receiver meaning the class. The
    pay-off is that `attributes[symbol]` stops being a special compile-time-only form and becomes ordinary
    indexing, and that `person.set_attribute('age', 2)` becomes writable in source at all. Overlaps milestone 10b
    (the class-level mapping's shape is part of that design pass) and should probably land with it.
15. **Nearly done (2026-09-24).** D14, dissolve the runtime (manual.md section 15, "Pure Spite"). `runtime/` is
    deleted: the only hand-written C left is `bootstrap/source/generation/prelude.spite` (the allocator switch, the
    object header, `String`'s layout, and the `Memory`/`DynamicLibrary`/`Arguments` floor), and the generated C
    is tree-shaken so a program carries only what it calls. **D81/D82 done (2026-09-24):** no class is
    registered by hand; the members whose bodies stay C are declarations without a body in the compiler's
    reopening of `Memory`, `DynamicLibrary`, `TypedMemory`, the number classes, `Concurrent`/`Parallel` and the
    reflection classes (item 23). Ordered, because each step unblocks the next:
    - **15a. Proposed (2026-09-23), waiting on Mortaro** -- manual.md section 15, "The floor, named". Name the floor: the five to ten intrinsics the compiler emits directly (raw memory in and out --
      `mmap`/`VirtualAlloc` through the FFI on native, `memory.grow` on wasm -- plus whatever the emitted C needs
      before any Spite exists). Nothing else is allowed to be hand-written C. This is a design step, and it gates
      the rest, because it decides what "pure" means precisely enough to check.
    - **15b.** `File`/`Directory`/`Process`/`Program.sleep` become `DynamicLibrary` calls over libc and
      `kernel32` instead of built-in classes with hand-written bodies. Needs milestone 11a/11b. ~150 lines leave
      the runtime and the system-class special case in the generator goes with them. **D80 done (2026-09-24):**
      no `CRuntime` wrapper; `library/windows/`, `library/linux/` and `library/mac/` each reopen `File`,
      `Directory`, `Process`, `Program`, `Console`, `String` and `Build` with only what differs, loaded by the
      launcher from `Build().target_operating_system` (`--target_operating_system=linux` writes C for Linux).
      Only Windows runs here; `check.sh` holds the linux and mac folders to compiling.
    - **15c.** `String`, `List<T>`, `Dictionary<T>` and the retain/release helpers move to `.spite` sources under
      a `spite/` standard library root: ~400 lines, pure algorithms over memory, expressible in the language as
      it stands. `Spite.Class`/`Spite.Function`/`Spite.Argument`/`Spite.Attribute` (D12) are written here as
      ordinary classes, which is what D6 requires. Float formatting (~50 lines) becomes a shortest-round-trip
      implementation in Spite or an FFI call -- **done (2026-09-24)**: pure Spite in `library/number_text.spite`,
      and its C is deleted. `List<T>` and `Dictionary<T>` -- **done (2026-09-24)**: `library/list.spite` and
      `library/dictionary.spite`, plus `split`/`lines` in `library/string.spite`; what the compiler still emits
      per element type is in the decision log's containers row. Wants milestone 8b first -- a compiler in Spite makes this natural
      rather than heroic -- and it is what finally makes standard library classes reopenable (section 16 item 7).
    - **15d. Done (2026-09-24).** The REPL is `library/read_evaluate_print_loop.spite` (D72): paths, assignment
      and calls with literal arguments; `spite_repl.h` is deleted. `--repl-port` and `spite connect` -- **done
      (2026-09-24)**: the same `answer` over a `Socket` (`library/socket.spite`, reopened per operating system)
      on a thread the compiler's two-line C entry starts; `check.sh` replays `docs/repl.md`'s ` ```wire ` session
      against the real program. Linux and macOS sockets and threads compile but have not run.
      D37's drain points -- **done (2026-09-24)**: commands are answered on the program's thread where it waits,
      through the scheduler below; a `--repl-port` build of a loop that never waits is a compile error.
    - **15f. Hidden async/await and threads (D35, D37, D99, D103) -- done on Windows (2026-09-24).**
      `Concurrent(function)` runs on a fiber (`library/concurrent.spite`, `library/scheduler.spite`, each system's
      folder for fibers, an event and a clock); `Parallel(function)` on a thread (`library/parallel.spite`). The
      compiler wraps the blocking calls (`Program.sleep`, and the system call under `Console.read_line`, `File`
      reads and writes, `Socket.accept_client`/`read_line`) in programs that use the scheduler, blocks when nothing
      else could run, and counts references atomically only in programs that start a thread. Codegen values are
      inferred from constructor arguments. `docs/concurrency.md`, `conformance/stage6/concurrent_waits`,
      `conformance/stage6/parallel_work`. **Next:** a thread pool and `parallel_each_` templates for the engine,
      D35's race rule, HTTP and database calls through the same waits, and running the Linux and macOS folders.
      Names and mechanism wait on Mortaro (`mortaros_missing_decisions.md` 13-18).
    - **15e.** The web shim: drive the hand-written portion to zero by generating the imports from `external js`
      declarations and the command-buffer drain loop from its opcode table (milestone 12). It cannot become
      Spite -- it runs in the JavaScript virtual machine -- but it can stop being authored. Goal, not a
      prerequisite for milestone 12 shipping.
    Performance is not a reason to delay any of this: the output is still C, so Spite-written standard library
    code meets the same optimiser the hand-written C does.

16. **Done.** Being able to run tests (manual.md D46: a test is a package that crashes). The point was
    to build only what tests actually need, not to wait for milestone 10. Ordered so that each step is usable on
    its own:
    - **16a. Done.** All nine examples migrated, each with an `expected_output.txt`, run by `check.sh` to the
      corpus standard: exact output and balanced memory. Unqualified class names now resolve by walking up
      namespaces. The old `tests/` fixtures were deleted rather than migrated -- they had no expected outputs
      left, and `conformance/` and `diagnostics/` cover what they covered.
    - **16b. Done.** `spite.crash<TAB>path:line<TAB>Class<TAB>function`. It was the one-line change it looked
      like. `check.sh` now also accepts programs that are meant to crash, via a `crashes.txt` beside them.
    - **16c. Done.** `tests/tests.spite`: 21 `test_` functions over `String`, `List<T>`, `Dictionary<T>` and the
      member templates, `crash` as the only assertion, hand-listed in the entry class, run by `check.sh`. A wrong
      expectation fails the run with `spite.crash tests/string_tests.spite:4 ...`, which is D46 working as
      described.
    - **16d. Done.** `instance.functions` is a `List<Spite.Function>`, generated only where used, each carrying
      `.name`, `.arguments` (a `List<Spite.Argument>` of `.name`/`.class`) and `.returns` -- `Nothing` (D48) for
      every function declared without a return type. A function value retains the instance it came from, and
      `call_function()` calls it. `tests/tests.spite` no longer hand-lists: `run(StringTests().functions)` calls
      everything named `test_*`. `library/nothing.spite`, `library/spite/function.spite` and `argument.spite`
      are ordinary Spite source, which is D14 continuing.
      **Since built (2026-09-23):** typed `Spite.Function<Arguments..., Return>` (D39), naming a function to pass
      it as a value, and calling one with its arguments (`conformance/stage6/typed_functions`); class-level
      `Class.functions` (D57). **Still not built:** `.owner` as a readable attribute (D40 -- the typed form does
      not say what class the owner is, so there is still no type to give it), and calling a *reflected*
      function with arguments.
    - **16e. Done.** The crash line is `spite.crash<TAB>path:line<TAB>Class<TAB>function<TAB>condition`, the
      condition rebuilt from its tokens and followed by the named operands of the failed comparison with their
      values (D25); a crash also prints the asserts that failed before it, from a ring of 32. Crash and assert
      sites carry content-derived ids and every build writes `<output>.crashes` (D32, D33).

    Why this order: after 16a and 16b -- both small -- a failing test is a crash naming a file, a line, a class
    and a function, which is enough for an AI to fix it and re-run. Everything after that is refinement.

17. **Mortaro's 2026-09-23 inbox** (manual.md decision log, D58-D69; open questions 12-20). Built:
    D58 `join`, D59 (already true), D60 `_:` and repeated cases, D61 for Symbol codegen (the `List<T>` member
    templates wait on 15c), D63 copy-to-narrow, D64 `[]` answers `T?` with count/bound proofs, D65
    `name_with_namespaces`, D66 leak-proven tests, D67 file order, D69 `T? == value`. **Waiting on Mortaro:**
    D68 (class names as `Symbol`) conflicts with D10 -- the manual records a proposal; open questions 12
    (generic header syntax), 14 (variadic arguments), 15 (whether `while` goes), 18 (entry file convention),
    19 (constants and read-only reflection attributes) and 20 (nested `if`/`else`) each carry Claude's position.
18. **Done (2026-09-23): the formatter** (manual.md sections 12 and 13): `bin/spite format [--check]`, a
    safety check that refuses any rewrite that changes the program or loses a comment, and a tree kept in one
    style by `check.sh`. Not built: formatting on every compile, long list literals, `docs/` code blocks.
19. **Done (2026-09-24): D76, `$` for generics only and `Environment` for settings** (manual.md section 9,
    "Program settings"). `library/environment.spite` is a singleton a program reopens in its own
    `environment.spite`; each declared field is read at run time from the program's `--name=value`, else the
    upper-case environment variable, else its literal default. An undeclared `$name` is an error pointing at
    `Environment`; a non-compiler `--name=value` before `--` is an error; `Arguments()` answers the command line
    anywhere. This supersedes the flag-fed half of milestone 13. **Then (2026-09-24): D84, D85, D86, D89 and
    D97.** `Build` (`library/build.spite`) is the compile-time twin: every compiler option is one of its fields,
    a program reopens it in `build.spite`, a flag before `--` sets a field, and every field folds to a constant,
    so branches on it are tree-shaken (`conformance/stage6/build_settings`). `operating_system` and
    `target_operating_system` replace `--operational_system`. A program is named by its folder and its entry
    constructor takes no arguments. `launcher/launcher.spite` loads `library/`, the target system's folder and the
    program with visible `load` calls, and `main` only constructs it. **Waiting on Mortaro:** the names `Build`
    and `Launcher`, `mode`/`format` read from the flag alone, the bare-`Bool` flag form, the renamed flags
    (`--final_classes`, `--repl_port`, `--format=false`), a `.spite` path still naming an entry, and the C that
    stays in `main` (`mortaros_missing_decisions.md`). **Not done:** `main`'s floor (arguments, `_setmode`,
    releasing singletons and class objects, the memory report) is still written in C by the compiler.
20. **Done (2026-09-24): D87, D90 and D104, the header lines and variadic arguments** (manual.md sections 5, 8
    and 9). `generic $name` lines replace the constructor's `<...>` list (a parse error now), so a generic class
    needs no constructor; `singleton` is a header line and `func is_singleton()` an error outside
    `Spite.Class`; D67's order starts with both. `...name: List<Type>` gathers the remaining arguments into a
    list, over a class or a `type`. **Waiting on Mortaro:** the interpretations in the three 2026-09-24
    "implements" rows, and whether `Console.print` takes `...values: List<Printable>` (what `Printable`
    requires). **Not done:** the named-constraint form open question 12 argued for (`generic $sub_type:
    Openable`), which D87 did not decide.
21. **Done (2026-09-24): D93, a directory's entries** (manual.md section 15, "System classes"). `Directory.path`
    and `entries(): List<Directory.Entry>`, a union of `Directory` and `File`
    (`conformance/stage4/directory_entries`, `docs/standard_library.md`). **Waiting on Mortaro:** the name
    `Entry`, the order, and removing `files()`/`folders()`.
22. **Done (2026-09-24): D95, `Json<T>`** (manual.md section 15, "JSON is reflection, not a library"; sections 8
    and 9 for the metaprogramming it needed). `library/json.spite` and `library/json_reader.spite`, written with
    `Symbol<$value_type>` templates called for every attribute by their plural, compile-time type tests
    (`$value_type == List`) and `$value_type.element_type`; text casts to an enum by name
    (`tests/json_tests.spite`, `conformance/stage6/every_attribute`, `conformance/stage6/json_crash`,
    `diagnostics/every_attribute`, `docs/json.md`). **Waiting on Mortaro:** every one of those forms, the API
    names, and the missing/unknown/mistyped rules. **Not done:** `inf`/`nan`, `Symbol` attributes, and printing a
    generic instance's generated functions in `--final-classes`.

23. **Done (2026-09-24): D82, D83, D88, D92, D98, D100 and D101 -- the compiler's reopening, numbers as classes,
    read-only reflection, and `Memory` as the floor** (manual.md sections 2, 4, 8, 11, 15 and 17; the five
    "implements" rows of 2026-09-24). A `func` without a body is a member the compiler supplies, merged as a
    reopening after `library/` and printed by `--final-classes`; `Memory`, `DynamicLibrary` and `TypedMemory` are
    `library/` files. `Int`, `Long`, `Double` and the rest are value classes with `text()` in Spite, `this` is a
    keyword, and number casts go through each class's `from_type` (an inline C cast). Reflection keeps its data in
    private `_` fields behind getters, and `_` is enforced as private. `Memory.allocate_stack_bytes`,
    `TypedMemory<$value_type>` (which `List<T>` now uses) and `value.memory` (`Spite.Memory`) are built, with a
    ring buffer in `docs/memory.md` (`conformance/stage6/numbers_are_classes`, `diagnostics/reflection_read_only`).
    **Waiting on Mortaro:** `mortaros_missing_decisions.md` 28-36. **Not done:** `Arguments` is still the
    generator's (`Console`'s printing is Spite since D109); `String`'s `length`, `code_at` and `slice` are still emitted by name
    rather than declared in its reopening; `missing_function`/`missing_attribute` for `DynamicLibrary`; a
    user generic instantiated with `Int` prints as `...Int` in `instantiated/`, which the name lint rejects on
    the way back.

24. **Done (2026-09-24): D107 and D108 -- `to_string()`, and every type's storage visible over `Memory`**
    (manual.md sections 4, 10 and 15; the four "implements D107/D108" rows). Numbers and `Bool` answer
    `to_string()`. `library/string.spite` declares `_bytes`, `_length`, `_section` and `_capacity`, the compiler
    derives `SpiteString`'s layout from them, and every `String` function is Spite, down to its constructor and
    `drop()`; `TextBytes`, `Memory.take_text` and `Memory.address_of` are gone and `Memory.text` is Spite. Each
    number declares `var _memory = Memory().allocate_bytes(N)`. The compiler places allocations (register,
    frame, constant, heap) and `allocate_stack_bytes` is removed; `Memory` is a static object in a production build and an ordinary one in
    an inspectable build (`--repl`, `--repl_port`, `--hot_reload`, `--development`; D143, done 2026-09-25, which
    also keeps those builds unshaken). **Waiting on
    Mortaro:** `mortaros_missing_decisions.md` 50-56. **Not done:** `Arguments` is still C in the prelude; a
    `List` or `Dictionary` does not declare a layout the compiler reads (they are ordinary classes already).
25. **Done on Windows (2026-09-24): milestone 6b, live reload -- D111 and D112** (manual.md section 14, "Live
    reload and 6b"; `docs/repl.md`). `--hot_reload` calls the program's functions through re-pointable slots,
    rebuilds the classes whose files changed (and the callers of their changed signatures) into a library with
    `--mode=reload`, and swaps it in at a drain point, from the REPL's `reload` or the operating system's file
    watcher. A change to attributes or enums is refused until a restart. `check.sh` edits a copy of docs' program
    and checks both paths keep its state. **Not done:** Linux and macOS are only compiled; instance migration,
    reflection that follows reloads, and compiling off the program's thread wait on
    `mortaros_missing_decisions.md` 64-69.

## Later, deliberately deferred

- **Generating class files at compile time** (Mortaro's idea, 2026-09-19). A framework could generate a namespace at
  compile time: a markup package with real `ul`, `li`, `div` functions instead of catching everything through
  `missing_function`; a class per database table; typed route functions; a class per protocol message. It is
  stronger than `missing_function` because generated functions can be listed (`functions`), reflected and printed
  by `--final-classes`, so an error names a real function instead of a hook. Constraints for whoever designs it:
  the output must be ordinary readable Spite source in `--final-classes` (that is the line between this and
  macros, which the philosophy forbids); generators must be pure and cached by a hash of their input, because
  compilation speed is a first principle; a generated class needs a defined place in the reopening order, since a
  later root may reopen it; and it depends on the compile time evaluator of milestone 10.
- **Standard library templates name a `<member>`** (D15) is done in the compiler: `map_`, `filter_`, `sum_`,
  `sort_by_`, `find_by_`, `any_`, `all_`, `count_` and `each_` accept an attribute or a function that takes
  nothing, on `List<T>` and on `Dictionary<T>`. **D91 done (2026-09-24):** they are Spite templates in
  `library/list.spite` (`item.attributes[member]` reads the element's member), the generator writes none of
  their C, and a `--repl` build lists them on a list. **D105 fusion done (2026-09-24):** a chain of them compiles
  to one loop with no intermediate list (`conformance/stage6/fused_chains`, `fused_chain_allocations`).
  Open: printing a list's instantiated templates in `--final-classes` (D61).
