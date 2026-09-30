# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`docs/decisions.md`, and the item is deleted. Item numbers never change. "D244" marks an item where something can
go wrong silently today; "D205" marks one Claude could decide itself (no syntax, safe, no slower).

Cleaned 2026-09-30: answered by D280 (64), D287 (71), D293 (34), D294 (21), D295 (22), D296 (5), D297 (218);
187 merged into 153.

## Start here

**Waiting in SlopEngine or the Theseus port** (most urgent first): 109, 6, 213, 153, 212, 173, 175, 208/209, 155,
206, and 46 (answered in your inbox, waiting for your confirmation).

**Taste** (names, syntax, how it reads): 6, 9, 10, 25, 36, 39, 76, 79, 89, 90, 93, 98-100, 105, 108, 134, 138, 161,
166, 171, 183, 188, 202, 204, 205, 211, 217, 219, 220, 221, 222, 223, 224, 225, 226.

**Performance, settle by measurement** (D214): 178, 179, 197, 210, 214, 215, 216.

## The items

6. **Open questions 8-11** ([open_questions.md](docs/open_questions.md#open-questions)): an unrelated `get_x()`
   silently takes over reads of `.x` (D244; D205 could pick the stricter option; Theseus L8), `${` in text prints a
   stray `$` (D244; D205 could make it an error), where `--final-classes` shows which root supplied a declaration,
   and whether a `type`'s functions are written as function-valued attributes.
9. **The floor**: what stays C, and how `--final-classes` shows it
   ([standard_library.md](docs/standard_library.md#pure-spite-dissolving-the-runtime--partial)).
10. **Confirm the rows marked "(proposed by Claude, unconfirmed)" from 2026-09-23/24**: the tree shaker, `nan`
    printing as `nan`, the REPL's command names and output, `Environment`'s sources and their order, the containers
    row, D91/D105 (a template's symbol names the element's member; how a chain fuses).
25. **The metaprogramming forms `Json` needed**, built 2026-09-24, each a yes or no: `Symbol<Label>` over another
    class, the plural walk, `$value_type == List`/`Dictionary`/`Null`/`Symbol`, `$value_type.element_type`, text
    cast to an enum by name.
36. **Unions of number classes**: `Json` tests for a number with ten comparisons because a union cannot hold them.
39. **The C left in `main`** (`argv`, `_setmode`, releasing singletons, the `--debug-memory` report): moving it
    into Spite needs a way to receive `argv` and to run code after the program ends.
76. **Enums declaring more than `to_string()`/`to_debug()`**, the way a number's class does.
79. **The REPL showing values through `to_debug()`** (quoted text, full nesting) instead of its own display.
89. **The name `source_folder()`** (D228; D287 added `package_folder()` beside it). Keep both names?
90. **A loaded folder that is itself a program merges into the loader**, silently reopening the loader's classes
    (D244). Proposal: a loaded folder with an entry file keeps its root classes to itself.
93. **A foreign call with a header converts silently through the header's prototype** (a `String` where C wants an
    `int`; a `void` result reads as `0`; D244). Fine, or require a header whenever a call passes anything but a
    `Long`?
98. **D114 as built**: the plural of a template over a function's arguments may be that function's whole argument
    list (`system.run_each(row_arguments())`), one exception to D77. Acceptable?
99. **D115 as built**: `Symbol<System>` walks classes in dotted-name order; a range matching nothing walks nothing,
    so a misspelled plural does nothing (D244); `System.Combat.Hit` below `system/` is not reached. Keep?
100. **D116 as built**: inside a pattern template, `system.phase_each(...)` calls the matched function. Is a member
     name changing meaning there acceptable?
105. **Naming a root class that a nearer one shadows**: a root qualifier (`Root.Plugin`), or fall through only for
     a self-reference? Nothing is built.
108. **Passing a template's symbol to a helper** (`run_combination(phase, combination)`): keep "a parameter whose
     name is not a word of the function's name is reached by passing the symbol", or another spelling?
109. **Which attributes a function reads and writes**, per attribute (D209/D229/D261/D268 answer waits, pieces
     and per-parameter writes). SlopEngine's `Changed<T>` and field skipping and Theseus L6 need it; also what
     becomes of the marker attributes `Resource.World`/`Resource.MainThread`. D205, as D261 was.
134. **How outputs are chosen**: `Boolean` fields of `Build` (`run` defaulting to `true`), or "name any output and
     get only those", or one `--outputs=` list?
138. **Two ways to format**: a compile formats the program, `spite format <path>` formats anything. Keep both?
153. **Kebab-case for a program's settings after `--`** too (`--player-name`)? Today a kebab-case setting is refused
     and names the snake_case spelling (the old item 187's stop-gap).
155. **A dedicated thread for work that blocks forever** (SlopEngine's window loop): `Thread(function)`, or a marker
     on `Parallel`?
161. **Where an enum reopening's values go**: appended in merge order, so a program's own values come before a
     loaded engine's. Let a reopening say where, and should the program's come last?
166. **The watcher's name and members**: `Watcher`, `PathWatcher`, `FileSystem.Watcher`?
     `wait_for_changes()` public? The 100 ms quiet period a constructor argument?
171. **Who may read and write an address**: any function of a standard-library class, including one a program adds
     by reopening it, or only files under `library/`?
173. **`Memory.Frame` and an arena's `reset()`**: an object made in the frame arena may not be stored anywhere that
     outlives the frame (a compile error naming `copy()`; a debug generation check as backstop)? Is `Memory.Frame` an
     arena reset once a frame, or a ring of two? Until then, is an arena without `reset()` right?
175. **A class reading its own allocator** (`memory.allocator` inside a class), so a list's buffer follows its arena.
178. **A wait inside an expression runs before the rest of its statement.** Keep, or move the statement's earlier
     parts into temporaries so the written order holds (more frame fields per wait)?
179. **Waits that run the event loop in place** (`and`/`or`, function values, join on drop) can hang each other
     silently (D244). Close each gap, or accept them? D205 where measured no slower.
183. **A capped `ShortText<32>`**, or is D203's 15-byte inline text enough?
188. **`$value_type == Enum`** to tell an enum from a `Symbol`: keep the word `Enum`?
197. **A flush per printed line** costs 5x to a file. Keep for every program, or let a program keep the buffer?
202. **The reader and writer names**: `append_to(bytes)`, `read_memory(address, count)`, `position`,
     `remaining()`, no `read_or_crash()` for bytes.
204. **The maths names**: `arc_tangent_over`, `logarithm`, `euler_number`, `largest`/`smallest`, constants as
     `Float.pi()`; whether `minimum`/`maximum` pass not-a-number on (a compare or two more); maths as primitives each
     backend lowers.
205. **`x_value`...`w_value`** on vectors and quaternions, or `x`, `y`, `z`, `w` as on `Vector2`?
206. **A walk nested inside a walk**, for systems over two row types (relations copy for now).
208. **How a list that falls back to references is reported** (8.1 against 35-40 ms a tick), with no warnings?
209. **Should a `List` own its items, so a kept reference is weak (`T?`)**, or stay an explicit `Weak<T>`?
210. **Is `Float` arithmetic rounded to `Float` after each operation?** 2-3x faster loops, but printed last bits
     change, so not D205.
211. **`ForeignCallback`'s names and two limits**: a call through a dropped context is not caught (D244); a
     `'no_context'` callback must be a singleton's function.
212. **Should `--run=false` alone build the executable?** Today it only checks, leaving a stale executable to run
     (D244). Keep and teach `--executable --run=false`, make checking its own flag or command, or delete the stale
     executable?
213. **The default build's `-O` level**: `-O0` (fast to build, 3-7x slower to run), `-O1`, or units at `-O1`?
214. **`--optimized` at `-O3` or `-O2`**: the measurements pick no clear winner (D205).
215. **The names `tune_for_this_machine` and `translation_units`**; a default build is not split (measured slower).
216. **Cleaning the object cache `.spite/objects`**: by size, by age, or leave it to the user? (D205)
217. **Should unsigned arithmetic that does not fit halt too**, with explicit wrapping functions for hashes? And
     `-fwrapv` for production builds?
219. **Confirm D263-D267** and the names `remove_where`, `truncate`, `swap`.
220. **A `Dictionary`'s `[]` is `get`/`set`**: rename them `get_at`/`set_at` so every `[]` is one function?
221. **A walked row states its reads** with a `crash` line per read, or the walked read halts on its own?
222. **A switch over a foreign status enum** (D272): no `_:` ever, `_:` that may not crash or assert, or `_:` freely?
223. **`Spite.Namespace`'s `.classes` and `.namespaces`** are one node's children split by kind, the shape D295
     removes from `Directory`. Keep them, or one list of children filtered by kind?
224. **The shape D295 needs for `Directory.Entry`** (proposed): the union stays, and both `Directory` and `File`
     answer `kind(): Directory.Kind` with the values `'files'` and `'folders'`, so `entries().filter_files()` reads.
     Values named in the plural, or `'file'`/`'folder'` with another spelling of the filter?
225. **One root pinning two commits of one repository directly** (D296): both versions' classes have the same dotted
     names there, so it stays an error (proposed). Or a way to name each?
226. **What a crash report shows** (D297): the proposal is the condition's operands, the parameters and locals in
     scope and the attributes of the object the function runs on, bounded, and no condition text on the line (the
     `.crashes` map keeps it). Right set, and is dropping the condition text confirmed?
