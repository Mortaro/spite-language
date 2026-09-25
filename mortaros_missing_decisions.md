# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into `manual.md` (prose and decision log) and removes the item. Each item links to where the
manual argues it.

## Open questions still open in manual.md

2. **Open question 15: whether `while` goes.** Proposal: the index loop over a list (199 of 259 loops) becomes an
   error naming the member template; `while` stays for loops over state.
   D94 review: [mortaros_review_while_and_else_if.md](mortaros_review_while_and_else_if.md). Of 472 loops, 31 are
   replaceable today and the exact-shape rule there catches about 10; most index loops pass the element to a
   function of the caller, which no template expresses.
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
## Variadic arguments

14. **Whether a generic line can name a constraint**, `generic $sub_type: Openable` (open question 12's own
    proposal). D87 decided the lines and not this half.

## From hidden async/await (D99, D103)

16. **The mechanism: stackful fibers plus a helper thread per blocking call**, chosen over a state-machine
    transform and over threads for everything (the decision-log row argues it). Built on Windows; the Linux and
    macOS folders (`makecontext`/`swapcontext`) are only compiled.
17. **What a `Parallel` function may touch.** Nothing is checked yet, and with reference-counted fields a race can
    free a value another thread is reading. Options: D35's syntactic rule (it reaches only its own instance and its
    locals, which rejects `Parallel(file.read)` because `File` reaches `Memory` and its library through fields);
    that rule with singletons allowed; or running a `Parallel` on a deep copy of its instance.
18. **Inferring codegen values from constructor arguments** (`Concurrent(file.read)` without `<String?>`), which
    D35's own example needs and D9 did not foresee.
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

## From D106 (assert guards)

27. **Library asserts in the crash trace.** Since D106 made `assert` the only way to write a default-returning
    guard, library functions that fail a guard routinely (`String.matches_at`, `String.slice`) fill the
    32-entry trace ring a crash prints. Either the ring skips asserts in `library/`, or those functions are
    written as plain expressions instead of guards.

## From D82, D83, D88, D92, D98, D100 and D101 (the compiler's reopening, numbers, reflection, memory)

29. **`_` now means private, enforced**: a `_name` is read, written or called only inside its own class. D88
    needed it (otherwise `klass._name = ...` undoes the read-only getters), and section 2 already said `_name` is
    private. Open question 6 (whether `_` means private *and* unused) is still yours.
31. **`value.memory` is shadowed by an attribute named `memory`**, and most of the standard library holds
    `var memory = Memory()`. Either rename those attributes (`heap`?) or give the reflection another name.
    Also the names: `Spite.Memory`, its sections `'heap'`, `'stack'`, `'constant'` (`static` is a C word).
33. **`TypedMemory<$value_type>`** is the name of what reads and writes values of any type in raw memory
    (`read_value`, `write_value`, `release_value`, `value_bytes`), one shared instance per type.
34. **`from_type` is an instance function on the class cast to** (as D100 wrote it), so calling it by hand reads
    `0.0.from_int(count)`; the compiler calls it for every number cast. A cast written in Spite inside a reopened
    `from_type` would call itself, so a reopening can only replace it with the same C cast. A class-level form
    (`Float.from_int(count)`, D6's class object) would read better.
36. **Unions of numbers**: should a union admit number classes? `Json` tests for a number with ten
    comparisons (`$value_type == Int or $value_type == Long or ...`) because it cannot today. With D83 every number is a class in
    `library/`, so a union of number classes is no longer a union of names that are not classes.

## Build, the launcher and the entry (D85, D86, D89, D97; manual sections 3, 9 and 13)

38. **Loading the program's folder runs it.** The launcher's `load(Build().program)` constructs the program's
    entry class; every other `load` still compiles to nothing at run time. And a launcher `load` may use `Build`
    fields (`"library/{Build().target_operating_system}"`), where every other `load` takes a literal.
39. **The C left in `main`**: handing `argv` to `Arguments()`, binary standard output on Windows (`_setmode`), and,
    after `Launcher` returns, releasing singletons and class objects and printing the `--debug_memory` report.
    Moving them into Spite needs a way for Spite to receive `argv` and to run code after the program ends (a
    `Launcher` that releases what the program left?) -- which is a language question.
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
46. **Singletons with arguments.** Section 8 ("Singletons") says one instance per distinct literal argument list;
    the compiler rejects any singleton constructor with parameters (`diagnostics/singleton_arguments`) except
    `DynamicLibrary`'s, which keeps the per-argument-list behaviour. Which is the rule, and is `DynamicLibrary` the
    exception or the rule?
## Visible storage and placement (D107, D108; manual sections 4, 10 and 15)

50. **How `String` declares its storage**: private attributes read by name -- `_bytes: Long` (the address of
    its characters), `_length: Long`, `_section: Spite.Memory.Section` (`'heap'`, or `'constant'` for a literal)
    and `_capacity: Long` -- and the compiler writes the C layout from them. The three it needs are a naming
    convention checked with an error, not a keyword. Keep the names? Is `_section` the right way for the compiler
    to record where it placed a text?
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

71. **Finding every function named `*_system` across the program** -- the same reflection over the program's
    classes. Not decided.

## From D109 (printing through `to_string()`, `Console.debug`; manual sections 5, 8 and 15)

74. **A number, `Bool`, `Symbol` or enum value passed where a `type` is wanted is boxed**: one small allocation,
    freed like any object, so `print(count)` costs a box and the `String` its `to_string()` makes, and every
    `print` costs the `List` its values arrive in. The self-compile did not slow measurably, and the two corpus
    programs that pin allocations moved from 29 to 50 and from 11 to 17. Is that an acceptable visible cost, or
    should a variadic list the callee only reads live in the caller's frame (the D108 placement rule, one step
    further)?
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
83. **Text holes call `to_string()`** (answered provisionally): a text hole now calls a class's `to_string()` (`"{ticket}"` works, and
    `"{attribute.class}"` prints the type's name), the way `console.print` does. Keep it, or require the call to
    be written?
84. **The default of a `type` is an object literal** (manual open question 1): `var row: $row_type = null` bound
    to `type Target { health: Health }` now holds `{health: Health()}`, whose `.class` answers `Object`. A `type`
    that requires a function has no default object and stays a null pointer that reads defaults. Should that
    case be a compile error naming the field instead?
## From the Vulkan renderer bugs (manual sections 11, 12, 13, 15 and 17)

89. **A loaded package cannot find its own folder at run time.** SlopEngine locates its shader sources by
    reopening `Build` with `var slop_folder = "../../slop"`, a copy of its `load` literal that breaks when the
    loader moves. Proposal: `Spite.Namespace.folder` -- the folder a namespace was loaded from, as the compiler
    resolved it -- or the `load` path available per namespace some other way. Which, if either?
90. **A loaded folder that is itself a program merges into the loader.** A test program that `load`s a game
    folder, and has its own root `composition.spite` or `plugin.spite`, silently reopens the game's classes of
    the same name (reopening, by design), which cost SlopEngine a debugging round. Master now makes reopening
    the entry class an error; this is the same trap one level down. Proposal: a loaded folder that has an entry
    file (a file named after the folder) keeps its root classes to itself, or merging with it must be asked for.
    Related to item 82.
92. **The compiler's own C names use `___`, and a name has one `_` between words.** `allocate`, `make`,
    `retain`, `release` and the rest are ordinary method names now, and `hit__count`, `__strike` and `strike_`
    are naming errors. `init` stays an abbreviation error and `default` a C keyword error, so neither became a
    legal method name. Is the stricter snake_case rule fine?
93. **A foreign call with a header goes through the header's prototype.** C checks the count and converts each
    argument; converting an integer to a pointer (a `Long` handle) is the one complaint silenced, so a `String`
    passed where C wants an `int` is converted silently too. A C function returning `void` reads as `0`. Every
    called function must be declared by the header, so a header that does not declare one is a C error at build
    time. Without a header, integers cross as 64 bits. Fine, or should the header be required whenever a call
    passes anything but a `Long`?
## From adding the bitwise functions (D117)

95. **The bitwise names** (proposed by Claude, unconfirmed): `shifted_left(count)`, `shifted_right(count)`,
    `bits_and(other)`, `bits_or(other)`, `bits_exclusive_or(other)`, `bits_inverted()`, and the extras
    `set_bit_count()`, `leading_zero_count()`, `trailing_zero_count()`. `bits_` because `and`/`or`/`not` are
    keywords and `xor` abbreviates. Keep them, or another family (`and_bits`, `shift_left`, `inverted_bits`)?
96. **Shift counts outside 0 to width - 1**: a count of the width or more shifts every bit out (0, or -1 for a
    negative value shifted right, as Go does), and a negative count halts with a report naming the function and
    the count. The alternative was masking the count to the width (Java, C#, what x86 does), which is branch-free
    but makes an `Int`'s `shifted_left(32)` answer the value unchanged. Fine?
97. **The other operand is cast to the receiver's type**: a `Byte`'s `bits_and` of a `Long` answers a `Byte` from
    the `Long`'s low 8 bits, and the count is always an `Int`. This is the ordinary argument cast, so it holds
    whatever item 88 decides for arithmetic. Should a narrowing here be an error instead?

## From D114, D115 and D116 (compile-time reflection over functions, folders and names; manual section 8)

98. **The D114 spelling as built**: `$system_type.has_function('run_each')` (a member of `Spite.Class`, folded in
    a condition, answering at run time elsewhere) and `argument: Symbol<$system_type.run_each>` over the
    arguments. A template over arguments that returns a value has exactly one use for its plural:
    `system.run_each(row_arguments())`, the whole argument list of that same function, which D77 would otherwise
    forbid. Is that one exception to D77 acceptable, or would you rather the values be gathered some other way
    (for example the template storing each value, and a separate `system.run_each(...)` spelled some other way)?
99. **The D115 spelling as built**: `system: Symbol<System>` -- a range that names no class or type is read as
    the end of a dotted namespace, so `System` finds `System.Heal` and `Ui.System.Interact`. Classes are walked in
    order of their dotted names (not discovery order), and a range matching no folder walks nothing (it used to be
    an error; SlopEngine's `Cook` needed a program with no recipes to compile), so a misspelled range walked by its
    plural (`Symbol<Sytem>`) silently does nothing -- a single class named with a typo is still an error. Keep the
    order and the empty walk? And should `Symbol<System>` also reach classes in folders below a `system/` folder
    (`System.Combat.Hit`), which it does not today?
100. **The D116 grammar as built**: one hole per pattern, spelled by the parameter's name being a word of the
    function name in the range (`phase: Symbol<$system_type.phase_each>`), exactly as a template's own name is
    spelled. Two readings came with it: `system.phase_each(...)` written inside that template calls the matched
    function (the pattern as a member name), and a template over `$system_type.phase_each` called from inside
    walks the matched function's arguments, its instances named `<name>_in_<function>`. Is a member name
    changing meaning inside a template acceptable, or should the matched function be reached some other way?
101. **`run_each` also fits `<phase>_each`**, with the phase `run`. A system with `run_each` under an engine
    walking `<phase>_each` is placed in a phase called `run`; the engine in `conformance/stage6/system_phases`
    crashes on a phase it does not own. Should a pattern exclude names the program uses another way, or is the
    engine's check the right place?
102. **What `has_function` counts**: folded, the functions a class declares (not its constructor, not a `_`
    function, not what a template generated for it); at run time, `.functions`, which includes what templates
    generated. The two can disagree for a template-generated name. Align them, and which way?
103. **A symbol literal where text is wanted is that text** when no enum has a value by that name, so
    `Sprinkler.has_function('drain')` works (it failed with "cannot tell which enum"). A literal an enum does
    name still means the enum value, so adding an enum value `drain` somewhere would change what that line
    passes. Keep, or should text always be written in double quotes?

## From the SlopEngine regressions after item 85

105. **Naming a root class that a nearer one shadows.** Inside `click_test/`, `Plugin()` finds `ClickTest.Plugin`
    first (the walk goes from the class's own namespace outward), so a `ClickTest.Composition` that wants both the
    game's root `Plugin` and its own `ClickTest.Plugin` cannot name the root one; SlopEngine renamed its own to
    `TestPlugin`. The reporter's two options: (a) a root qualifier, some spelling that starts the walk at the whole
    program (a leading `Root.` or `.`, say); or (b) fall through to the next match when the nearer class would be
    a reference to the class itself (`var counter = Plugin()` written inside `ClickTest.Plugin`). (b) needs no
    syntax but only covers the self-reference case, and a name then means different things in different files.
    Nothing is built; which, if either?

## From SlopEngine adopting D114-D116

108. **Passing a template's symbol to a helper** (proposed by Claude, unconfirmed; manual section 8). A function
    whose ranged `Symbol<...>` parameter is not a word of its own name, such as `run_combination(phase:
    Symbol<$system_type.phase_each>, combination: Int)`, is now a template reached by passing it the calling
    template's own symbol by name, `run_combination(phase, combination)`, and it is compiled once per symbol
    (`run_combination_for_update`). Two readings came with it: whether a template is spelled by its name or
    reached by a passed symbol now depends on whether the parameter's name is a word of the function's name, and
    the passed symbol must be over exactly the same range text. Keep the rule, or would you rather the helper be
    spelled some other way (a name with the hole, `run_phase_combination()`, reached from inside the template)?

## Behind D118 (unused attributes), from SlopEngine

109. **Should the compiler tell an engine what a function does?** SlopEngine marks systems with attributes nothing
     reads -- `var world = Resource.World()` (it may spawn or despawn, so it runs alone in its stage) and
     `var main_thread = Resource.MainThread()` (Win32 window procedures, Vulkan present) -- and its engine finds
     them with a compile-time walk over each system's attributes. D118 keeps them legal because that walk counts
     as a use. The session's guess at what you want, since your D118 example was exactly `var world =
     Resource.World()`: the marker is noise, and the engine should learn "this system changes the world" from the
     compiler -- which functions it calls (`Spawn`, `Insert`, `Remove`), which singletons it touches -- as a
     compile-time reflection like D114's (`function.calls(Spawn)`, or the singletons a function reaches). Wanted?

## Zero hidden code (D147)

113. **How Spite names the operations the machine does directly** (reading and writing the value at an address,
     atomics), so `Memory`'s functions get real Spite bodies and a later backend can replace C. Every language
     bottoms out here (Zig's `@builtins`, Rust's intrinsics); the choice is only how they are spelled and where
     they live. Options to react to: (a) a reserved namespace of operations, `Spite.Machine.read_byte(address)`,
     declared in one library file the backend implements; (b) operators on a pointer-like value type,
     `address.byte_at(offset)`, where `Address` is a number class whose members the backend lowers; (c) something
     you have in mind. The rest of the floor (allocation, copying, comparing, loading libraries) becomes plain Spite
     calling the platform's library through `DynamicLibrary`.
     *Mortaro (2026-09-25):* likes `Address` only if "the code you see is what you get"; otherwise prefers what the OS
     offers through `DynamicLibrary`, and needs convincing with limitations and downsides. *Claude's case:* the OS
     offers memory pages (`VirtualAlloc`/`mmap`), so allocation, freeing, copying and loading libraries can all be
     visible Spite calling the OS -- at the cost of writing our own small-object allocator in Spite on top of pages.
     No OS offers "the byte at this address": that is one CPU load instruction. Doing it through a DLL call
     (`RtlMoveMemory`/`memcpy` per read) works but costs a call per access with no inlining, roughly 5-20x slower
     on memory-heavy code (zstd, ECS loops, text). `address.byte_at(offset)` is a language primitive in the same
     sense as `+` on two integers -- one instruction, nothing below it -- that each backend lowers; downsides: it can
     read a wrong address (so `library/` only), and every backend must implement ~10-15 such operations.
     Recommendation: OS for pages, libraries and files (with a Spite allocator); `Address` only for loads, stores and
     atomics.

## From D127 (dates, times and time zones; manual section 15, `docs/time.md`)

All of it is proposed by Claude, unconfirmed.

118. **The constructors.** `Instant(since_1970: Duration)`, `Duration(amount, unit)` and `Period(amount, unit)`
     with a unit enum, since Spite has no static functions for `Duration.of_hours(2)`. With no overloading,
     `instant - instant` is the `Duration` between them and going back is `instant + -duration`. A date that
     does not exist (`LocalDate(2023, 2, 29)`) halts, while `TimeText` answers `null` for such text.
120. **`Clock.unix_milliseconds()` is gone**, replaced by `now(): Instant`; the monotonic `elapsed_*` readings stay.
121. **Reading text is lenient where RFC 3339 is**: `t` or a space for `T`, `z` for `Z`, `,` before a fraction, a
     leap second read as the second before it, and an RFC 9557 `[zone]` after an offset read and ignored. Writing
     is always the one form. Stricter instead?
122. **`far` and `near` are not in the C-reserved list**, but `windows.h` defines them as macros, so a local named
     `far` compiled to broken C (found writing `calendar_math`). Add them, and whatever else `windows.h` defines
     in lower case, to the list item 107 is about?

## From SlopEngine's system phases (D116)

123. **Limiting a name pattern to a known set.** SlopEngine's private helpers `interact_all(...)` and `drag_all(...)`
     were matched by `Symbol<$system_type.phase_all>` as phases called "interact" and "drag", with errors far from the
     cause. Options: let a pattern's hole be constrained to a list the engine owns (`phase` must be one of
     `App.phases`), so a non-phase `_all` function stays ordinary; or make a function that fits a walked pattern but
     is not meant as one an error at its declaration; or leave naming discipline to the program.
