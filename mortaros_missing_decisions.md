# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353).

Answered on 2026-09-30 and 2026-10-01 (rows in `design/decisions.md`): 5, 21, 22, 25, 34, 36 (D337), 46, 64, 71,
79 (D322's principle), 89 (D326), 98, 99, 100, 108, 153 (D342), 155 (D336), 166 (D338, D347), 175 (D353), 187,
188, 197 (D346), 202 and 204 (D339), 206, 208 (D332), 209 (D354), 212 (D348), 215 (D349), 216 (D327), 218, 222
(D351), 223, 224 (D330), 225 (D334), 227 (D320, D329), 229 (D318), 230 (D319), 231 (D321), 232 (D322), 234 (D340), 214 (D356), 210 (D357), 183
(D358), 217 (D359), 39 and 9 (D361), 109 (D362, D363), 247 (D364), 240 (D365).
Answered, row pending: 228 (`map_` over a predicate is an error), 213 (the default build compiles fastest), 205's
spelling (`x`, `y`, `z`, `w`). Last pass, 2026-10-01: 9 and 39 by D350 (moved to "Confirm quickly"); 6's `${` by
D244; 134 narrowed by D348; 183, 210, 214 and 217 moved to "Confirm quickly" as compiler choices; 109 rewritten;
205 narrowed; 243-247 and 249 added.

## Blocking work now

245. **D345's run-time `Number` operators** (proposed, unconfirmed): with both sides read at run time, the right
     side is converted to the left side's class and must fit exactly, or the program halts; and an operator works
     through any `type` that requires its function (`sum` for `+`). Options: that; convert both to the wider class;
     refuse operators on `Number` outside a copy. Recommend that. Blocks: code over `Number` in hot builds.
246. **Nullable type parameters under D321** (`value: Number?`, "not yet" in D343): a copy per class plus the null
     case, the tag (D344) carrying "no value". Options: that; refuse `T?` of a `type`. Recommend that. Blocks:
     finishing D321.
173. **An object made in the frame arena** (the reset is the compiler's, D352): may it be stored where it outlives
     the frame? Recommend a compile error naming `copy()`, a debug generation check as backstop. Blocks: D154's
     frame lists.

## Silent today (D244)

236. **Text read as an enum by name** answers the first value when none matches. Options: (a)
     `"calm".to_mood(): Mood?`, as `to_integer()` answers `Integer?`; (b) `Mood.values['calm']`; (c) halting on no
     match. Recommend (a). Blocks: reading enums from files.
235. **Testing a `Boolean?` for presence**: today only `== true` or a `switch`. Options: (a) `flag != null` on a
     `Boolean?` only; (b) `switch` only; (c) `assert flag`. Recommend (a).
249. **`minimum`/`maximum` and not-a-number** (C's rule, kept as built by D339): `nan.minimum(0.0)` is `0`, the
     not-a-number ignored. Options: keep; propagate (`nan` in, `nan` out); halt while developing. Recommend
     propagate (an ignored operand is a silent wrong value).
90. **A loaded folder that is itself a program** merges into the loader, silently reopening its classes. Recommend:
    a loaded folder with an entry file keeps its root classes to itself.
93. **A foreign call through a header's prototype converts silently** (a `String` where C wants an `int`; a `void`
    read as `0`). Recommend: check every argument and the result against the prototype, an error naming the C type.
179. **Waits that run the event loop in place** (`and`/`or`, function values, join on drop) can hang each other.
     Each gap is closed (D244); left for Mortaro only a gap whose sole fix costs speed.
211. **`ForeignCallback`**: a call through a dropped context is not caught, and a `'no_context'` callback must be a
     singleton's function. Recommend: a dropped context halts at the call; the names stay.
6. **Open questions 10 and 11** ([open_questions.md](design/open_questions.md#open-questions)): how
   `--final-classes` shows which root supplied a declaration; whether a `type`'s functions are written as
   function-valued attributes. (9, `${` printing a stray `$`, is a D244 error.)

## Confirm what agents decided

243. **D337's `Number`** (proposed): its members (`sum`, `subtract`, `multiply`, `divide`, `less_than`,
     `greater_than`, `to_long`, `to_double`; no `remainder`), and a `type`'s own name inside a required signature
     meaning "the class that fits" (`sum(Number): Number` met by `Integer.sum(Integer): Integer`). Recommend both.
244. **D342's readings** (proposed): a flag given twice is an error; a setting's value is checked against its type
     while compiling; a bare `Boolean` setting; arguments that are not flags go to the program. Recommend all.
241. **D334's error**: a commit one package mixes with another, while a second package pins it alone, is an error
     naming both lines. Recommend keep (a commit's files are read once).
205. **Where `x`, `y`, `z`, `w` apply** (the spelling is answered, row pending): vectors and quaternions only, or every
     class whose attributes are coordinates? Recommend everywhere.
242. **The parts of D316-D354 proposed by Claude**: D317's inflection and question names, D327's eviction, D328's
     pluralize rules, D331's storage steps, D333's reload details, D335's name `accesses`, D339's class-level
     constants, D340's normalisation, D341's "nothing was rebuilt", D343's copy naming, D344's tagged layout, D346's
     flushing strategies, D353's `memory.allocator`. Each: yes, or reopen.
239. **The rows decided by Claude on 2026-09-30**: D292, D300-D313. Each: yes, or reopen.
10. **The rows proposed by Claude on 2026-09-23/24**: the tree shaker, `nan` printing as `nan`, the REPL's commands
    and output, `Environment`'s sources and order, the containers row, D105 (how a chain fuses).
238. **Algorithm and format names** (D324 settled `Http`/`Udp`): `Sha256`, `Argon2`, `Base64`, `Zlib`, `Gzip`,
     `Deflate`, `SecureRandom`, `Reload` (D290), `Memory.Inspector` (D291). Recommend keep.
237. **`List<Byte>` back to text**: (a) `bytes.to_utf8_text(): String?`; (b) a `String` constructor halting on bad
     UTF-8; (c) `to_string()` decodes. Recommend (a). Blocks: reading text from sockets and files as bytes.
226. **What a crash report shows** (D298): operands, parameters and locals, the attributes of `this`, at most 12
     values, text cut at 80 bytes, objects left out, no condition text. Recommend keep.
219. **D263-D267** and the names `remove_where`, `truncate`, `swap`. Recommend keep.

## The rest

138. **Two ways to format** (only one may stay): the compile formats the program, `spite format <path>` formats
     anything. Recommend the compile alone.
134. **The outputs D348 leaves** (`--c-source`, paths): with C never reaching the moron (D350), keep `--c-source`
     only for the compiler's own debugging, or remove it? Recommend remove it from the moron's flags.
220. **A `Dictionary`'s `[]` functions** are `get`/`set`: rename to `get_at`/`set_at` like every other `[]`?
     Recommend yes (D205).
221. **A walked row's reads**: a `crash` line per read, or the walked read halts on its own? Recommend the read
     halts on its own (one way, nothing to remember).
105. **A root class a nearer one shadows**: a root qualifier (`Root.Plugin`), or fall through only for a
     self-reference?
161. **An enum reopening's values** are appended in merge order, so a program's come before a loaded engine's.
     Should the program's come last? (D302 fixes each value's number either way.)
76. **Enums declaring more than `to_string()`/`to_debug()`**, as a number's class does?
171. **Who may read and write an address**: any standard-library class's function, including one a program adds by
     reopening it, or only files under `library/`? Recommend only `library/`.
178. **A wait inside an expression runs before the rest of its statement.** Keep, or keep the written order with
     temporaries (more frame fields per wait)? Recommend the written order (no surprise).
233. **Checking a binding's numbers against a C header** (D351 needs no header): (a) a declaration parser over the
     C compiler's `-E` output; (b) generated probe C printing the values; (c) no check. Recommend (b).
