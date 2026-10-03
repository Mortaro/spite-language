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
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02); 266 to 296 by D468 to D488 (2026-10-03). Earlier answers are listed in each row of `design/decisions.md`.

## Open

268. **`clamp` and not-a-number** (D369 item 249). `minimum` and `maximum` now pass `nan` on; `clamp` was built the
     same way, answering `nan` when the value or either bound is `nan` (it used to answer the low bound). Recommend
     confirming: the same rule as `minimum` and `maximum`.
269. **The name `wrapping_subtract`** (D359). D359 names `wrapping_sum` and `wrapping_multiply`; the subtraction
     was built as `wrapping_subtract`, after the `subtract` operator function. Recommend confirming it.
271. **Member templates by class on a union list beyond `filter_`**: D330 names only `filter_<classes>()`;
     `count_`, `any_`, `all_` and `remove_where_` by member class are not built. Recommend allowing them the same
     way (one rule for every member template). Blocks nothing.
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
284. **A dropped `ForeignCallback`'s ticket table is C** (D361; backlog F3, built). A context callback now hands C
     a ticket (a table slot and a reuse count), so a call through a dropped context halts naming the function. The
     table is C the generator writes, carried only by programs that make context callbacks. Options: (a) keep it C
     until C9 gives the backend primitives a home; (b) write it now as a library singleton the emitted C calls.
     Recommend (a): it is a few lines beside the callback C that already exists, and C9 moves both together.
291. **Which values a failed assert keeps for the crash report** (backlog X1). Text and objects may be freed before a
     crash prints them. Options: (a) numbers, Booleans and enum values kept as they are, text as its length only;
     (b) keep a reference to each, a count on every failed assert; (c) no values, as the docs say today. Recommend
     (a).
297. **Values and generic classes as union members** (D459, D244; values_and_types.md "Unions in full"). A union
     with `String`, a number, `Boolean` or `List<Byte>` among its members was accepted and then failed later with
     an error that did not name the cause. It is now refused where the union is declared, saying to wrap the value
     in a class of the program's own. Options: (a) **refuse them**, as built: a union is a set of classes a `switch`
     tests by the header's class, which a value does not have; (b) make them work: a value member boxed with a tag
     (as `String?` already is), and a generic member tested with its codegen values (`List<Byte>`, as
     `found == Storage<$component_type>` is). Recommend (a): one representation of a union, and wrapping costs one
     small class. Blocks nothing; (b) can be built later without breaking a program written for (a).
298. **Which receivers may call a function** (Mortaro's note: `(start + bytes * last).copy_to(...)` is unreadable;
     "maybe only named identifiers call functions"). Counted across the repository: about 561 calls have a receiver
     that is not a name. Most are chains (`list.filter_x().count()`, `text.trim().lowercase()`), which are a
     language feature: member templates fuse a chain into one loop, and naming every step would add a local per
     step and lose nothing but readability. The rest are computed receivers: an operator expression in parentheses
     (`(a + b).copy_to(...)`, `(-x).absolute()`). Options: (a) a compile error only on a computed receiver (an
     operator or a unary minus in parentheses), naming a local to introduce, and chains stay; (b) also refuse
     chains longer than two calls; (c) refuse every receiver that is not a name. Recommend (a): it removes the
     unreadable form and keeps fused chains. Built as (a) (D492); (b) or (c) would go further.
299. **Frame arenas: the first step of automatic memory** (D485; [proposal](design/proposals/automatic_memory.md)).
     The proposal makes "a frame" a pass of a loop that ends in a wait (`program.sleep`, a present, a `Concurrent`
     wait), and puts a value made in a pass and proven never to outlive it into an arena the compiler resets at the
     end of the pass; anything else stays on the heap, so nothing is freed early. Options: (a) build that first, as
     the proposal orders it; (b) start with structure of arrays for lists, the larger win for the engine; (c) another
     order. Recommend (a): it reuses the placement proof and deletes the engine's hand scratch pools. Blocks
     backlog E1 and the rest of the proposal.
300. **How a signature names a number's fractional class** (D486, answering item 290). D486 makes
     `Vector3(3, 4, 12).normalized()` a `Vector3<Float>`, but a generic class can only name `$number_type` itself
     in a signature, so `Vector3<$number_type>.normalized()` has no way to say "the fractional class of
     `$number_type`". Options: (a) a type the compiler works out, `Fraction<$number_type>` (`Float` for every whole
     number up to 32 bits, `Double` for `Long` and `UnsignedLong`, the class itself for `Float` and `Double`), so
     the library writes `func normalized(): Vector3<Fraction<$number_type>>`; (b) a codegen value with a default
     worked out from another, `generic $fraction_type = $number_type.fraction`. Recommend (a): one type, readable where it is used, and nothing a program
     writes changes. Blocks building D486.
