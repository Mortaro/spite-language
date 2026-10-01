# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353); one way to do each thing; prevent
mistakes rather than offer options; storage owns its items and other references are `T?` (D354).

Answered on 2026-09-30 and 2026-10-01 (rows in `design/decisions.md`): 5, 9 and 39 (D361), 21, 22, 25, 34, 36
(D337), 46, 64, 71, 79 (D322's principle), 89 (D326), 90 (D364: no entry file is special, a loaded root reopens
like any other, and D366 shows the winner), 98, 99, 100, 108, 109 (D362, D363), 153 (D342), 155 (D336), 166
(D338, D347), 175 (D353), 183 (D358), 187, 188, 197 (D346), 202 and 204 (D339), 206, 208 (D332), 209 (D354), 210
(D357), 212 (D348), 214 (D356), 215 (D349), 216 (D327), 217 (D359, D360), 218, 222 (D351), 223, 224 (D330), 225
(D334), 227 (D320, D329), 229 (D318), 230 (D319), 231 (D321), 232 (D322), 234 (D340), 240 (D365), 245 (D367), 247 (D364); 6 (D366,
D368). Answered, row pending: 228 (`map_` over a predicate is an error), 213 (the default build
compiles fastest), 205's spelling (`x`, `y`, `z`, `w`). Pass of 2026-10-01 (after D366): 90 removed; 93 merged
into 233; 134, 138, 171, 173, 178, 179, 211, 220, 221, 233, 236, 246 and 249 moved to "Confirm quickly"; 242
extended.

## Confirm quickly

Each has the one answer the principles imply. Yes, or reopen.

246. **Nullable type parameters under D321** (`value: Number?`): a copy per class plus the null case, the tag
     (D344) carrying "no value"; refusing `T?` is out (D354 makes `T?` the everyday reference). Blocks: finishing
     D321.
173. **An object made in the frame arena outliving the frame**: a compile error naming `copy()`, a debug generation
     check as backstop (prevent mistakes). Blocks: D154's frame lists.
236. **Text read as an enum by name**: `"calm".to_mood(): Mood?`, as `to_integer()` answers `Integer?` (one way,
     D293); today it silently answers the first value. Blocks: reading enums from files.
249. **`minimum`/`maximum` and not-a-number**: propagate (`nan` in, `nan` out); C's rule of ignoring it, kept by
     D339, is a silent wrong value (D244).
179. **Waits that run the event loop in place** can hang each other: close every gap, even one whose fix costs
     speed; a hang is never acceptable (D244).
211. **`ForeignCallback`**: a call through a dropped context halts at the call (a kept reference is a `T?`, D354);
     a `'no_context'` callback stays a singleton's function.
233. **Checking a binding against a C header** (merges 93): when a header is given, the binding's argument and
     result types and its enum numbers are checked against it, an error naming the C type; how it reads the
     header is the compiler's choice. Today a call through a header converts silently (D244).
138. **Two ways to format**: the compile formats the program; `spite format <path>` goes (one way).
134. **`--c-source`**: leaves the moron's flags, kept only as the compiler's own debugging output (D350, D361).
220. **A `Dictionary`'s `[]` functions**: renamed `get_at`/`set_at`, like every other `[]` (one way).
221. **A walked row's reads**: the read halts on its own; no `crash` line per read (nothing to remember).
171. **Who may read and write an address**: only files under `library/` (prevent mistakes).
178. **A wait inside an expression**: the written order holds, with temporaries where needed (no surprise); the
     compiler drops them where it proves nothing changes.

## Open

235. **Testing a `Boolean?` for presence**: today only `== true` or a `switch`. Options: (a) `flag != null` on a
     `Boolean?` only; (b) `switch` only; (c) `assert flag`. Recommend (a). Blocks nothing.
105. **A root class a nearer one shadows**: a root qualifier (`Root.Plugin`), or fall through only for a
     self-reference? Recommend fall through for a self-reference. Blocks nothing yet.
161. **An enum reopening's values** are appended in merge order, so a program's come before a loaded package's.
     Should the program's come last? Recommend last (the program has the final say). Blocks nothing.
76. **Enums declaring more than `to_string()`/`to_debug()`**, as a number's class does? Recommend yes. Blocks
    nothing.

## Confirm what agents decided

243. **D337's `Number`** (proposed): its members (`sum`, `subtract`, `multiply`, `divide`, `less_than`,
     `greater_than`, `to_long`, `to_double`; no `remainder`), and a `type`'s own name inside a required signature
     meaning "the class that fits". Recommend both.
244. **D342's readings** (proposed): a flag given twice is an error; a setting's value is checked against its type
     while compiling; a bare `Boolean` setting; arguments that are not flags go to the program. Recommend all.
241. **D334's error**: a commit one package mixes with another, while a second package pins it alone, is an error
     naming both lines. Recommend keep.
205. **Where `x`, `y`, `z`, `w` apply** (the spelling is answered, row pending): vectors and quaternions only, or every
     class whose attributes are coordinates? Recommend everywhere. Blocks: D355.
242. **The parts of D316-D366 proposed by Claude**: D317's inflection and question names, D327's eviction, D328's
     pluralize rules, D331's storage steps, D333's reload details, D335's name `accesses`, D339's class-level
     constants, D340's normalisation, D341's "nothing was rebuilt", D343's copy naming, D344's tagged layout, D346's
     flushing strategies, D353's `memory.allocator`, D355's fractional members answering `Float`, D359's
     `wrapping_sum`/`wrapping_multiply`. Each: yes, or reopen.
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
