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
D374 (2026-10-01). Earlier answers are listed in each row of `design/decisions.md`.

## Open

250. **A heap the C library finds corrupted on Linux and macOS** ends in `SIGABRT` with glibc's own line (`free():
     invalid pointer`), and no `spite.fault` line or Spite frames; Windows reports `heap-corruption` with frames
     ([failure.md](docs/failure.md#what-a-native-fault-reports)). Options: (a) the fault handler also takes
     `SIGABRT`, reported as `abort` (any `abort()`, not only the heap's); (b) as `heap-corruption` when the C
     library's message says so; (c) leave it to the C library. Recommend (a): the frames are the useful part.
     Blocks nothing; `native_fault_heap` pins today's Linux output.
251. **How `call_with` spreads a walk, now that `map(function)` is gone** (D317 item 98, nomap's row). Options: (a)
     `function.call_with_each(made)`, calling the named function `made(argument: Spite.Argument): argument.class`
     once per argument and passing the results, specialised while compiling with no list between; (b) collect into
     a `List<Anything>` with `each`, then `call_with(list)`, which travels as a shape and costs a dispatch; (c) a
     read-only attribute on `Spite.Argument` collected with `map_<members>()`, which only works when the value
     depends on the argument alone. Recommendation: (a), the only one that is free at run time.
252. **What "storage owns" covers** (D354). Is an attribute typed `T` an owner like a list slot, so only `T?`
     attributes and other places are weak, or does only collection storage own, making every attribute that points
     at a listed item weak? And an object placed in two lists, or a tree whose children are held by attributes?
     Recommendation: list, dictionary and vector slots and non-nullable attributes own; a `T?` attribute or local
     holding an object something else owns is weak; putting one object into a second owner is a compile error
     naming `copy()`. It keeps trees working and matches "99% of code already narrows".
253. **How a binding's Spite enum names each value's C number** (D351). Options: (a) a number written beside each
     value in the enum (new syntax, allowed only where a binding uses it); (b) the enum declares `func
     foreign_number(): Integer` with a `switch` (possible once D371 lets enums declare functions) and the compiler
     inverts it; (c) the binding writes a function from `Integer` to the enum by hand, which D351 wanted the
     compiler to do. Recommendation: (b), no new syntax and readable as Spite.
254. **What a fractional member of a generic vector is declared to answer** (D355). `Vector3<Integer>.length()`
     answers `Float`, `Vector3<Double>.length()` a `Double`: there is no spelling yet for "my class if fractional,
     else `Float`". Options: (a) every number class gets a get-only class constant naming its fractional class and
     the vector writes `func length(): $number_type.fraction_class`; (b) always `Float`, losing `Double` precision
     against D355; (c) a codegen `if` in the return type. Recommendation: (a), one rule the compiler folds.
255. **Does D374 cover the standard library, superseding D284?** D374 makes any class shadowing a visible class an
     error, which already refuses a program's `Game.Math.Vector3` beside the library's `Vector3`, more strictly
     than D284's attribute matching with zero false positives. Recommendation: yes, one rule; record a row
     superseding D284.
256. **Does `Console.flush()` stay public?** D346 says the moron never chooses flushing, and its third guarantee
     covers prompts. Recommendation: remove `flush()` from the public surface; keep `write` for text without a line
     end.
257. **How `--final-classes` marks the supplying file and root** (D366). A comment may only be a markdown link
     (D34). Recommendation: one comment line per declaration that is a link to the file it came from, which needs
     no new form; decidable under D205 if Mortaro agrees.
258. **Whether a serializer is compiler-written code or library Spite specialised per class** (D319 says "generated
     while compiling"; D240 says nothing hidden). Recommendation: library Spite over
     `attributes.each(write_attribute)` with the writer holding its output as an attribute, specialised per class,
     so the generated code is visible in `--final-classes` and the walk function's missing output (status) is
     solved by the instance.
259. **Is D229's `function_runs_in_pieces` retired?** D335 and D362 give the runner `function.accesses`, from which
     "runs in pieces" follows. Recommendation: retire it with a row, the runner deciding from accesses.
260. **Where the optimisation report goes** (D36, D332). Recommendation: always written beside the build in
     `.spite/build/<program>/`, one line per refusal with its source line, no flag to ask for it.
261. **What `--format` does beside `--check`** (D369 item 138, D348, D190 "every compile formats first"). Does
     `--check` reformat files (it compiles), and is there a no-write check for linters, as `spite format --check`
     was? Recommendation: `--check` formats like every compile; `--format` alone rewrites and stops; no separate
     no-write mode (`check.sh` compares a copy, as it already does for `diagnostics/`).
262. **What "measured for each program" means for `--optimized`** (D356). There is no workload to time in an
     ordinary build. Recommendation: `-O2` everywhere now; measure `-O3` only where a program carries a benchmark,
     until a decided way to declare one exists.
263. **How TLS is built** (D294, D350, D361). Options: (a) TLS 1.3 written in Spite (X25519, an AEAD, certificate
     verification against the system's root store), the long road D350 and D361 point to; (b) the system's own TLS
     through `DynamicLibrary` on Windows and macOS, which leaves Linux without one that is not a third-party
     library. Recommendation: (a), client first, checked against public test vectors, with an outside security
     review before it is called done.
264. **Plain IO that is concurrent on its own** (D99, D176, D336; [concurrency.md](docs/concurrency.md#what-the-compiler-does-at-a-wait)).
     D99 says waiting on IO is hidden async/await that the moron never chooses, but what is built only parks inside a
     `Concurrent`: straight-line code outside one (the entry constructor and everything it calls) waits where it is,
     letting other `Concurrent`s run meanwhile, and only two reads side by side overlap on their own. So a server written
     as plain code (`next_request`, handle, `respond`, again) serves one request at a time, and `HttpServer` says so
     ([standard_library.md](docs/standard_library.md#http)): a slow client holds up every other. The docs and WHY.md now
     say this plainly. Options: (a) **the library serves each client in a `Concurrent` of its own**: `HttpServer` and a
     `Socket` listener take the named function that answers one request or one connection and start it as a
     `Concurrent` per client, so the plain server is concurrent with nothing written; it costs the scheduler only in
     programs that serve. (b) **the compiler overlaps loop passes that wait**: extend "reads in a row overlap" to a
     `while` whose passes it can prove independent (an accept loop whose body reaches no state another pass writes),
     starting each pass as a `Concurrent`; nothing new to learn, but the proof is narrow and order of output changes.
     (c) **keep it explicit**: plain code waits in order, concurrency is always the caller's `Concurrent`, and D99 is read
     as "a wait never stops the other `Concurrent`s". Recommendation: (a) for servers and listeners, with (c) as the
     rule everywhere else, since it keeps one way to start concurrent work and needs no proof that can silently stop
     applying. Blocks: rewriting concurrency.md to lead with plain IO, and any claim that a plain server is concurrent.

265. **A change of signedness that does not fit** (D359, D162; [values_and_types.md](docs/values_and_types.md#arithmetic-that-does-not-fit-halts)).
     Arithmetic and narrowing now halt, but `var bits: UnsignedInteger = count` with a negative `Integer` `count`, or any
     change of signedness at the same width or wider, still keeps the bits, since D162 leaves
     signedness out of "wider" and the hashes read their words this way (`var word: UnsignedLong = block.read_long(at)`).
     Options: (a) **keep it**: a same-width signedness change is a reading of the bits, as today. (b) **check it too**,
     so `-1` into an `UnsignedInteger` halts, and add a named function for reading the bits, as `bits()` reads a
     `Float`'s (say `Long.bits_as_unsigned()` and `UnsignedLong.bits_as_signed()`), which the hashes would call.
     Recommendation: (b), since a negative count turning into four billion is the silent wrap D359 forbids, and the
     bit readings are few and all in `library/`. Blocks: nothing; today's reading is (a).
