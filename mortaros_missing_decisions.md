# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change; the list is ordered by what an answer
unblocks, most first. "D244" marks an item where something can go wrong silently today; "D205" marks one Claude
could decide itself (no syntax, safe, no slower). Each item: the question, the options, a recommendation where
one is clear, and what it blocks.

Cleaned 2026-09-30 (second pass, after D314-D317): answered by the reflection redesign (D316/D317,
[proposal section 9](design/proposals/reflection_objects.md#9-what-this-answers-in-mortaros_missing_decisionsmd)):
25, 98, 99, 100, 108, 188, 206, 223; open question 8 (item 6) answered by D315. Rewritten for the new model: 6,
10, 36, 89, 109, 220, 222. Added: 227-239. Earlier: 64, 71, 34, 21, 22, 5, 218 answered; 187 merged into 153; 46
answered by D314.

Third pass, after D318-D325: 229 answered by D318 (on the reflection branch), 230 by D319, 227 by D320 and
D329, 231 by D321, 232 by D322, 89 by D326, 216 by D327, 224 by D330, 208 by D332. Rewritten: 238 (D324), 36 (D319).

Fourth pass, 2026-09-30, by the principles (the moron, anyone who uses Spite, never has to decide, so "leave it
to the moron" is never an answer; everything monomorphizes; get-only attributes; renames by map; REPL and hot-reload
builds favour information over speed): answered 2026-09-30, rows pending: 225 (same-package pins are two loads in
load order), 234 (identical code folded by the compiler), 36 (`type Number`), 202 and 204 (confirmed as proposed,
maths constants as get-only attributes such as `Float.pi`), 205 (`x`, `y`, `z`, `w`). 166's names by D337. 79 by D322's principle (the REPL shows the fuller
`to_debug()`). Narrowed: 161. Moved to "Confirm quickly": 166 (quiet period only), 197, 212, 215.

Fifth pass, 2026-09-30: 109's first half by D335 (narrowed to the runner's markers), 225 by D334. Answered,
rows pending: 228 (`map_` over a predicate is an error pointing at `filter_`/`count_`/`any_`/`all_`; class-qualified
function values and plain `List.map(function)` go), 213 (the default build compiles fastest; hot builds compile
like it, reversing D299's `-O3`, so D299 leaves 239; run-speed-only passes are skipped unless measured to speed up
the whole build), 153 (no `--` separator; settings kebab on the command line, snake in code; a name colliding with
a compiler flag is an error). Narrowed by D244 and one-way: 93, 179, 138. Added: 240-242. 155 by D336.

## Confirm quickly

Each has one answer the principle implies: the moron (anyone who uses Spite, SPITE.md) never has to decide, the
language does. Yes, or reopen.

197. **A flush per printed line** (5x to a file): the language chooses, with no per-program buffer setting (for
     example per line to a terminal, buffered to a file, always flushed at exit and at a crash).
166. **The file watcher's quiet period** (`FileSystemWatcher`): fixed by the language at 100 ms, not an argument.
212. **`--run=false` and a stale executable** (D244): never leave one. The build writes a fresh executable or
     deletes the old one; "teach `--executable --run=false`" is out.
215. **`tune_for_this_machine` and `translation_units`**: not settings; the compiler chooses tuning and the number
     of units (a default build stays unsplit, as measured), so the names go.

## Blocking now

These hold up the D316/D317 migration, the game engine package or the game port.

109. **The runner's marker attributes** (`Resource.World`/`Resource.MainThread`; what a function reads and writes is
     D335's `function.accesses`): keep them as attributes the runner reads, or a class-level marker? D205, as
     D261 was. Blocks: the game engine's runner, the game port's L6.
222. **A foreign status enum** (D272, not built): a C function returning a C `enum` answers a Spite enum made from
     the header, must be used, and is read by a `switch`. May that switch have `_:`? Options: never; `_:` that may
     not `crash`/`assert`; freely. Recommend never (each outcome a written line). Blocks: D272, the game engine's Vulkan
     resize handling. Needs 233.
233. **How the compiler reads a C header** (PLAN milestone 11c area; today it reads none, [foreign_libraries.md](docs/foreign_libraries.md)).
     Needed for D272's enums, a header's types as reflection, and trampoline width checks. Options: (a) a
     declaration parser in Spite over the C compiler's `-E` output; (b) the C compiler's own dump (clang
     `-ast-dump=json`, ties Spite to clang); (c) generated probe C that prints sizes and values. Recommend (a).
     Blocks: 222.
173. **`Memory.Frame` and an arena's `reset()`**: an object made in the frame arena may not be stored anywhere that
     outlives the frame (a compile error naming `copy()`, a debug generation check as backstop)? Is `Memory.Frame`
     an arena reset once a frame, or a ring of two? Until then, is an arena without `reset()` right?
175. **A class reading its own allocator** (`memory.allocator` inside a class), so a list's buffer follows its arena.
209. **Should a `List` own its items, so a kept reference is weak (`T?`)**, or stay an explicit `Weak<T>`?

## Silent today (D244)

236. **Text read as an enum by name** answers the first value when no value matches (D95 (5), still open under
     D244). Options: (a) `"calm".to_mood(): Mood?`, as `to_integer()` answers `Integer?` (D293's `to_` form);
     (b) `Mood.values['calm']` with run-time text; (c) assigning text to an enum halts on no match. Recommend (a),
     with text assigned to an enum allowed only into a `Mood?`, as for numbers.
235. **Testing a `Boolean?` for presence**: it is never a condition and `value == null` is an error, so today only
     `== true` or a `switch` read one. Options: (a) allow `flag != null`/`== null` on a `Boolean?` only; (b) keep
     `switch`; (c) `crash`/`assert` accept a `Boolean?` as a presence test. Recommend (a).
90. **A loaded folder that is itself a program merges into the loader**, silently reopening its classes. Proposal:
    a loaded folder with an entry file keeps its root classes to itself.
93. **A foreign call with a header converts silently through the header's prototype** (a `String` where C wants an
    `int`; a `void` result reads as `0`). Silent is a bug (D244), so "fine" is out: check each argument and the
    result against the prototype (an error naming the C type), or also require a header whenever a call passes
    anything but a `Long`? Recommend the check alone.
179. **Waits that run the event loop in place** (`and`/`or`, function values, join on drop) can hang each other.
     A hang is a bug (D244), so accepting them is out and each gap is closed, D205 where measured no slower. Left
     for Mortaro: a gap whose only fix costs speed.
211. **`ForeignCallback`'s names and two limits**: a call through a dropped context is not caught; a `'no_context'`
     callback must be a singleton's function.
6. **Open questions 9-11** ([open_questions.md](design/open_questions.md#open-questions)): `${` in text prints a stray
   `$` (D205 could make it an error); how `--final-classes` shows which root supplied a declaration; whether a
   `type`'s functions are written as function-valued attributes. (Question 8, `get_x()` taking over `.x`, is
   answered by D315: calling `get_x()` is an error naming `.x`.)

## Confirm what agents decided

240. **Operator-form follow-ups to D315** (the orchestrator's recommendations, not confirmed): `x.type` parses (a
     keyword is allowed after `.`); `Dictionary.set` is covered as `d[k] = v`; a local copy of a read may be
     narrowed. Each: yes, or which to reopen. Blocks: the D315 migration's last call sites.
241. **D334's error for a commit read into two versions**: a commit one package mixes with another commit, while a
     second package pins it on its own, is an error naming both lines (proposed by Claude, unconfirmed). Keep the
     error, or let each package read its own copy? Recommend the error (a commit's files are read once).
242. **Confirm the parts of D316-D335 proposed by Claude**: D317's inflection details and question names, D327's
     eviction (least recently used past a size cap), D328's pluralize rules, D331's three storage steps, D333's
     reload details (settings read again, which `Build` reads stay folded, helpers without a slot), D335's name
     `accesses`. Each: yes, or which to reopen.

239. **Confirm the rows decided by Claude on 2026-09-30**: D292 (a singleton write is not a parameter write), D300 (`wait_reload`), D301 (REPL `describe`, `enums`, `memory`, union walks,
     assignments), D302 (enum numbers fixed for a reload), D303 (breakpoints), D304 (reloaded REPL tables), D305
     (`eval`/`run`), D306 (the program's errors first), D307 (checking a proven fact is an error), D308 (nested `[]`
     reach), D309 (fixed signatures of `drop`, `to_string`...), D310 (nothing after `return`), D311 (casts as
     `to_<type>()`), D312 (`List.set_at` halts out of range), D313 (short symbols inline). Each: yes, or which to
     reopen.
10. **Confirm the rows marked "(proposed by Claude, unconfirmed)" from 2026-09-23/24**: the tree shaker, `nan`
    printing as `nan`, the REPL's command names and output, `Environment`'s sources and their order, the containers
    row, D105 (how a chain fuses). D91's member templates stay under D317 with `map_` in the plural (228).
238. **Algorithm and format names in the library** (D324 settled `Http`/`Udp`): `Sha256`, `Argon2`, `Base64`,
     `Zlib`, `Gzip`, and the rest Claude proposed (`Deflate`, `SecureRandom`, `Reload` (D290), `Memory.Inspector`
     (D291)). Keep them, or spell them some other way? Blocks nothing yet; renames grow with their callers.
237. **Turning `List<Byte>` back into text** (not built; only `library/` reads a list's items). `to_string()` is
     taken by the list's display and D309 fixes it to `String`. Options: (a) `bytes.to_utf8_text(): String?`,
     `null` for invalid UTF-8; (b) a `String` constructor taking bytes, halting on invalid UTF-8; (c) `to_string()`
     on `List<Byte>` decodes. Recommend (a) (invalid bytes are a normal outcome).
226. **What a crash report shows** (D297, built as D298): the condition's operands, the parameters and locals in
     scope and the attributes of `this`, at most 12 values, text cut at 80 bytes, objects left out, and no condition
     text on the line (the `.crashes` map keeps it). Right set?
219. **Confirm D263-D267** and the names `remove_where`, `truncate`, `swap`.

## Taste (names, syntax, how it reads)

220. **A `Dictionary`'s `[]` is `get`/`set`**: under D315 nobody calls them by name, so rename them `get_at`/`set_at`
     so every `[]` is one function pair? Recommend yes (D205: no syntax).
221. **A walked row states its reads** with a `crash` line per read, or the walked read halts on its own?
105. **Naming a root class that a nearer one shadows**: a root qualifier (`Root.Plugin`), or fall through only for
     a self-reference? Nothing is built.
161. **Where an enum reopening's values go**: appended in merge order, so a program's own values come before a
     loaded engine's. Should the program's come last instead? (A reopening choosing its place is out: the language
     decides, and D302 already fixes each value's number.)
9. **The floor**: what stays C, and how `--final-classes` shows it
   ([standard_library.md](docs/standard_library.md#pure-spite-dissolving-the-runtime)).
39. **The C left in `main`** (`argv`, `_setmode`, releasing singletons, the `--debug-memory` report): moving it into
    Spite needs a way to receive `argv` and to run code after the program ends.
76. **Enums declaring more than `to_string()`/`to_debug()`**, the way a number's class does.
134. **How outputs are chosen**: `Boolean` fields of `Build` (`run` defaulting to `true`), "name any output and get
     only those", or one `--outputs=` list?
138. **Two ways to format**: a compile formats the program, `spite format <path>` formats anything. Keeping both is
     out (SPITE.md, one way to do a thing): which stays? Recommend the compile (the compiler is the formatter).
171. **Who may read and write an address**: any function of a standard-library class, including one a program adds
     by reopening it, or only files under `library/`?
183. **A capped `ShortText<32>`**, or is D203's 15-byte inline text enough?
217. **Should unsigned arithmetic that does not fit halt too**, with explicit wrapping functions for hashes? And
     `-fwrapv` for production builds?

## Performance, settle by measurement (D214)

214. **`--optimized` at `-O3` or `-O2`**: the measurements pick no clear winner (D205).
178. **A wait inside an expression runs before the rest of its statement.** Keep, or move the statement's earlier
     parts into temporaries so the written order holds (more frame fields per wait)?
210. **Is `Float` arithmetic rounded to `Float` after each operation?** 2-3x faster loops, but printed last bits
     change, so not D205.
