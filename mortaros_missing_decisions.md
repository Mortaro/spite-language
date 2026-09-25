# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into `manual.md` (prose and decision log) and removes the item. Each item links to where the
manual argues it.

## Open questions still open in manual.md

5. **Open question 16: two versions of one dependency.** Needs D38 (git dependencies) first.
6. **Open questions 1, 3, 6, 8, 9, 10, 11**, the older ones: `= null` on a generic field, right-to-left casting
   in comparisons, `_` meaning private and unused, an unrelated `get_x()` intercepting `.x`, `${` in text,
   where `--final-classes` shows provenance, and function-valued `type` members.

## Proposals built and waiting for a yes or no

9. **The floor** (section 15, "The floor, named"): what stays C, and D82's form for showing it in
   `--final-classes`.
10. **Rows marked "(proposed by Claude, unconfirmed)"** in the decision log from 2026-09-23 and 2026-09-24:
    `Memory.address_of`/`compare_bytes`/`take_text`, the tree shaker, `nan` printing as `nan`, the REPL's
    command names and output, `Environment`'s sources and their order, `--operational_system` (superseded by
    D86, and spelled `operating_system` now), the containers row, and the D91/D105 rows (a `List` template's symbol names the element's member;
    how a chain fuses).

## From hidden async/await (D99, D103)

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
    after `Launcher` returns, releasing singletons and class objects and printing the `--debug-memory` report.
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
    one level of indirection per object in `--hot-reload` builds), or something else?
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
## Outputs and paths (D128, D129, D130)

134. **How outputs are chosen.** Built as `Bool` fields of `Build` -- `run` (default `true`), `executable`,
     `c_source`, `format` (default `true`) -- plus `final_classes` as a folder (proposed by Claude, unconfirmed).
     Because `run` defaults to `true`, asking for another output also runs the program unless `--run=false` is
     given: `spite game --c-source --run=false` for the C alone. The alternatives: a program that names any output
     on the command line gets only the outputs it named (shorter, but a flag then changes another flag's default),
     or one list field, `--outputs=executable,c_source`. Keep the `Bool`s?
135. **`target_operating_system` in a program's `build.spite`.** It is still read from the flag alone, because the
     launcher needs it to pick `library/<system>/` before the rest of the program is read; a program that reopens it
     is silently overridden today. Make that reopening an error, or read the program twice (once to find `Build`,
     once with the right library folder)?
136. **Where a run's executable goes.** With no flag it is built beside the program (`game/game.exe`, and
     `game.crashes`), which D129's "build to the same folder" reads as, so every run leaves those two files in the
     program's folder (`.gitignore` now ignores `*.crashes`). The one intermediate, the C compiled when `--c-source`
     is off, goes to the language repository's `.spite-cache/<name>.c`. Should a plain run (no `--executable`)
     build into the cache instead and leave the program's folder untouched? A `--hot-reload` build beside its program
     also puts its reload libraries in the folder the watcher watches, so each reload wakes it once more.
137. **`--mode=tokens` and `--mode=tree` are gone** rather than made outputs: nothing used them, and a `.spite` file
     can no longer be named. Bring them back as outputs (`--tokens`, `--tree`, printing every program file)?
138. **Two ways to format?** A compile formats the program's files (`format`, an output), and `spite format <path>`
     formats files that need not be a program -- `library/`, which no program's compile formats. Keep both, or
     make formatting the library the job of compiling `bootstrap` (which loads it)?

## From building D194 (the file and folder watcher; manual section 15)

164. **What is the watcher called, and are its members right?** Built as `Watcher` (`library/watcher.spite`, with
     `library/<system>/watcher.spite`), proposed by Claude, unconfirmed. D194 asked for a better name than
     `FileWatcher`, since it watches folders too. Alternatives: `Watcher` (short, but says nothing of files --
     a game may want its own `Watcher`), `FileSystem.Watcher` (namespaced; there is no `FileSystem` namespace
     yet, and `File` and `Directory` would not move into it), `PathWatcher` (says what it takes and what it
     answers: paths), `Directory.Watcher` (reads as a folder only). The members are also proposed:
     `watch(path): Bool`, `changes(): List<String>` (never waits; answers once no change has been seen for 100 ms)
     and a third, `wait_for_changes()`, which blocks the calling thread in the operating system until `changes()`
     has something -- added because `HotReload`'s thread must sleep in the kernel rather than poll `changes()`.
     Keep `wait_for_changes()` public, and should the 100 ms be a constructor argument?

## From building D143 (inspectable and production builds; manual sections 8 and 13)

142. **Which builds are "production"?** D143 says internals are hidden "at optimized production builds" and
     ordinary in `--repl`, `--repl-port`, `--hot-reload` and `--development` builds. Built (proposed by Claude,
     unconfirmed): every build that is not one of those four is production, the plain `spite program` included, so
     an ordinary build keeps the static singletons and the tree shaker it had; `--optimized --repl` is inspectable.
     Only tree shaking and static singletons count as hiding an internal; fused chains, placement and text appended
     in place stay on in every build, since they change speed rather than what reflection sees. Keep that, or should
     a plain build be inspectable too, leaving hiding to `--optimized` alone (every ordinary build then allocates
     `Memory` and `Build` and carries unshaken C)?

## From D134 and D135 (the thread pool and join on first use; manual section 15, `docs/concurrency.md`)

All of it is proposed by Claude, unconfirmed.

143. **Where a handle becomes its value.** Everywhere a `T` is expected (typed `var`, argument, `return`,
     operand, text, a member the handle lacks, a condition) the compiler reads the value; an untyped `var` keeps
     the handle, and `finished` is the handle's own. `wait()` and `join()` are removed rather than kept as an
     explicit form. Two consequences to confirm: `a == b` on two handles compares their values, and there is no
     way to compare the handles themselves; a `Concurrent<Nothing>`/`Parallel<Nothing>` is only waited for by
     dropping it (SlopEngine's `running[index].join()` becomes `running.clear()`, or leaving the function).
144. **`ThreadPool` as a visible singleton**, with `size()` and `worker_index()`. The name says what it is; it
     could instead stay hidden behind `Parallel`. Workers are one per core but one (the program's thread keeps
     one), started by the first `Parallel`, first in first out. Waiting for a job no worker has started runs it on
     the waiting thread. Is cores-minus-one right for the engine, or should it be every core?
145. **A `Parallel` costs about two dozen allocations**, almost all of them the two `Spite.Function` values (each
     is its own reflection object, D39, with a list of `Spite.Argument`s). Making a function value's reflection
     lazy would cut that to a handful; worth doing for every callback, not only here?
146. **`finished` on a `Concurrent` does not run anything.** It reads the flag; the fiber only progresses when the
     program waits somewhere (`program.sleep(1)` in a polling loop). It could instead let ready fibers run once,
     which would make it a wait point in the D37 sense. Keep it a plain read?
147. **`ThreadLocal<T>`, `Lock` and `ThreadSlot`**: the names, `while_locked(function)` as the main way to hold a
     lock (with `lock()`/`unlock()` kept), and a `ThreadLocal` keeping every thread's value until it is itself
     dropped (no per-thread destructor). A lock that is not reentrant crashes nothing: taking it twice on one
     thread deadlocks. Should a second `lock()` on the same thread be a crash instead?
148. **`parallel_each_`'s rule** (D35 as built): plain-value attributes, library singletons and locals only;
     `filter_` steps allowed, `map_` refused; a list of a `type` refused. Is refusing `List<Particle>` members that
     own a list of their own (`trail.append(...)`) too strict for the engine? The honest alternative is an
     ownership marker on the attribute, which is new syntax.
149. **Reads in a row overlap only among themselves**, and only `File.read`/`Socket.read_line` into a fresh
     untyped name. Should a read also overlap the statements after it until its name is used, which is faster but
     needs the compiler to prove those statements do not touch the file?
150. **`File`'s byte functions**: the names, positions instead of an open handle with a cursor, and each call
     opening the file. A `File.Reader` with its own position and `drop()` closing it would suit record-by-record
     scanning; wanted?
151. **D174's check point** is a call per pass (thread check, two atomic loads). A C-level flag that the REPL's
     thread sets would make it one load; worth the extra hidden code (D147)?
152. **D183 as built locks every call** to a program singleton that can change, from any thread, in a program that
     uses `Parallel`, not only the calls a `Parallel` makes: telling them apart needs a walk of everything a
     `Parallel` can reach. Acceptable as the fallback until D184, or should that walk come first?

## From D186-D188 (the load keyword and kebab-case flags)

153. **Kebab-case for a program's run-time settings too?** D188 makes the compiler's flags kebab-case
     (`--repl-port` sets `Build.repl_port`). A program's `Environment` settings are read after the `--` when the
     program runs, and still match their field's spelling (`spite game -- --player-name=ada`). The compiler could
     write the kebab form into the program as the text it matches (`"player-name"`), which costs nothing at run time
     (D177). Make run-time settings kebab-case as well, so both sides of `--` read alike?
154. **A folder a package `load`s from inside its own tree.** A package `kitchen/` that loads `garnish/pepper` from
     inside itself gets that folder twice: as the namespace `Garnish.Pepper` (every folder of a root is one) and as a
     root of its own. Both compile, and tree shaking drops the unused one, but a folder named by a `load` could
     instead never be a namespace, as the entry folder already does for the folders its own file loads. Also: `Build`
     is read before any package is loaded, so a package's `build.spite` cannot add a field that decides a `load`.

## From SlopEngine on the thread pool

155. **Work that blocks for the program's whole life needs its own thread, not a pool worker.** SlopEngine's
     window thread runs the Win32 message loop forever (messages only reach the thread that made the window); on a
     pool of cores-minus-one it pins a worker for good, and a few such loops on a four-core machine starve the
     engine's stages. Proposal (Claude, unconfirmed): `Thread(function)` -- a dedicated OS thread, same handle
     shape as `Parallel` (`finished`, the value joins on first use, dropping waits) -- for loops that block on the
     operating system, with `Parallel` staying for work that computes. Alternative: a marker on `Parallel`
     (`Parallel(window.run, 'dedicated')`). Which, and what name?

## Found implementing D180 (enums an engine reopens and walks)

156. **Which enum constrains a pattern's hole: the one named for it.** `phase: Symbol<$system_type.phase_all>` reads
     the enum `Phase`, found as a type written `Phase` would be from the template's class (its class, its folders,
     then the whole program if exactly one class declares one); `render_step` reads `RenderStep`. A hole no enum is
     named for is an error at the template, so every pattern needs its enum (the corpus and docs gained one each).
     The alternative is an explicit form such as `Symbol<Phase, $system_type.phase_all>`, which is new syntax. Keep
     the naming rule, and keep it mandatory?
157. **Enum reflection is spelled like the other templates.** `course: Symbol<Course>` ranges over the values;
     `course.name` is the text, `course.value` the value typed `Course` (also inside a pattern template, where
     `phase.value` is the matched `Phase`), and the plural calls it once per value in the enum's order. There is no
     run-time list of values (nothing to tree-shake). Is `.value` the right word, and is a run-time form ever
     wanted (`Course.values`)?
158. **A pattern's plural now walks in the enum's order**, not the order the functions are declared in, since the
     enum is the engine's list of phases. Keep that?
159. **Where reopened values go.** They are appended in merge order, which puts the program's own folder before
     every loaded folder, so a program's `schedule.spite` adding `'input'` to a loaded engine's `Phase` puts
     `'input'` *first*, and a mod loaded after the engine puts it last. A value already present stays where it was
     (so `--final-classes` output, which restates whole enums, still compiles); nothing removes or reorders one.
     Should a reopening be able to say where its values go (before or after another value), and should the
     program's own values come last instead?
160. **`has_function` at run time still matches any text in a hole.** Folded (`$type.has_function("<phase>_each")`)
     the hole is the enum's values, like a template's; `Sprinkler.has_function("<phase>_each")` and `name_fits` at
     run time are plain text questions over `.functions`, since there is no run-time enum list. Keep the two
     different, or make the run-time form read the enum too (which would need one)?

## Constrained generics and `Json(order)` (D175, D138)

161. **How `Json` reads** (D138: "Reading keeps a way to name the class it reads into"). Built as one constructor
     taking a `$value_type?`: `Json(order).write()` to write, and `Json<Order>(null).read(text)` to read -- the
     class named, and `null` for the object there is not yet. A `T?` argument gives `Json<T>`, whose `write()` of an
     empty value is `null`, and `read` makes a new value, so one `Json(order)` also reads. The alternatives: a
     second class for reading (`JsonReader<Order>`, the cursor being renamed), or `Json(Order())`, which names the
     class through a throwaway default object. Keep `Json<Order>(null)`?
162. **What fits a constraint** (proposed by Claude, unconfirmed). The same rule a `type` already admits values
     by: a class with the functions and attributes, `String`, numbers, enums, a `List<T>` or `Dictionary<T>` with
     what the `type` needs, and the `type` itself. A `T?` does not fit (`Shelf<Int?>` for a `Printable` shelf is
     an error), nor does a function value. Should a `T?` fit when every use narrows it, and should a union be
     allowed as a constraint ("one of these classes")? Neither is built.
163. **After a constraint error the generic is compiled with the `type` in place of the class**, so its body adds
     no errors, and the error is given once per class, where that instance is first made (a second
     `Shelf<Pet>` elsewhere in the program is not reported again). Report every use site instead?
