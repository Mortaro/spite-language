# Decisions waiting on Mortaro

Agents add here what only Mortaro can decide; Mortaro answers inline or in `mortaros_notes.md`, and an agent
moves the answer into the docs (the page that teaches it, and a row of `docs/decisions.md`) and removes the
item. Each item links to where the docs argue it.

## Open questions still open in docs/open_questions.md

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
    compiler's own discovery still reads names)? `docs/standard_library.md`, "System classes".
23. **`Json<T>`'s API**: `Json<Order>().write(order)`, `.read(text)` (`Order?`) and `.read_or_crash(text)`. This
    settles D22's open naming pair as `read`/`read_or_crash`, unless you prefer `to_json`/`to_crashing_json` or
    `parse_json`/`parse_json_or_crash`. Also `Json` itself: JSON is an abbreviation, but it is the format's name.
24. **Reading foreign JSON**: unknown keys are skipped, missing attributes keep their defaults, and a value of the
    wrong kind makes the whole read `null`. The alternative for missing attributes is to fail as well.
25. **The metaprogramming `Json` needed** (`docs/values_and_types.md` and `docs/metaprogramming.md`): `attribute: Symbol<Label>` for a template
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

## Build, the launcher and the entry (D85, D86, D89, D97; `docs/programs.md` and `docs/compiler.md`)

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
## Visible storage and placement (D107, D108; `docs/values_and_types.md`, `docs/memory.md` and `docs/standard_library.md`)

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
## Live reload (D111, D112; `docs/repl.md`, "Live reload and 6b")

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

## From D109 (printing through `to_string()`, `Console.debug`; `docs/functions_and_operators.md`, `docs/metaprogramming.md` and `docs/standard_library.md`)

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

## From the SlopEngine bug batches (`docs/functions_and_operators.md`, `docs/values_and_types.md`, `docs/metaprogramming.md` and `docs/packages.md`)

81. **A Symbol template and another class's private attributes.** `func fill_attribute(attribute:
    Symbol<Target>, ...)` in `Query` cannot read `target._cache`: `_` is private to `Target`, and the template
    belongs to `Query`, so the single call `fill__cache(...)` is the privacy error and the plural skips `_cache`
    (item 78's change). A template is metaprogramming acting on `Target`'s shape, so it could be allowed to see
    what `Target` sees -- or privacy could stay absolute, as today. The rule is unchanged; which one?
83. **Text holes call `to_string()`** (answered provisionally): a text hole now calls a class's `to_string()` (`"{ticket}"` works, and
    `"{attribute.class}"` prints the type's name), the way `console.print` does. Keep it, or require the call to
    be written?
84. **The default of a `type` is an object literal** (open question 1, `docs/open_questions.md`): `var row: $row_type = null` bound
    to `type Target { health: Health }` now holds `{health: Health()}`, whose `.class` answers `Object`. A `type`
    that requires a function has no default object and stays a null pointer that reads defaults. Should that
    case be a compile error naming the field instead?
## From the Vulkan renderer bugs (`docs/packages.md`, `docs/style.md`, `docs/compiler.md`, `docs/standard_library.md` and `docs/foreign_libraries.md`)

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

## From D114, D115 and D116 (compile-time reflection over functions, folders and names; `docs/metaprogramming.md`)

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

108. **Passing a template's symbol to a helper** (proposed by Claude, unconfirmed; `docs/metaprogramming.md`). A function
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

## From D127 (dates, times and time zones; `docs/time.md`)

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

## From building D194 (the file and folder watcher; `docs/standard_library.md`)

166. **What is the watcher called, and are its members right?** Built as `Watcher` (`library/watcher.spite`, with
     `library/<system>/watcher.spite`), proposed by Claude, unconfirmed. D194 asked for a better name than
     `FileWatcher`, since it watches folders too. Alternatives: `Watcher` (short, but says nothing of files --
     a game may want its own `Watcher`), `FileSystem.Watcher` (namespaced; there is no `FileSystem` namespace
     yet, and `File` and `Directory` would not move into it), `PathWatcher` (says what it takes and what it
     answers: paths), `Directory.Watcher` (reads as a folder only). The members are also proposed:
     `watch(path): Bool`, `changes(): List<String>` (never waits; answers once no change has been seen for 100 ms)
     and a third, `wait_for_changes()`, which blocks the calling thread in the operating system until `changes()`
     has something -- added because `HotReload`'s thread must sleep in the kernel rather than poll `changes()`.
     Keep `wait_for_changes()` public, and should the 100 ms be a constructor argument?

## From building D143 (inspectable and production builds; `docs/classes_and_files.md` and `docs/compiler.md`)

142. **Which builds are "production"?** D143 says internals are hidden "at optimized production builds" and
     ordinary in `--repl`, `--repl-port`, `--hot-reload` and `--development` builds. Built (proposed by Claude,
     unconfirmed): every build that is not one of those four is production, the plain `spite program` included, so
     an ordinary build keeps the static singletons and the tree shaker it had; `--optimized --repl` is inspectable.
     Only tree shaking and static singletons count as hiding an internal; fused chains, placement and text appended
     in place stay on in every build, since they change speed rather than what reflection sees. Keep that, or should
     a plain build be inspectable too, leaving hiding to `--optimized` alone (every ordinary build then allocates
     `Memory` and `Build` and carries unshaken C)?

## From D134 and D135 (the thread pool and join on first use; `docs/concurrency.md`)

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
     lazy would cut that to a handful; worth doing for every callback, not only here? *Built lazily for every
     function value (2026-09-25, proposed by Claude, unconfirmed): `.arguments` is filled on first read, and the
     discarded `Spite.Class('Nothing')` defaults are no longer made -- a `Parallel` makes 9 fewer allocations. Nothing
     a program reads changed, so there is nothing left to decide unless you want it eager again.*
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
156. **A D183 lock is held for the whole call, so a singleton function that never returns locks the singleton
     forever.** The lock is taken around each of the singleton's functions and is reentrant (a call it makes to
     itself on the same thread does not wait). SlopEngine's window loop was `Windows.Owner.run()`, a function that
     never returns, so every other call on `Owner` from the frame thread (`owner.take_events()`) waited for good;
     SlopEngine moved its loops into plain objects. Options (Claude, unconfirmed): (a) keep it, and make a loop
     that cannot end inside a locked singleton's function a compile error naming the problem; (b) lock around the
     reads and writes of the singleton's attributes rather than around whole calls -- finer, never held across a
     wait, but two reads in one function no longer see one consistent state; (c) D184's cheaper forms first, which
     make most singletons need no lock at all. Which?
157. **Bytes nobody wrote, and output lost to a crash.** SlopEngine's "run mode segfaults, the built executable
     works" was neither run mode nor the pool: `Added.fill_from` reads a marker column's value slot, which `append`
     never writes, and `Memory.resize` hands back whatever the heap held. What it held depended on the process's
     environment block (Git Bash's crashes every time; a minimal one, or PowerShell's, passes), so the launcher
     decided which run crashed. And in run mode the program's stdout is a pipe, fully buffered, so the crash also
     threw away everything printed before it. Proposals (Claude, unconfirmed), both costing nothing outside the
     builds named: (a) under `--debug-memory` fill every byte `allocate_bytes` and `resize` hand out with a poison
     pattern, so reading unwritten memory fails the same way on every run and every launcher (D143); (b) should a
     program flush its output when it dies from a hardware fault (an exception filter in `main`: a few lines of C
     in every program), or is losing buffered output on a segfault acceptable? (SlopEngine's session: poisoning would have caught its marker-slot bug on the first run, and it is for it.)

## Found implementing D180 (enums an engine reopens and walks)

158. **Which enum constrains a pattern's hole: the one named for it.** `phase: Symbol<$system_type.phase_all>` reads
     the enum `Phase`, found as a type written `Phase` would be from the template's class (its class, its folders,
     then the whole program if exactly one class declares one); `render_step` reads `RenderStep`. A hole no enum is
     named for is an error at the template, so every pattern needs its enum (the corpus and docs gained one each).
     The alternative is an explicit form such as `Symbol<Phase, $system_type.phase_all>`, which is new syntax. Keep
     the naming rule, and keep it mandatory?
159. **Enum reflection is spelled like the other templates.** `course: Symbol<Course>` ranges over the values;
     `course.name` is the text, `course.value` the value typed `Course` (also inside a pattern template, where
     `phase.value` is the matched `Phase`), and the plural calls it once per value in the enum's order. There is no
     run-time list of values (nothing to tree-shake). Is `.value` the right word, and is a run-time form ever
     wanted (`Course.values`)?
160. **A pattern's plural now walks in the enum's order**, not the order the functions are declared in, since the
     enum is the engine's list of phases. Keep that?
161. **Where reopened values go.** They are appended in merge order, which puts the program's own folder before
     every loaded folder, so a program's `schedule.spite` adding `'input'` to a loaded engine's `Phase` puts
     `'input'` *first*, and a mod loaded after the engine puts it last. A value already present stays where it was
     (so `--final-classes` output, which restates whole enums, still compiles); nothing removes or reorders one.
     Should a reopening be able to say where its values go (before or after another value), and should the
     program's own values come last instead?
162. **`has_function` at run time still matches any text in a hole.** Folded (`$type.has_function("<phase>_each")`)
     the hole is the enum's values, like a template's; `Sprinkler.has_function("<phase>_each")` and `name_fits` at
     run time are plain text questions over `.functions`, since there is no run-time enum list. Keep the two
     different, or make the run-time form read the enum too (which would need one)?

## Constrained generics and `Json(order)` (D175, D138)

163. **How `Json` reads** (D138: "Reading keeps a way to name the class it reads into"). Built as one constructor
     taking a `$value_type?`: `Json(order).write()` to write, and `Json<Order>(null).read(text)` to read -- the
     class named, and `null` for the object there is not yet. A `T?` argument gives `Json<T>`, whose `write()` of an
     empty value is `null`, and `read` makes a new value, so one `Json(order)` also reads. The alternatives: a
     second class for reading (`JsonReader<Order>`, the cursor being renamed), or `Json(Order())`, which names the
     class through a throwaway default object. Keep `Json<Order>(null)`?
164. **What fits a constraint** (proposed by Claude, unconfirmed). The same rule a `type` already admits values
     by: a class with the functions and attributes, `String`, numbers, enums, a `List<T>` or `Dictionary<T>` with
     what the `type` needs, and the `type` itself. A `T?` does not fit (`Shelf<Int?>` for a `Printable` shelf is
     an error), nor does a function value. Should a `T?` fit when every use narrows it, and should a union be
     allowed as a constraint ("one of these classes")? Neither is built.
165. **After a constraint error the generic is compiled with the `type` in place of the class**, so its body adds
     no errors, and the error is given once per class, where that instance is first made (a second
     `Shelf<Pet>` elsewhere in the program is not reported again). Report every use site instead?

## Found building D169-D172

167. **D169 covers every proof, not only `[]` reads.** The question (item 106) was about proven `list[...]`, but a
     call could also set a narrowed attribute to `null` (`assert target`, `forget()`, `target.name` compiled and
     then crashed on a null pointer), so a call now undoes narrowed attributes and paths too, by the same rule.
     Right reading? The rule also treats a function value's call as able to change anything, does not follow
     operator functions or getters, and does not track a local that aliases an attribute's list (a proven read
     still checks its bounds). Should any of those be followed too?
168. **D171 names member templates only.** D148's function values for `each`/`map`/`filter` are not built, so a
     `while` that only passes each element to a function of another object (`evaluator.process_line(line)`) is not
     an error yet, and D113's rule still names `list.each_f()` for a function of the caller. Once D148 is built,
     should D171 name `list.each(f)` for those loops, replacing D113's rule?
169. **D172's "visible" function names** are read as the functions of the variable's own class (its file and its
     reopenings), which is what a bare name reaches. Should a variable also be kept from the name of a function of
     a class it only uses (`var print = ...` beside `console.print`)?
170. **D170's message names `switch` for unions only**, since `switch` does not take an enum. Decide enum
     switches (review section 2), so the message can say "union or enum" as D170 wrote it?

## Found building the `Memory` namespace (D178, D150, D151)

171. **Who may read and write an address.** D178 says the reads and writes of `Memory.Address` are "usable only
     from `library/`". Built as: a function of a class the standard library declares may call them, including a
     function a program adds by reopening that class -- `--final-classes` prints every class it uses into the
     program's own folder, and the printed program must still compile (check.sh runs it). Any other class gets
     an error pointing at the standard library and `TypedMemory<T>`. `copy_to`, `compare_bytes`, `text` and
     `terminated_text` are not reads or writes, so programs keep them. This also means a program such as
     SlopEngine can no longer write a `Float` into a C struct with `write_float`; it uses `TypedMemory<Float>`.
     Keep the reopening reading, or should only files under `library/` count (and `--final-classes` print the
     standard library's classes some other way)?
172. **`Memory.Heap`'s names.** `allocate(bytes)`, `resize(address, bytes)`, `free(address)` and
     `live_allocations()` (was `allocate_bytes`); every class of `library/` binds it as `var heap = Memory.Heap()`.
     Keep, or another spelling?
173. **`Memory.Arena` and `Memory.Frame`.** D151 names `Memory.Heap`, `Memory.Arena` and `Memory.Frame`. Built:
     `Memory.Arena(block_bytes)` hands out memory from chained blocks and frees them all when the arena itself is
     dropped; everything made in it holds it, so it cannot go while they live. Not built: a `reset()` that
     reuses the blocks each frame, and `Memory.Frame`. Both need `mortaros_allocators_proposal.md` question 3
     (an object must not outlive a reset). Is `Memory.Frame` an arena the engine resets once per frame (and
     storing a frame-made object anywhere that outlives the frame a compile error), or something else -- a ring
     of two arenas, say? Until then, is an arena without `reset()` the right first step?
174. **Where `.memory.allocator` may be set.** Built as: only the statement right after `var name = ...` whose value
     makes the object (a constructor, `List<T>()`, `Dictionary<T>()` or `.copy()`), and only to a name or a path
     of names. D152 says "before the object escapes"; a line in between that does not mention the object is
     still an error. Widen it to "any line before the object is first used", or keep "the next line"?
175. **D154's list buffer and `Vector<T>`.** A `List` placed in an arena keeps its buffer of references on the
     heap: for the buffer to follow, `library/list.spite` has to ask its own object for its allocator, and
     Spite has no way yet for a class to read its own `.memory` (`this` only passes the object, D146). Proposal
     (Claude): inside a class, `memory.allocator` reads the object's own allocator, the way a number's `this`
     is its value. Also `examples/vectors` has a user class `Vector`, which a library `Vector<T>` would turn into
     a reopening of the library's class. Name it `Vector<T>` anyway (the example renames its class), or another
     name? Built as `Vector<T>` (`library/vector.spite`, D204's borrowed items): the user classes called `Vector`
     in `examples/vectors`, `conformance/stage6/operators` and `benchmarks/small_allocations` are now
     `Displacement`. The allocator half is not built: a vector's block of items is on the heap wherever the vector
     object is placed, as a list's buffer is, until a class can read its own allocator.
176. **How much of a singleton may be atomics instead of a lock (D184).** Built as: a singleton whose changing
     attributes are whole numbers or `Bool`s, where each function touches that state once (one read, one
     `x = x + step`, or one `flag = value`), is compiled to atomics and takes no lock; that keeps every function one
     indivisible step, exactly as the lock did. A function touching two counters (`hits = hits + 1` then
     `total = total + size`) falls back to the lock, because another thread could read between the two. Keep the
     one-touch rule, or accept that each line is atomic on its own and drop the lock there too? And `Float`
     counters: C has no atomic add for them, so they need a compare-and-swap loop -- worth it, or keep the lock?
177. **A singleton's attribute written or read from another class.** D183 guards a singleton's functions. Built:
     a write from another class (`registry.last = name`) takes the singleton's lock, and makes the singleton one
     that changes, so it is never atomics or nothing. Not built: a read from another class (`registry.last`)
     takes nothing, and `counter.hits = counter.hits + 1` from outside is a read and a write, two steps with
     another thread free to run between them. Keep this, or make either an error in a program where a `Parallel`
     reaches the singleton ("call a function of 'Registry' instead")?

## Found building D176's state machines

178. **A wait inside an expression runs before the rest of its statement.** Built as: in
     `log.append("{name} read {file.read()}")` inside a `Concurrent`, the read happens first and `name` is read
     after it, so the state machine can stop at a statement boundary (a C statement expression cannot be jumped
     back into). Arguments of one call keep their written order otherwise. Keep, or should the compiler also move
     every earlier part of the statement into temporaries first, so the written order holds exactly (more frame
     fields per wait)?
179. **Waits that run the loop in place instead of returning.** Inside a `Concurrent`, a wait in the right side of
     `and`/`or`, one reached through a function value or a constructor, and dropping a `Concurrent` (join on drop)
     are not points the state machine returns from: they run the event loop right there, which keeps every other
     `Concurrent` going but holds this one (and anything under it on the C stack) until the wait is over. Two such
     waits that each wait for the other would never end; nothing reports it. A `Concurrent` whose own function
     cannot be a state machine -- a function value made in a standard-library class and stored before it reached
     `Concurrent`, a shape's function, a singleton function that takes a lock for `Parallel` (D183), or any function
     of a program class in a `--hot-reload` build, which is called through a swappable slot -- runs to its end when
     it is started. Close each gap (a function value carrying its state machine's start, `and`/`or` lowered to
     `if`, drops at scope end written as waits), or accept them as they are?
180. **The event loop waits on one OS event, and each IO call in flight takes a helper thread.** Chosen over
     IOCP/epoll/kqueue because an ordinary file is always "ready" to epoll and kqueue, the Windows console cannot
     be read through IOCP, and one mechanism keeps each system's folder small. A server holding thousands of open
     sockets would hold thousands of threads. Move `Socket` alone to the system's readiness (IOCP on Windows,
     epoll/kqueue elsewhere) when HTTP is built, or keep one mechanism?
181. **Item 146 again, now that there are no fibers.** `finished` still only reads a flag, and a state machine only
     moves when the program waits somewhere. Making `finished` run the ready state machines once would be cheap
     now (no stack switch). Keep it a pure read?

## Data-oriented components (D203, from SlopEngine)

183. **A fixed-size text for inline components.** With D203, text up to 15 bytes is inline (built: a `String` is
     sixteen bytes; 22 would have made it twenty-four, see `docs/optimizations.md`), so `Vector<Name>` has a
     fixed stride and only a longer name points out to the heap. Is that enough, or do you want a capped
     `ShortText<32>` that refuses longer text?

## Found fixing the docs pass's shortfalls

184. **D174 and a local `--repl` build.** D174 lists `--repl` among the builds that get loop check points, but a
     local REPL reads the console only after the entry constructor returns, so a check point there would answer
     nothing. Built as: no check points in a `--repl`-only build (docs/control_flow.md). Right, or should a local
     REPL become able to interrupt a running loop (which would mean reading the console on another thread)?
185. **D144's exceptions.** Built as: `String`, the numbers, `List`, `Vector` and `Dictionary` may bind a singleton as a
     local, and so may any function whose singleton's codegen value comes from its own codegen or Symbol
     (`Query<argument.class>()`). Binding as an attribute also moves when a singleton is made, to when the object
     holding it is made. Keep these, or narrow them?
186. **D181, which folders.** Built as: the program's folder, every folder inside it or a loaded root that holds a
     `.spite` file, and a loaded root's own folder; not the folders a `../../plugins/render_vulkan` path passes
     through. Should those be checked too?
187. **Kebab-case after `--` (item 153 still open).** Built as a stop-gap: a declared setting given kebab-case
     stops the program naming the snake_case spelling. When 153 is decided this becomes either the accepted form
     or stays the error.
188. **`$value_type == Enum`.** Json needs to tell an enum from a plain `Symbol` (the text cast reads the one,
     `Symbol(text)` the other), and `$value_type == Symbol` is true for both. Keep the new codegen word `Enum`, or
     spell it another way?
189. **Json and a union or a `type`.** Built as a compile error at the program's line. D22's refinement makes
     writing total, so the error is the decided outcome; the open part is whether a union should instead be
     written with its member's class name as a key (`{"Cat": {...}}`) so it can be read back.
190. **A written `Parallel<T>`/`Concurrent<T>` type.** Built as: an error only on the `var` that makes the handle;
     a list's element type and a parameter still write it. Right?
191. **A program's `target_operating_system`.** Built as an error naming the flag. The alternative is letting a
     program's `build.spite` choose its default target, which needs `Build` read before the launcher picks
     `library/<system>`.
192. **D179, how strict.** Built as: a `Parallel(work)` may not reach an attribute holding a list or an object,
     even one only its own instance holds (a `Pump`'s own buffer list), since the compiler cannot tell owned from
     shared; `Lock` and `ThreadLocal` are allowed as made-to-share. Loosen it to "its own instance's attributes of
     any type" (which admits a shared `Meter`), or keep it strict? **SlopEngine's experience (2026-09-26):** the
     strict rule made it rewrite four pool tasks into busywork -- `CookTask` carries its recipe ids as comma-joined
     text and parses them back, `ChangeCheck` carries two lists as newline-joined text, `TextureLoad` copies a
     record's three fields out and rebuilds it inside `run`, `DialTask` returns its socket instead of keeping it.
     Two middle grounds it proposes (Claude, unconfirmed): (a) allow an attribute whose object nothing else uses
     after `Parallel(...)` is made -- the compiler proves the task's instance is the only holder, the way D204
     proves a borrow, and a use afterwards is the error; (b) allow a `List` of values (numbers, text, enums), which
     cannot be shared by reference.
193. **`first()`/`last()` as `T?`.** Built, by the same argument as `[]` (a missing element is a normal outcome, D199).
     `remove_first()`/`remove_last()` still answer the default on an empty list; make them `T?` too?
194. **`Weak<T>` across threads.** A `Weak` read on one thread while another frees the object is a race today;
     the table has no lock. Guard it in a program that uses `Parallel`, or forbid a `Weak` a `Parallel` reaches?

## `Socket` for game servers (from SlopEngine)

195. **How a closed peer shows.** Built as a `closed: Boolean` attribute: `read_bytes_now` answers `0` both for
     "nothing yet" and for "the other end hung up", `read_line_now` answers `null` for both, and `closed` tells
     them apart (D24's "a distinction that matters is data"). The alternative was a count of `-1`, which every
     caller would have to remember to test before adding the count to a length. Keep `closed`?
196. **The names of the calls that never wait.** Built with a `_now` suffix beside the waiting calls:
     `accept_client_now(): Socket?`, `read_line_now(): String?`, `read_bytes_now(address, count): Integer`,
     `write_bytes_now(address, count): Integer`; and `listen_everywhere(port)`, `listen_at(host, port)`,
     `connect(host, port)` beside `listen_locally` and `connect_locally`. Other spellings weighed: a mode on the
     socket (`socket.waits = false`), which would make the same call mean two things, and `poll_`/`try_`
     prefixes. Keep the names?
197. **Every printed line is written out at once.** `print`, `error` and `debug` now flush, so a server's log
     redirected to a file shows each line as it happens. Measured with `benchmarks/console_lines` (200 000 lines,
     Windows): about 700 ms to a file against about 140 ms buffered until exit; the same to a pipe or the null
     device. Nothing cheaper shows every line promptly without a thread. Keep it for every program, or should a
     program be able to say it prints to a file nobody watches (a build setting, say) and keep the buffer?

## Short text inside the `String` (D203)

198. **What `.memory.section` says for short text (D203).** Text of up to 15 bytes lives in the sixteen bytes of
     the `String` itself, so its characters are wherever the value is. Built (Claude, unconfirmed): `'stack'` for
     a local, `'heap'` read from an attribute, `'constant'` for a literal as before, and `'heap'` for long text.
     Would you rather have a fourth section, `'inline'`, that says the characters are in the value, wherever it
     is?

## Vector columns in SlopEngine (D204)

199. **May a `type` row hold borrowed `Vector` items for one system call (D204)?** SlopEngine's `Row` fills a
     `type` row (`moving.position`, `moving.velocity`) with one entity's components and hands it to the system's
     `update_each`; with `Vector` columns those are borrowed items, and D204 forbids keeping a borrow in an
     attribute. Proposal (Claude, unconfirmed): a `type` value made and dropped inside one statement block may hold
     borrows for that block -- the compiler proves the row is not kept (not stored, returned or captured) and that
     no column is resized while it lives, the same proof as a local borrow. Measured on a stress-shaped program:
     5.9 ms per tick with `List` columns, 1.0 ms with `Vector` columns. Without it, systems must be written as
     member templates on the column (`velocities.each_integrate(delta)`), which cannot see two components at once.
     **SlopEngine's case for it:** every structural change (create/add/remove component, despawn) is a queued
     command applied by `world.flush()` after the whole stage, and two systems touching one component class never
     share a stage, so a column never resizes during a system call -- the compiler can check it, since only
     `flush` mutates `Column<T>`'s storage. Member templates see one component class at a time, so joins, relations
     (`owner: Entity`) and `Added<T>`/`Removed<T>` filters would need engine-generated glue; a row holding borrows
     costs the same as a template plus the join. Narrowest version it proposes: a borrow may be held by a `type`
     value that is a parameter of the running function, for that call only, never stored anywhere else -- exactly
     the `update_each(row)` shape.

