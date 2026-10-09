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
D374 (2026-10-01); 264 by D378 and 250 by D379, 252 by D380, 254 by D381, 256 by D383, 257 by D382, 258 by D384, 261 by D385, 253 by D386, 255 by D387, 259 by D388, 260 by D389, 262 by D390, 251 by D391, 263 by D394 (2026-10-02); 266 to 296 by D468 to D488, and 268, 269, 271 and 276 to 279 by D493 to D497, 298 by D498, 301 by D499, 291 by D500, 299 by D501 (2026-10-03); 275 by D503, 302 by D504, 304 by D505 (2026-10-04); 280, 281, 284, 297, 300 and 303 by D510, decided by Claude under D509 and listed under To confirm (2026-10-07). Earlier answers are listed in each row of `design/decisions.md`.

## To confirm

Decided by an agent under D509 (anything that can be changed later). Each is built or documented as decided; say
"confirmed" or give the other answer, and the agent changes it.

- **D562, a write-back of what the slot already holds is not written, across calls**: a call made as a statement
  is left out when the compiler proves, by following values through the calls from every known caller, that it only
  stores into slots what they already hold, checks what was already checked and leaves numbers it can set in its
  place; a branch that may break the proof keeps the call on its own path. Why: the naive engine's runner copies
  each component into a row and stores it back after the system, and that store was 3 to 4 ms of a 20 ms tick.
- **D552, a list held only by another list lives in its slot**: an inner list nothing names but through its slot is
  stored inside the outer list's block, decided per element type for the whole program and refused on any use
  that could keep or alias one. Why: the naive engine's physics grid reads a bucket per query, and the pointer to
  each bucket object was one dependent load too many.
- **D553, a list of lists filled again keeps each list's room**: clearing such a list keeps each slot's block for
  the next empty list put there, so a grid rebuilt every tick allocates nothing after the first. Why: the cost is
  only memory kept past the count, which a list filled the same way every pass uses again.
- **D510, item 280**: a walk's extra values are extra arguments to `each_<member>(...)`. Why: keeps the binary
  format's walker a stateless singleton, no allocation and no state shared between threads.
- **D510, item 281**: a loop that can never leave, and whose calls can neither end the program nor wait, is a
  compile error. No time-based halt, since it would misfire on long computations.
- **D510, item 284**: the callback ticket table stays C until the backend primitives get a home.
- **D510, item 297**: strings, numbers and generic classes stay refused as union members; wrap them in a class.
- **D510, item 300**: `Fraction<$number_type>` names a number's fractional class (`Double` stays `Double`). Why:
  making everything `Float` would silently lose precision for `Double` programs.
- **D510, item 303**: `move_to` never replaces an existing file; delete it first if you mean to.
- **D510, D505's limits**: calls in a row overlap only as bare calls without arguments that reach a loop, with
  objects told apart by class. Widening them is planned work, not a question.
- **D510, D508's run-time branch**: an optimisation may leave one branch between two forms compiled in advance,
  on a fact only the run knows (a list's length). Never a scheduler.
- **D511, the spec wins**: when a docs page and `specs/` disagree, the spec is right and the page is the bug.
- **Which section of values_and_types.md was outdated**: the link had no anchor, so the agent fixing it took the
  "Inline types and duck typing" section (a `type`'s functions taught as dispatched function values, replaced by
  "always mono it") and checked the rest of the page against the compiler. Say if you meant another section.
- **D512, the naive engine's language gaps**: assets that finish in a later frame are the compiler's job (until
  then asset loading keeps `Parallel`); bytes and foreign structs get plain library forms; thread-pinned classes are
  a fact the compiler respects; the engine keeps stages, and commands flush between them.
- **Engine, the waiting-system write rule**: with plain `List<T>` columns, the engine's rule refusing a waiting
  (IO) system that writes `Vector`-fitting components of its rows has lost its reason. Kept for now (it refuses,
  never silently drops); it goes when the compiler arranges waiting itself (D512).
- **D554, a wait in a frame does not hold the frame**: inside a frame loop (a `while` that sleeps on a `Program`,
  itself or through its own class's functions), `receiver.function()` with no arguments, answering nothing, on
  another object of the program, is started as a `Concurrent` when the function can wait. A started call nobody
  waits for still finishes before the program ends; a crash in it halts at once with its own report; at most eight
  per statement are in flight, and the ninth waits for the oldest (holding the frame, never dropping a call). The
  engine's waiting-system rule stays after all: started work writes after the frame has moved on, so a row it
  wrote would be stored stale. Why: the naive engine's `io_systems` advanced 3 frames while its lookups waited
  (main 31); now 17, with the engine unchanged. Say if eight should be another number, or if the frame's own
  calls should be started too (they are its pace, so starting them would make the loop spin). A call where either
  side would write back a value read before a wait is not started (it waits in place and is listed in
  `--optimization-report`); say if you would rather have it be a compile error.
- **D513, plain counts per class**: in a program with threads, a class no other thread can count is counted with
  plain arithmetic; one used by threaded work stays atomic for all of its objects. Why: atomic counts were the
  largest single cost of the naive engine's stress test (105 ms a tick to 73), and the fallback is always atomic.
- **D514, inline releases**: every class's release is a `static inline` count-down with its freeing out of line,
  in every build but the reloadable ones. Why: a release in another C unit was not inlined (73 ms a tick to 66).
- **D515, an item written back to its own slot**: `var item = list[index]`, changed, then `list[index] = item` in
  the same block writes nothing back when nothing between can change the slot, and the read takes no count when
  the same holds to the end of the block. Why: the write-back puts back the object already there; a plain loop
  changing each item of a list runs 2.5 times faster. The engine's runner does not get it (its read and store are
  in different functions), so its numbers do not move.
- **D516, a name read from a list and assigned is an error when nothing reads it after**: the assignment can only
  be a write meant for the list, so it is refused naming `list[index] = value`, or the function of the class
  holding the list that writes it. Why: it compiled and stored nothing (D244); the general dead store ("any
  assignment nothing reads") is a wider rule left for you, since it would refuse code that is merely redundant.
- **D517, a pool per class for objects a list holds**: in a production build, an object of a class the program
  keeps in a list (and no other thread makes or counts) comes from that class's own pool, so a list's objects sit
  side by side. Why: the naive engine's components were interleaved in the C library's heap and every read was a
  cache miss (a stress tick 64 ms to 48, despawning 113 ms to 21, peak memory 96 MB to 67). The cost to confirm: a
  class keeps the memory it once used for its own next objects, never for another class or the system, while the
  program runs. Pooling every class was measured and not kept (one benchmark 4% slower); pooling short-lived
  objects no list holds would make `vector_maths` 1.75 times faster and is listed as not built.
- **D518, a list item read only to test it**: `crash list[index]`, `assert`, `if` and `if not` of an item test the
  slot in place and count nothing. Why: the count up and down was two writes to the item's object for a test that
  only reads the slot (a stress tick 48.1 ms to 43.8). Nothing runs between the read and the test, so it holds in
  every build.
- **D519, an item a name holds from its list**: `var item = list[index]` takes no count while nothing to the end
  of the block can write that list, and `item` may be passed to calls, which take it as held. A call that assigns
  attributes (letting go of other objects) no longer stops it; a store into a list the call is handed, a local
  list or a generic class's list of its own item does. Why: the slot holds the item the whole time, so its count
  is two writes for nothing (a stress tick 43.6 ms to 41.2). The same rule now decides D515's write-back.
- **D521, where the cases live**: `benchmarks/cases/<optimisation>/` (moved up to `benchmarks/<optimisation>/`
  by D524, below), each with `naive.spite`, `naive.c`, `expert.c`, the extracted generated C, and a README; the
  docs page links to each case.
- **D523, a test against a value a list never holds**: a list of numbers or of an enum that only its own class
  fills, and that nothing else can reach, holds only the values its writes can put in, and a `==` or `!=` against
  any other value is decided while compiling (its operand still read). Why: it needs no proof that a "setup" phase
  ends, since the fact holds at every moment, and it is the first step of S1, partial evaluation per configuration
  (a stress tick 39.0 ms to 35.4 as one C file).
- **D524, the layout after the Spite-only benchmarks go**: the cases move up to `benchmarks/<case>/` (no `cases/`
  level), and the `versus_c` programs become cases with an expert C each.
- **D525, the file names**: `generated.c` is the whole generated C, `highlights.c` the excerpt that shows the
  optimisation.
- **D528, values in the caller's frame**: a function value the callee only calls is a struct in the caller's frame,
  and a variadic list the callee only reads is framed for a `var`, an assignment or a `return` as for a statement.
  Why: the plain loop `apply(scorer.score, index)` made two objects per call and took 66.8 times naive C's time
  (0.65 now), and `var biggest = largest(a, b, c)` 31.6 times (1.01 now).
- **D529, no lock for a singleton no other thread reaches**: after the program is written out, the walk that
  decides plain counts decides locks again, and a singleton no code on another thread names keeps no lock and no
  atomic attribute. Why: the walk made while compiling counts every function made into a value as one a thread
  might run, so a `filter(matches)` locked a whole engine's matchers and columns while only a recipe loader ran on
  the pool (a stress tick 43.3 ms to 36.9 as one C file). The count of locks held is kept, so nothing a program
  does changes.
- **D530, a singleton's lock backs off**: a thread that finds a singleton's lock taken pauses, doubling up to 1 024
  pauses, then yields to the system between looks. Why: under four threads the plain compare-and-swap loop took
  113 ms against 21.5 for C behind a critical section; backing off takes 7.7, faster than a system lock at every
  thread count measured (1, 2, 4, 8).
- **D532, object keys**: an object key is keyed by identity; a class with its own `equals` used as a key is a
  compile error until keying by `equals` is built; `Boolean` keys are allowed.
- **D531, an attribute a call cannot assign is passed without counting**: `total_of(amounts)` or
  `discount_of(customer.terms)` passes the attribute held, as a local already was, when nothing the call reaches
  assigns any attribute along the path. Why: each such argument was a count up and a count down on an object the
  attribute kept alive anyway (a stress tick 36.0 ms to 30.5; the case 28.4 ms to 5.4, as fast as its naive C).
- **D534, the profile**: a text file beside the entry file, committed with the program so builds are
  reproducible; `--optimization-report` names every choice it made; the profiling flag's name is provisional.
- **D536, plain bytes and plain foreign structs (items 305 to 315), confirm after the speed goal**: Mortaro chose
  `List<Byte>` for bytes and asked Claude to pick the rest until the compiler work reaches the goal, then ask. Each
  was taken as the proposal recommends ([proposal](design/proposals/plain_bytes_and_foreign_structs.md#questions-for-mortaro)):
  305 a fresh list per read; 306 `read_integer_big_endian` twins; 307 both `BinaryReader` and number reads;
  308 `ForeignBytes(address, count)`; 309 `type` inline, `type?` a pointer; 310 a one-attribute `type` for values C
  writes back; 311 `List<T>(32)` and `String(256)` only in a `type` that crosses into C; 312 a held-list class when a
  binding needs it; 313 Booleans and enums 32 bits in a struct; 314 address forms leave programs now; 315 a
  `<list>_count` attribute is written by the compiler.
- **D537, two small calls, confirm after the speed goal**: (a) a walk that hashes or prints a class's description,
  such as `BinaryWriter<T>.schema()`, counts as reading those attributes, since they are the wire format; (b) of the
  two ways to ask whether a class's objects can be stored inline, `is_fixed_size` stays (it says what the class is,
  not which container holds it, D520) and `fits_vector()` goes.
- **D538, an item used at once is not counted**: `plays[at].count()`, `tracks[first].seconds` and
  `length_of(tracks[first])` read the item in its slot with no count, the call only where it cannot change the
  list. Why: the list holds the item through the use, so the count was two writes for nothing (a stress tick 27.3
  ms to 25.9).
- **D539, a loop over a list of different classes runs them at once**: `systems.each_update()` over a list of a
  `type` runs its elements on the pool when the table of classes decided while compiling allows every two of them,
  with the classes read when the loop starts (a branch between two compiled forms), instead of the unrolling pair
  T4b proposed. Why: the naive engine builds its stages at run time from strings, so their order is not known while
  compiling; the table needs no order (the case's three voices 257 ms to 91).
- **D540, the bytes half of D536 as built, confirm after the speed goal**: what Claude chose beyond the proposal.
  (a) `File.read_bytes()` answers `null` when fewer bytes arrive than the size said; `read_bytes_at` reads up to
  `count` and halts on a negative position or count. (b) A `Socket` keeps one 64 KB receive block and copies what
  arrived into a fresh list per read (the compiler does not reuse a let-go list's block yet). (c) The library keeps
  `read_bytes_into`, `read_bytes_now_into`, `write_bytes_from`, `write_bytes_now_from` for HTTP and WebSocket, refused
  in a program like the old forms; `BinaryReader.read_memory` is removed, not kept. (d) Beyond the proposal's list:
  `tiny` and `unsigned_long` reads, and `_big_endian` twins of the appends and writes too. (e) `ForeignBytes(address,
  count)` reads as well as writes, has `get_at`/`set_at`, `read_bytes`, `write_bytes`, `count(): Long`, and takes the
  address as a `Long`. (f) New proofs: `base + k <= list.count()`; non-negative positions; `rows * stride <=
  list.count()` for `row * stride + column`; a counted loop stepping by up to its window plus one. (g) A proven read
  outside a counted loop keeps one compare, because a local alias of a list is not tracked (a stale read would be a
  silent wrong value); only counted loops get the bare load.
- **D541, how `Dictionary<Key, Value>` keys behave** (built for D532): a fraction key is found by value, so
  `0.0` and `-0.0` are one key and every `not_a_number` is one key; a `Boolean` and an enum value are keys by value;
  an untyped literal is keyed `String` or `Integer` by its first key; a key is cast to the key type as any argument
  is, so a number given to a `Dictionary<String, V>` becomes its text. Why: each answers the same as the value's
  `==` would, but for `not_a_number`, where a key that can never be found again would be a silent loss (D244).
- **D541, what cannot be a key**: a class that declares `equals` (until keying by `equals` is built), a
  nullable, a union, a shape, a function value, a `Symbol` and a value class are compile errors. Why: each would
  either miss an equal key or has no identity to key by; every one can be allowed later without breaking a program.
- **D541, writing keyed dictionaries out**: JSON writes an enum key by its name and refuses keys other than a
  `String`, a whole number or an enum; no format writes an object key; the binary schema text keeps its old
  spelling, so binary files written before still read. Why: a JSON key is text, and an object's identity does not
  survive a file.
- **D542, a loop whose passes write only their own item runs in bands**: `orbits.each_advance()` over a `List` or
  `Vector` of a class runs in bands on the pool when every pass writes only its own element and counts no
  reference, the pass weighs at least 256 and the count times the weight reaches a million; a `List` is also
  checked for an element held twice. Why: the passes are independent by proof, and the two numbers were measured
  on this machine so light passes (bound by memory) and short lists stay in order (the case 800 ms to 51).
- **D543, ranges drop overflow checks**: the compiler works out the range of every whole-number local (from
  literals, assignments, remainders, `clamp`, counts and the conditions in force, widened at each loop's entry) and
  leaves out a check its ranges prove can never fire, such as a `Long` total of `Integer` items over a list; every
  check that stays is listed in `--optimization-report` with the ranges it found. Why: D360 asks for each proven
  check to go, and the proof is made entirely while compiling. Attributes and list items keep their type's range
  for now (another function could change them).
- **D544, a store counts only when it changes the slot**: `crates[at] = crate` from a name the function holds
  compares the slot with the new object and counts only when they differ. Why: a component written back to the slot
  it came from paid four counts on one object (a stress tick 38.0 ms to 33.3 on a busy machine). It is a test at run
  time, since only the run knows whether the slot holds the object; proving it while compiling (pair B2b) is not
  built. The cost to confirm: an index outside the list is reported as the read `crates[at]` rather than by the
  list's `set_at`.
- **D545, `Benchmark(work)` instead of `clock.benchmark(work)`**: `var result = Benchmark(do_something)` runs
  `do_something` once between two clock readings; `result.answer` is what it answered and `result.duration` a
  `Duration`. Why a class: Spite has no generic functions, so only a class made from the work can carry the
  answer's type (`Benchmark<Long>`, inferred from the function). The other answers: generic functions (a language
  change, yours to decide), or a `clock.benchmark(work)` taking only work that answers nothing and returning just
  the `Duration`, with the work storing its results in attributes. The cost to confirm: the function value is made
  on the heap with its reflection (a constructor of a library class gets no framed value yet), outside the
  measured time; the call through it is one pointer. In a program that starts a `Parallel`, the work counts as
  code another thread may run (values are matched to a thread's calls by argument count), so five cases whose point
  is a lock or a plain count left out keep their two clock readings (design/status.md).
- **D546, the program ends after its `Concurrent`s**: when the entry function returns, every `Concurrent` still
  running runs to its end before singletons are destroyed. Why: a singleton holding a running handle let it go
  after the scheduler was destroyed, reading freed memory and halting with the wrong cause. The cost to confirm: a
  `Concurrent` that never ends (a server loop kept by a singleton) keeps the program from ending once the entry
  function returns, where before its drop at exit waited on a destroyed scheduler.
- **D551, a table filled once is read as constants**: a list an object's setup fills once (its constructor, or a
  function that starts `assert not prepared`) gets, for each function that reads it, a copy reading it as a constant
  list, one per combination of the values it can hold (at most four), and a test at the call that picks the copy
  when the object's list holds those values. Why: tables a program fills once (a checkout's steps, a matcher's kinds)
  fold into straight code. The cost to confirm: the C and the executable grow (the naive engine's by 20% and 13%,
  for about 7% of its tick, measured while the machine was in other use), since combinations the program never makes are copied too.
- **D555, counts per group, and the despawn cost**: in a loop over different classes that may run them at once, a
  class only one of the calls counts keeps plain counts and its pool, and one two of them count is atomic only
  while they run. The stress update stage went from about 28 to 15 ms, but the sixty ticks after despawning
  everything got about a tenth slower (caches on the other core, and a pool start per empty tick), which D214
  would refuse; kept because the tick is the number that matters. Say if it should wait for a cost model.
- **D556, objects made for their owner are told apart**: an attribute only ever given an object constructed
  where it is given holds an object no other such attribute holds, so two calls that each keep their own meter,
  timing record or scratch list may run at once; an item of one singleton's list and an item of another's are told
  apart by comparing the two lists when the loop starts. Why: it is what keeps the naive engine's two stress
  systems apart, with no annotation.

## Open

None.
