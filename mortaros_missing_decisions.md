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

## What the standard library offers

21. **Go's standard library against Spite's**, package by package, with a suggested order of what to add:
    [mortaros_go_standard_library_comparison.md](mortaros_go_standard_library_comparison.md).

## From D93 and D95 (directories and JSON)

22. **`Directory.Entry`** is the name of the union of `Directory` and `File` that `entries()` answers; folders
    come first, then files, each sorted. Should `files()` and `folders()` go now that `entries()` exists (the
    compiler's own discovery still reads names)? `docs/standard_library.md`, "System classes".
25. **The metaprogramming `Json` needed** (`docs/values_and_types.md` and `docs/metaprogramming.md`): `attribute: Symbol<Label>` for a template
    over another class's attributes; the plural (`show_attributes`) to call a template for every attribute;
    `$value_type == List` / `Dictionary` / `Null` / `Symbol` as compile-time type tests; `$value_type.element_type`
    to name what a container holds; text casting to an enum by name. Each is a new form, so each wants a yes or no.

## From D82, D83, D88, D92, D98, D100 and D101 (the compiler's reopening, numbers, reflection, memory)

34. **`from_type` is an instance function on the class cast to** (as D100 wrote it), so calling it by hand reads
    `0.0.from_int(count)`; the compiler calls it for every number cast. A cast written in Spite inside a reopened
    `from_type` would call itself, so a reopening can only replace it with the same C cast. A class-level form
    (`Float.from_int(count)`, D6's class object) would read better.
36. **Unions of numbers**: should a union admit number classes? `Json` tests for a number with ten
    comparisons (`$value_type == Int or $value_type == Long or ...`) because it cannot today. With D83 every number is a class in
    `library/`, so a union of number classes is no longer a union of names that are not classes.

## Build, the launcher and the entry (D85, D86, D89, D97; `docs/programs.md` and `docs/compiler.md`)

39. **The C left in `main`**: handing `argv` to `Arguments()`, binary standard output on Windows (`_setmode`), and,
    after `Launcher` returns, releasing singletons and class objects and printing the `--debug-memory` report.
    Moving them into Spite needs a way for Spite to receive `argv` and to run code after the program ends (a
    `Launcher` that releases what the program left?) -- which is a language question.
## Found while auditing docs/ against the manual (2026-09-24)

Behaviour that does not match the manual. The language was not changed; each is pinned or noted in
`docs/KNOWN_ISSUES.md` so a fix shows up there.

46. **Singletons with arguments.** Section 8 ("Singletons") says one instance per distinct literal argument list;
    the compiler rejects any singleton constructor with parameters (`diagnostics/singleton_arguments`) except
    `DynamicLibrary`'s, which keeps the per-argument-list behaviour. Which is the rule, and is `DynamicLibrary` the
    exception or the rule?
## Live reload (D111, D112; `docs/repl.md`, "Live reload and 6b")

64. **A changed attribute or enum is refused, with an error saying to restart.** D111 says a change rebuilds "what
    depends on their layout", but instances already in memory have the old layout. Migrating them -- a new object
    per instance, attributes copied by name, new ones taking their defaults -- needs every live instance and every
    reference to each, which nothing finds today (`Class.instances` is not built, and references would have to be
    re-pointed). Keep refusing, build migration (and how should references be found: a per-class instance list, or
    one level of indirection per object in `--hot-reload` builds), or something else?
68. **Names**: `reload` and `last_reload` at the prompt, `--mode=reload` for the compiler, and `rebuilt A, B` /
    `removed A.f` / `nothing changed since the code the program runs` as answers. When the watcher swapped a save
    in before `reload` arrived, `reload` answers `nothing changed`, and `last_reload` tells what happened. Keep?
## From D114 (compile-time function reflection, for SlopEngine)

71. **Finding every function named `*_system` across the program** -- the same reflection over the program's
    classes. Not decided.

## From D109 (printing through `to_string()`, `Console.debug`; `docs/functions_and_operators.md`, `docs/metaprogramming.md` and `docs/standard_library.md`)

76. **An enum value answers `to_string()` and `to_debug()` and nothing else**, which is what lets it be
    `Printable` and `Debuggable`. Should an enum be able to declare more, the way a number's class does (a file
    per enum is not a thing yet)?
79. **The REPL could show values with `to_debug()`**: today it shows `Player { name: hero, ... }` (text unquoted,
    nested objects as `Name {...}`, lists as `List<String>(...)`) from `Spite.Attribute`'s text, and the
    documented sessions depend on that. Switching means quoting text and nesting fully in every answer. Want it?
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
93. **A foreign call with a header goes through the header's prototype.** C checks the count and converts each
    argument; converting an integer to a pointer (a `Long` handle) is the one complaint silenced, so a `String`
    passed where C wants an `int` is converted silently too. A C function returning `void` reads as `0`. Every
    called function must be declared by the header, so a header that does not declare one is a C error at build
    time. Without a header, integers cross as 64 bits. Fine, or should the header be required whenever a call
    passes anything but a `Long`?
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
102. **What `has_function` counts**: folded, the functions a class declares (not its constructor, not a `_`
    function, not what a template generated for it); at run time, `.functions`, which includes what templates
    generated. The two can disagree for a template-generated name. Align them, and which way?
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

## Outputs and paths (D128, D129, D130)

134. **How outputs are chosen.** Built as `Bool` fields of `Build` -- `run` (default `true`), `executable`,
     `c_source`, `format` (default `true`) -- plus `final_classes` as a folder (proposed by Claude, unconfirmed).
     Because `run` defaults to `true`, asking for another output also runs the program unless `--run=false` is
     given: `spite game --c-source --run=false` for the C alone. The alternatives: a program that names any output
     on the command line gets only the outputs it named (shorter, but a flag then changes another flag's default),
     or one list field, `--outputs=executable,c_source`. Keep the `Bool`s?
136. **Where a run's executable goes.** With no flag it is built beside the program (`game/game.exe`, and
     `game.crashes`), which D129's "build to the same folder" reads as, so every run leaves those two files in the
     program's folder (`.gitignore` now ignores `*.crashes`). The one intermediate, the C compiled when `--c-source`
     is off, goes to the language repository's `.spite-cache/<name>.c`. Should a plain run (no `--executable`)
     build into the cache instead and leave the program's folder untouched? A `--hot-reload` build beside its program
     also puts its reload libraries in the folder the watcher watches, so each reload wakes it once more.
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

## From D186-D188 (the load keyword and kebab-case flags)

153. **Kebab-case for a program's run-time settings too?** D188 makes the compiler's flags kebab-case
     (`--repl-port` sets `Build.repl_port`). A program's `Environment` settings are read after the `--` when the
     program runs, and still match their field's spelling (`spite game -- --player-name=ada`). The compiler could
     write the kebab form into the program as the text it matches (`"player-name"`), which costs nothing at run time
     (D177). Make run-time settings kebab-case as well, so both sides of `--` read alike?
## From SlopEngine on the thread pool

155. **Work that blocks for the program's whole life needs its own thread, not a pool worker.** SlopEngine's
     window thread runs the Win32 message loop forever (messages only reach the thread that made the window); on a
     pool of cores-minus-one it pins a worker for good, and a few such loops on a four-core machine starve the
     engine's stages. Proposal (Claude, unconfirmed): `Thread(function)` -- a dedicated OS thread, same handle
     shape as `Parallel` (`finished`, the value joins on first use, dropping waits) -- for loops that block on the
     operating system, with `Parallel` staying for work that computes. Alternative: a marker on `Parallel`
     (`Parallel(window.run, 'dedicated')`). Which, and what name?
## Found implementing D180 (enums an engine reopens and walks)

161. **Where reopened values go.** They are appended in merge order, which puts the program's own folder before
     every loaded folder, so a program's `schedule.spite` adding `'input'` to a loaded engine's `Phase` puts
     `'input'` *first*, and a mod loaded after the engine puts it last. A value already present stays where it was
     (so `--final-classes` output, which restates whole enums, still compiles); nothing removes or reorders one.
     Should a reopening be able to say where its values go (before or after another value), and should the
     program's own values come last instead?
## Found building D169-D172

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
173. **`Memory.Arena` and `Memory.Frame`.** D151 names `Memory.Heap`, `Memory.Arena` and `Memory.Frame`. Built:
     `Memory.Arena(block_bytes)` hands out memory from chained blocks and frees them all when the arena itself is
     dropped; everything made in it holds it, so it cannot go while they live. Not built: a `reset()` that
     reuses the blocks each frame, and `Memory.Frame`. Both need `mortaros_allocators_proposal.md` question 3
     (an object must not outlive a reset). Is `Memory.Frame` an arena the engine resets once per frame (and
     storing a frame-made object anywhere that outlives the frame a compile error), or something else -- a ring
     of two arenas, say? Until then, is an arena without `reset()` the right first step?
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
## Data-oriented components (D203, from SlopEngine)

183. **A fixed-size text for inline components.** With D203, text up to 15 bytes is inline (built: a `String` is
     sixteen bytes; 22 would have made it twenty-four, see `docs/optimizations.md`), so `Vector<Name>` has a
     fixed stride and only a longer name points out to the heap. Is that enough, or do you want a capped
     `ShortText<32>` that refuses longer text?

## Found fixing the docs pass's shortfalls

187. **Kebab-case after `--` (item 153 still open).** Built as a stop-gap: a declared setting given kebab-case
     stops the program naming the snake_case spelling. When 153 is decided this becomes either the accepted form
     or stays the error.
188. **`$value_type == Enum`.** Json needs to tell an enum from a plain `Symbol` (the text cast reads the one,
     `Symbol(text)` the other), and `$value_type == Symbol` is true for both. Keep the new codegen word `Enum`, or
     spell it another way?
## `Socket` for game servers (from SlopEngine)

197. **Every printed line is written out at once.** `print`, `error` and `debug` now flush, so a server's log
     redirected to a file shows each line as it happens. Measured with `benchmarks/console_lines` (200 000 lines,
     Windows): about 700 ms to a file against about 140 ms buffered until exit; the same to a pipe or the null
     device. Nothing cheaper shows every line promptly without a thread. Keep it for every program, or should a
     program be able to say it prints to a file nobody watches (a build setting, say) and keep the buffer?

## From SlopEngine's MongoDB driver

200. **A default that is a real answer, under D106.** `if document.is_empty() { return 0 }` in a size function
     is an error naming `assert not document.is_empty()`, though there 0 means "zero bytes", not "no answer".
     Built, with nothing exempted: the message now says the default "is how a guard is written" and names the way
     to give a real 0 -- a local set in an `if`/`else` and returned once (`docs/failure.md`). Should functions
     whose default is a real answer be exempt, and if so, how would the compiler recognise one (a name such as
     `size`/`count`/`length`, a function returning a number that never returns `null`-like absence elsewhere, a
     mark written on the function)?

## `JsonWriter`/`JsonReader` and `BinaryWriter`/`BinaryReader` (D208)

202. **The names.** Built (Claude, unconfirmed): `append_to(bytes)` for writing onto a buffer the program has,
     `read_memory(address, count)` for reading a socket's buffer without a copy, `position` and `remaining()` on the
     reader, and no `read_or_crash()` for bytes (JSON keeps it). Keep them?

## Which list is the default (Mortaro, relayed by SlopEngine)

198. **Should the everyday list keep its items inline?** Mortaro: "are List in spite just a list of pointers? if yes
     its a bad default for most assets since we need continuous memory cache". Today `List<Integer>` and other
     number lists are contiguous, but `List<SomeClass>` holds references to objects spread through the heap, and
     `Vector<SomeClass>` (D154, D204) is the inline form: one block, no header or count per item, items borrowed
     rather than shared. Options (Claude, unconfirmed): (a) keep both, `List` for shared objects and `Vector` for
     data, and teach `Vector` first for assets; (b) make the inline form the one called `List` and give the
     reference list another name (`References<T>`?), since most game data is values; (c) let the compiler pick the
     layout per list -- inline when every item is only ever reached through that list, references otherwise --
     behind one name. (c) is the most "zero noise" but a borrowed item and a shared object behave differently
     (D204's keep rules), so the difference would show up as errors rather than as a type name. Which default?

## The maths functions (for SlopEngine's skinning, animation and PBR)

204. **The maths names, and constants answered by the class.** Built as proposed by Claude, unconfirmed
     (`docs/standard_library.md#maths--implemented`): members of the number classes, `angle.sine()`, each the C
     library's function written where it is called. To confirm or rename:
     - `rise.arc_tangent_over(run)` for C's `atan2(rise, run)`. Other readings: `rise.arc_tangent_of(run)`, or
       an `angle()` on a future vector type instead.
     - `logarithm()` for the natural logarithm, beside `logarithm_base_2()` and `logarithm_base_10()`; or
       `natural_logarithm()`.
     - `euler_number()` for e, since a name is never one letter; `pi()` and `tau()` kept.
     - `largest()` and `smallest()`: `Float.smallest()` is the most negative finite `Float`, like
       `Integer.smallest()`, not C's `FLT_MIN` (the smallest positive normal one), which is not built.
     - `exponential()`, `truncate()`, `ceiling()`, and `round()` rounding half away from zero (C's `round`).
     - **Constants are functions the class object answers**, `Float.pi()`, the one thing a number class answers
       on its name: `angle.pi()` is an error naming it. The other shapes were an attribute of the class object,
       `Float.pi` (like `Spite.Class.instances`), or a singleton `Maths`, which the brief ruled out. Keep
       `Float.pi()`?
     - `minimum`, `maximum` and so `clamp` follow C's `fmin`/`fmax`: a not-a-number operand is ignored, so
       `nan.clamp(0.0, 1.0)` is `0`. IEEE 754-2019's `minimum` passes not-a-number on instead, which D200's "wrong
       maths should not look plausible" leans towards, at a compare or two more per call. Which?
     - D147 wants a library function to become Spite that calls it; these are instead primitives each backend
       lowers, D178's form, because a backend without a C library would bring its own maths anyway. Agreed?
     - Folding them at compile time (`docs/optimizations.md`) makes the compiler itself call the C library's
       maths, so building the seed on Linux now needs `-lm`.

## Game maths (D213)

205. **Vectors and matrices allocate on every answer.** Built (proposed by Claude, unconfirmed): `Vector3` and the
     others are classes, so `position + velocity.scaled(delta)` makes two heap objects and frees one
     (`benchmarks/game_maths`: 26 ms and two allocations per step for a million steps; clang removed them only
     where nothing but a sum survived). Ways to remove them, none built: (a) extend D108's placement to a class
     instance that never outlives its statement or loop pass, so the temporary lives in the frame; (b) let a class
     of only numbers be a value kept inline like a number (D149 makes every class a reference today); (c) in-place
     twins such as `position.add(moved)`, which cost nothing but double the names. Which, if any? And the parts:
     `x_value`...`w_value` because a name is never one letter -- keep them, or allow `x`, `y`, `z`, `w` on these
     classes as the field's own names (as D213 allows `Vector2`)?
