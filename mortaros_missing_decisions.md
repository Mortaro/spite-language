# Decisions waiting on Mortaro

Only questions Mortaro still has to answer. An agent adds one here with a link to where the docs argue it; when he
answers (inline or in `mortaros_notes.md`), the answer goes into the page that teaches it and a row of
`design/decisions.md`, and the item is deleted. Item numbers never change. Each item: the question, the options,
the recommendation, what it blocks. "D244" marks something that can go wrong silently today; "D205" one Claude
could decide itself. The principles that answer most questions before they are asked: the moron (anyone who uses
Spite) never chooses, the language does; performance choices belong to the compiler; whatever can be written in
Spite is written in Spite (D350); reflection exposes everything and tree-shakes what is not read (D353); one way to do each
thing; prevent mistakes rather than offer options; storage owns its items and other references are `T?` (D354).

Every other item is answered: the 14 principle answers by D369, and every confirmation of what agents decided by
D370, 76 by D371, 235 by D372, 161 by D373 and 105 by
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02). Earlier answers are listed in each row of `design/decisions.md`.

## Open

265. **A change of signedness that does not fit** (D359, D162; [values_and_types.md](docs/values_and_types.md#arithmetic-that-does-not-fit-halts)).
     Arithmetic and narrowing now halt, but `var bits: UnsignedInteger = count` with a negative `Integer` `count`, or any
     change of signedness at the same width or wider, still keeps the bits, since D162 leaves
     signedness out of "wider" and the hashes read their words this way (`var word: UnsignedLong = block.read_long(at)`).
     Options: (a) **keep it**: a same-width signedness change is a reading of the bits, as today. (b) **check it too**,
     so `-1` into an `UnsignedInteger` halts, and add a named function for reading the bits, as `bits()` reads a
     `Float`'s (say `Long.bits_as_unsigned()` and `UnsignedLong.bits_as_signed()`), which the hashes would call.
     Recommendation: (b), since a negative count turning into four billion is the silent wrap D359 forbids, and the
     bit readings are few and all in `library/`. Blocks: nothing; today's reading is (a).
266. **`crash flag` on a `Boolean?`** (D372). D372 makes `assert flag` mean "there and `true`" and names only
     `assert`, so `crash flag` on a `Boolean?` is still an error. Options: (a) `crash flag` halts unless the flag
     is there and `true`, as `assert` reads it; (b) keep it an error and write `crash flag == true`. Recommend (a):
     one reading of a `Boolean?` condition everywhere.
267. **`UdpSocket.port` of a closed socket** (D369 item 250). Options: (a) halts naming the closed socket, as
     built; (b) answers `Integer?`, `null` once closed. Recommend (a): asking a closed socket is a developer
     mistake (D199), and `0` would be a silent wrong value.
268. **`clamp` and not-a-number** (D369 item 249). `minimum` and `maximum` now pass `nan` on; `clamp` was built the
     same way, answering `nan` when the value or either bound is `nan` (it used to answer the low bound). Recommend
     confirming: the same rule as `minimum` and `maximum`.
269. **The name `wrapping_subtract`** (D359). D359 names `wrapping_sum` and `wrapping_multiply`; the subtraction
     was built as `wrapping_subtract`, after the `subtract` operator function. Recommend confirming it.
270. **Which functions of `Spite.Class` a class may override** (status "Functions of `Spite.Class`"). `Spite.Class`
     declares `to_string`, `to_debug`, `get_name` and the other reflection getters, so every class's own
     `to_string()` would become a class-object override that must fold. Options: (a) only members a reopening of
     `Spite.Class` adds are hooks; (b) everything except the per-object members and the reflection getters, which
     become reserved names; (c) a spelling that marks a hook. Recommend (b). Blocks backlog R8 (overriding).
271. **Member templates by class on a union list beyond `filter_`**: D330 names only `filter_<classes>()`;
     `count_`, `any_`, `all_` and `remove_where_` by member class are not built. Recommend allowing them the same
     way (one rule for every member template). Blocks nothing.
272. **Is a numbered enum value written with its quotes?** D386's example reads `admin = 99`, while every enum
     value is written single quoted (`'admin'`). Built as `'admin' = 99`, the existing form of a value with `=`
     after it. Options: (a) `'admin' = 99`, as built; (b) `admin = 99` unquoted, only when numbered. Recommend (a):
     one way to write a value.
273. **Where a comment link resolves from.** A comment link resolves from the entry file's folder, while `load`
     resolves from the file's own folder, so a program that loads a package from another depth fails on the
     package's comment links. Options: (a) resolve a link from the file's own folder, as `load` does; (b) from the
     entry folder, as today. Recommend (a).
274. **How library Spite reads a numbered enum value's number** (D386). Binary files still write a value by its
     position, since there is no decided spelling for its number. Options: (a) a get-only `number` on every enum
     value; (b) `to_integer()`; (c) through reflection (`Spite.Enum`, D371). Recommend (a), which a binding and a
     serializer both read.
275. **Loading a subfolder of a package** (D364 says never `load "kal/physics"`). It is not enforced: when only the
     subfolder is loaded the compiler cannot tell that `kal/` is a package root. Options: (a) leave it unenforced;
     (b) refuse a load whose folder lies inside another loaded root, once both loads are seen. Recommend (a) now
     and (b) when both loads are visible.
276. **Which library classes the shadowing rule counts** (D387). It compares the last name, so a program class
     named `Function`, `Debug` or `Arena` is refused because of `Spite.Function`, `Spite.Debug` and
     `Memory.Arena`. Options: (a) every library class counts, as built; (b) only classes outside `Spite.` and
     `Memory.`. Recommend (a).
277. **Does a nested `enum`, `union` or `type` count as a class for shadowing?** A program class `Entry` is legal
     beside `Directory`'s union `Entry` today. Recommend yes, legal: a nested declaration is only visible through
     its class.
278. **How the test runner calls a test once declarations cannot be called** (D391, D393). `tests/` calls
     `call_function()` on a class's `.functions`, which run on a stand-in instance; a declaration cannot be called.
     Options: (a) the runner makes an instance and calls `Spite.Call(declaration, instance)`, running the test
     class's constructor; (b) keep a stand-in instance the runner can reach. Recommend (a), the one way D393 allows.
     Blocks backlog R8's function half and R9.
279. **Which calls an enum parameter is specialised for** (reflection proposal rule 5). Options: (a) only values
     that come from reflection (`Phase.values`); (b) every enum literal too, which needs `==` on enum values to fold
     and specialises many existing functions. Recommend (a).
280. **How a walk passes values beyond the element.** `each` hands only the element; a walk that needs more (a
     value, a writer) keeps it in attributes of the walking object today. Options: (a) that is the way (one
     argument, state in the walker, as D384's serializers do); (b) `each` with extra arguments. Recommend (a).
     Found building backlog J1 (D244): (a) is fine where the walker is made per call (`JsonWriter`, `JsonReader`),
     but `BinaryFormat<T>` and `Spite.DebugInstance<T>` are singletons every thread shares, so state in them
     corrupts memory when two `Parallel`s write or show a value at once (tried), and a walker made per value would
     cost an allocation per nested object and per enum value that the binary format does not make today. With (a),
     the binary walk needs a walker made per call, or a reentrant per-call lock from the compiler; with (b), it stays
     a stateless singleton at no cost. `BinaryFormat` keeps its plural walk until this is answered; now recommend
     (b), the only form that keeps the binary format at zero allocations without shared state. Blocks backlog J1's
     binary half and R7.
281. **When a waiting loop counts as a hang** (backlog B6 and B15; failure.md's open list). A `while true` that can
     never leave, and a `Concurrent` polled for `finished` under `resume_only_when_asked()` without `run_ready()`,
     hang without a word. Options: (a) a compile error wherever no exit and no wait is reachable in the loop, as
     D336 does inside a `Parallel`; (b) a run-time halt once a polled `Concurrent` cannot make progress (nothing
     else runnable); (c) both. Recommend (c): the error where provable, the halt as backstop.
282. **A write to a copy that dies unread** (backlog B12, proposed by Claude): `values[row].copy()` answered, the
     caller sets an attribute on it, and the copy dies. Options: (a) a compile error when an object only this
     function holds is written and never read again; (b) leave it. Recommend (a).
283. **How a serializer is given D320's rename map.** D320 writes `JsonWriter<Monster>({Monster.attributes['health']:
     "hp"})`, but a writer takes its value in its constructor (`JsonWriter(order)`, D208), and Spite has neither
     overloading (D59) nor default arguments. Options: (a) a second constructor argument on all four,
     `JsonWriter(order, keys)` and `JsonReader<Order>(text, keys)`, with `{}` when there is none (every call site
     changes); (b) an attribute set before the first `write()` or `read()`, `writer.keys = {...}`, the table filled
     on first use; (c) a class made once from the map that makes the writers and readers. Recommend (a): D320 fills
     the table "when the serializer is constructed", and one way to call it. Blocks backlog J2
     ([json.md](docs/json.md#a-key-that-is-not-an-attributes-name)).
284. **A dropped `ForeignCallback`'s ticket table is C** (D361; backlog F3, built). A context callback now hands C
     a ticket (a table slot and a reuse count), so a call through a dropped context halts naming the function. The
     table is C the generator writes, carried only by programs that make context callbacks. Options: (a) keep it C
     until C9 gives the backend primitives a home; (b) write it now as a library singleton the emitted C calls.
     Recommend (a): it is a few lines beside the callback C that already exists, and C9 moves both together.
285. **How long a poll of an unstepped `Concurrent` may go before it halts** (D244; backlog B15, built). A
     `Concurrent` read for `finished` a million times in a row, answering `false`, with nothing stepping it between
     (`resume_only_when_asked()` without `run_ready()`), now halts at `Scheduler.polled_unfinished`. It is a
     count, not a proof: nothing else could ever finish it, so the count only decides how soon the halt comes.
     Options: (a) a million, as built; (b) also a compile error where the compiler sees a loop poll without
     stepping. Recommend (a) now, (b) later.
286. **Names for three new library members** (backlog J3, S11, S12; each "proposed by Claude, unconfirmed"): an
     untyped JSON value read by `JsonReader` and walked with `switch` (J3), renaming or moving a file or folder on
     `File` and `Directory` (S11), and `List.index_of(item): Integer?` with `find_index_by_<member>` (S12).
     Recommend: `JsonValue` (a union of `JsonObject`, `List<JsonValue>`, `String`, `Double`, `Boolean`), `move_to(path)`
     on both `File` and `Directory` (one name for rename and move), and `index_of`/`find_index_by_<member>` as
     proposed. Blocks J3, S11 and S12, which are not built until named.
287. **How `--final-classes` names the file that supplied a declaration** (D382; backlog C7, not built). D382
     marks each declaration with one comment line linking the `.spite` file that supplied it, but a comment may
     only link a `.md` page with an anchor (`# path.md#anchor`), so the printed program would no longer compile,
     and `check.sh` compiles it. Options: (a) a comment may also link an existing `.spite` file, with no anchor;
     (b) allow that only inside a `--final-classes` folder; (c) name the source some other way than a comment.
     Recommend (a): one comment form, a link that resolves. Blocks C7.
288. **What a frame is for frame arenas** (D352, D369 item 173; backlog E1). The docs never say what "a frame" is or
     how `Memory.Frame` is spelled. Options: (a) a loop pass the compiler finds (a frame loop ending in a wait such as
     `program.sleep`), reset by the compiler; (b) a singleton `Memory.Frame()` whose frame the program ends
     explicitly; (c) a function's call frame, which placement already covers. Recommend (a), with an object that
     would outlive the frame a compile error naming `copy()`. Blocks backlog E1.
289. **Which geometry classes become generic over `Number`** (D355, D381). `Vector2`, `Vector3` and `Vector4` are.
     Making `Quaternion`, the matrices, `Plane`, `Ray`, `AxisAlignedBox`, `Frustum` and `CubicBezier` generic too
     makes every user write `Quaternion<Float>()`. Options: (a) all of them; (b) `Quaternion` and the matrices only,
     for `Double` precision; (c) none beyond the vectors. Recommend (b).
290. **A whole-number vector's fractional answers** (D381, D244). `Vector3(3, 4, 12).normalized()` answers
     `(0, 0, 0)`: the components stay `Integer`. Options: (a) the fractional members answer the vector of the
     number's fractional class (`Vector3<Float>` for `Integer`); (b) a compile error on `normalized()` of a
     whole-number vector, naming the conversion; (c) keep it. Recommend (a); (c) is a silent wrong value.
291. **Which values a failed assert keeps for the crash report** (backlog X1). Text and objects may be freed before a
     crash prints them. Options: (a) numbers, Booleans and enum values kept as they are, text as its length only;
     (b) keep a reference to each, a count on every failed assert; (c) no values, as the docs say today. Recommend
     (a).
292. **The call chain of an `--optimized` crash on Linux and macOS** (backlog X1). Options: (a) build with
     `-fno-omit-frame-pointer`, about 1% slower (D398); (b) the C library's unwinder, against D361; (c) an unwinder
     written in Spite that reads the unwind tables, which is large. Recommend (c) as the direction, with (a) until it
     exists.
293. **Cancelling a `Concurrent`** (backlog K6; status concurrency.md). The page names no way to stop running work,
     and dropping a handle waits for it. Options: (a) `handle.cancel()`: the work stops at its next wait, its locals
     are let go, a call already on a helper thread finishes there with its answer dropped, `finished` answers
     `true`, and reading the value afterwards halts naming the cancel; (b) no cancel in the language: the work
     reads a flag it is handed and returns; (c) dropping the last handle cancels instead of waiting (reverses
     "leaving a scope is a join point"). Recommend (a). Blocks backlog K6.
294. **A `Concurrent` made off the scheduler's thread** (backlog K6). Inside a `Parallel` or a helper there is no
     event loop, and today the work runs to its end on the spot, its waits blocking that thread. Options: (a) keep
     that, and say so in the rules; (b) hand it to the scheduler's thread, the handle joining across threads (a
     `Parallel` that waits for it while the scheduler's thread waits for that `Parallel` would hang); (c) a compile
     error where the compiler sees `Concurrent(...)` reached from `Parallel` work. Recommend (a), with (c) added
     where it is visible. Blocks backlog K6.
QNEW1. **What a program writes to add a word to `String.Inflection`** (backlog R1; metaprogramming.md's plural
     error and standard_library.md point at "reopening `String.Inflection`"). D328 says nothing evaluates a library
     function of the program while compiling, and the compiler inflects with its own copy of `string.spite`, so the
     compiler can only honour a program's words if they are data it reads, like a `Build` field's default. Nothing
     settles that data's form. Options: (a) `String.Inflection` is a singleton in `library/string/inflection.spite`
     with `var irregulars = {"person": "people", ...}` and `var uncountables = ["data", ...]`, literal defaults the
     compiler reads; a reopening replaces a `var` whole (as a reopened enum restates its list), so a program adding
     `cactus` restates the library's pairs too, and `pluralize()` reads the same dictionary at run time (one
     allocation the first time a program inflects); (b) the same singleton, but every `Dictionary<String>` attribute
     a reopening adds is read as more irregulars (`var game_words = {"cactus": "cacti"}`), so nothing is restated;
     (c) drop the reopening: the plural error says only "rename the member", and the table stays the library's
     (a word is added by changing `string.spite`, upstream). Recommend (a): it is how a reopening already works, and
     whole replacement keeps one place that lists every word. Blocks backlog R1.
