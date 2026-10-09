# Optimisation and proof pairs for naive programs

The work list for [naive_programs.md](naive_programs.md). Each row is one thing a naive program writes, the fact
the compiler must prove about it, and the faster form that fact allows. A pair moves to `docs/optimizations.md` and
`docs/proofs.md` when it is built, and its row here then says **built** and links there. Every row not marked
built is proposed by Claude, unconfirmed.

Columns: **Naive code** is what a person writes. **Proof** is what the compiler must establish. **Faster form** is
what it generates. **Falls back** is what happens when the proof fails (always the plain, correct form). **Retires**
is the hand code in the engine package it makes unnecessary. **Backend** is what an own backend could add that C cannot
say.

**Granularity (D520).** Every row below decides per use (a list, a loop, a creation site), not per class: a class
may become several classes after compilation, and one list may have different shapes in different places. Pairs
built so far per class (C5, M6) are a first step; their per-site forms are rows to add.

| # | Naive code | Proof | Faster form | Falls back |
|---|---|---|---|---|
| S1 | a generic loop driven by lists that never change after setup | the lists' contents are fixed once setup ends, known per configuration | the loop specialised per configuration into direct code (partial evaluation) | the generic loop |

S1 is built in steps ([naive_programs.md](naive_programs.md#s1-design-2026-10-07)). **Step 1 is built**: the values
a list only its class fills can hold are listed while compiling, and a test against any other value is decided
([optimizations](../docs/optimizations.md#a-test-against-a-value-a-list-never-holds-is-decided-while-compiling),
[proofs](../docs/proofs.md#a-list-only-its-class-fills-holds-only-what-it-fills)): stress 39.0 to 35.4 ms a tick as
one C file, 41.9 to 41.3 split. **Step 2 is built**: a list filled once by its object's setup, with its count and
order traced in the C, gets a copy of every function that reads it per combination of its values (at most four),
the copy reading it as a constant, and a test of the object's list at the call that picks the copy
([optimizations](../docs/optimizations.md#a-table-filled-once-is-read-as-constants),
[proofs](../docs/proofs.md#a-table-filled-once-holds-what-its-setup-put-in)): stress about 7% less a tick, its C 20%
larger (measured while the machine was in other use, [the ninth pass](naive_programs.md#ninth-pass-2026-10-08)). Step 3
(the runner made direct, with L8b and B2b) is not built.

The sixth pass ([naive_programs.md](naive_programs.md#sixth-pass-2026-10-07)) took the 4.0 ms hand edit apart: of
the 40.4 ms tick, L8b's fused loop is 1.4 ms, the repeated match 3.3, the write-back (B2b) 11.8, the row kept in
locals with the call inlined 13.2, the configuration folded 0.8, and the matcher reduced to one lookup 5.1; every
singleton lock on top of those was about 18%, and is built.

S1 measured by hand on the stress C after B1 (the fourth pass of [naive_programs.md](naive_programs.md#fourth-pass-2026-10-07)):
`Runner<Move>` and `Runner<Regenerate>` specialised for the configuration `prepare()` leaves in their matchers'
`headers`, `kinds` and `keys` take the tick from 37.9 ms to 4.0 ms, against the hand engine's 7.0 ms on two
threads. For the runner the configuration is a function of the row class alone (`prepare()` reads only the
attribute classes), so the specialised copy is one per runner instance, and the loop over entities stays a run-time
loop.
| S2 | a class whose fields fall into groups read or written by different loops | access sets per field from every loop | one storage per group | one storage |
| S3 | the same list read field by field in one phase and whole in another | the phases and the conversion cost | each phase reads its own shape; a compiled conversion between them | one shape |
| S4 | objects of one class made where threads reach them and where they do not | per creation site flow | two classes after compilation, each with its own counts and pool | one class |

S4 is **built per group** for T4b loops (D550, [optimizations](../docs/optimizations.md#counts-stay-plain-for-what-one-of-the-calls-run-at-once-counts),
[proofs](../docs/proofs.md#what-one-of-the-calls-run-at-once-counts)): a class only one of the calls that can run
together counts keeps plain counts and its pool; a class two of them count counts atomically only while they run
at once, with its pool per thread. Per creation site (R9) is not built.
| S5 | a procedural loop over uniform records with independent steps | records are uniform; steps share nothing written | a pipeline of passes over columns | the loop as written |
| S5 | a procedural loop over uniform records with independent steps | records are uniform; steps share nothing written | a pipeline of passes over columns | the loop as written |

## Already built (the base everything else stands on)

| Pair | Where it is documented |
|---|---|
| Calls in a row that share nothing written run at once (D505) | [optimizations](../docs/optimizations.md#calls-in-a-row-run-at-once), [proofs](../docs/proofs.md#calls-that-share-nothing-written) |
| Objects that never leave their function live in the frame | [optimizations](../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame) |
| Template chains run as one loop | [optimizations](../docs/optimizations.md#template-chains-run-as-one-loop) |
| A counted loop reads its items unchecked; proven reads test only bounds | [optimizations](../docs/optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked) |
| A function taking a `type` is compiled per class | [optimizations](../docs/optimizations.md#a-function-taking-a-type-is-compiled-per-class) |
| Singleton locks: skipped with no task in flight, shared by readers, taken once per counted loop, atomic counters | [optimizations](../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form) |
| Crash reports in cold functions | [optimizations](../docs/optimizations.md#a-crashs-report-is-kept-out-of-the-way) |
| C5: plain counts for every class no other thread can count | [optimizations](../docs/optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class), [proofs](../docs/proofs.md#no-other-thread-counts-a-class) |
| C6: a release is a `static inline` count-down in every unit, its freeing out of line | [optimizations](../docs/optimizations.md#a-release-is-inlined-in-every-unit) |
| B2: an item written back to its own slot is not written, and B1's uncounted read where the same proof holds to the end of the block (same block only; the engine's runner reads and stores in different functions) | [optimizations](../docs/optimizations.md#an-item-written-back-to-its-own-slot-is-not-written), [proofs](../docs/proofs.md#an-item-written-back-to-its-own-slot-is-the-slot) |
| M6: objects of a class a list holds come from that class's own pool, side by side (stress 64.0 to 47.6 ms a tick, despawning 113 to 21 ms, physics 9.5 to 8.5 ms) | [optimizations](../docs/optimizations.md#objects-of-one-class-sit-together), [proofs](../docs/proofs.md#objects-a-list-holds-made-on-one-thread) |
| B1 for named items: an item a name holds from its list is not counted while nothing can write the list, the name passed to calls as held (stress 43.6 to 41.2 ms a tick, 40.0 to 38.1 as one C file) | [optimizations](../docs/optimizations.md#an-item-a-name-holds-from-its-list-is-not-counted), [proofs](../docs/proofs.md#an-item-a-name-holds-from-its-list-is-not-counted) |
| S1 step 1: a test against a value a list only its class fills never holds is decided while compiling (stress 39.0 to 35.4 ms a tick as one C file, 41.9 to 41.3 split, physics 9.2 to 8.9 ms) | [optimizations](../docs/optimizations.md#a-test-against-a-value-a-list-never-holds-is-decided-while-compiling), [proofs](../docs/proofs.md#a-list-only-its-class-fills-holds-only-what-it-fills) |
| S1 step 2: a table filled once by its object's setup is read as constants in a copy of each function reading it, chosen by a test at the call (stress about 7% a tick, 20% more C; measured while the machine was in other use) | [optimizations](../docs/optimizations.md#a-table-filled-once-is-read-as-constants), [proofs](../docs/proofs.md#a-table-filled-once-holds-what-its-setup-put-in) |
| Singleton locks only where another thread reaches: the walk over the written-out C (C5's) decides locks and atomic attributes again, so a function made into a value no thread calls no longer locks its singleton (stress 43.3 to 36.9 ms a tick as one C file, 50.3 to 42.7 split, physics 13.4 to 12.6 ms) | [optimizations](../docs/optimizations.md#a-singleton-no-other-thread-reaches-takes-no-lock), [proofs](../docs/proofs.md#no-other-thread-touches-a-singleton) |
| B1 for attributes passed to calls: an attribute, or a path of attributes, that nothing the call can run assigns is passed held, a shape's attribute through its reading function, a held parameter held again for the calls it makes (stress 36.0 to 30.5 ms a tick as one C file, 42.3 to 36.7 split, physics 12.5 to 12.0 ms) | [optimizations](../docs/optimizations.md#an-attribute-a-call-cannot-assign-is-passed-without-counting), [proofs](../docs/proofs.md#an-attribute-a-call-cannot-assign-is-passed-uncounted) |
| B1t: an item used at once (passed to a call that cannot change its list, read for an attribute, or asked a list's reading function) is read in its slot with no count (stress 27.3 to 25.9 ms a tick as one C file) | [optimizations](../docs/optimizations.md#an-item-passed-to-a-call-that-cannot-change-its-list-is-not-counted), [proofs](../docs/proofs.md#an-item-used-at-once-is-not-counted) |
| A1: an overflow check a range proves unneeded is left out (the compiler's own C 1 513 to 1 263 checks; the benchmark cases' loops still add `Integer` items or attributes, whose checks stay) | [optimizations](../docs/optimizations.md#arithmetic-a-range-proves-is-not-checked), [proofs](../docs/proofs.md#a-range-proves-arithmetic-fits) |
| B2b's run-time form: `list[index] = name` from a held name is written in place and counted only when the slot held another object (stress 38.0 to 33.3 ms a tick as one C file on a busy machine) | [optimizations](../docs/optimizations.md#storing-an-object-into-a-list-counts-it-only-when-it-changes-the-slot) |
| S4 per group (D550): what only one of the calls run at once counts stays plain, with its pool; a meeting point counts atomically only while they run | [optimizations](../docs/optimizations.md#counts-stay-plain-for-what-one-of-the-calls-run-at-once-counts), [proofs](../docs/proofs.md#what-one-of-the-calls-run-at-once-counts) |
| B3: a list item read only to test it is not counted, the slot tested in place (stress 48.1 to 43.8 ms a tick, 43.3 to 40.3 as one C file, physics 8.4 to 8.1 ms) | [optimizations](../docs/optimizations.md#a-list-item-read-only-to-test-it-is-not-counted), [proofs](../docs/proofs.md#a-list-item-read-only-to-test-it-is-not-counted) |

## Threads

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| T1 | `for item in list { item.x = f(item) }` | each iteration writes only its own item and reads nothing another writes | bands on the pool, joined after the loop | serial loop | `Parallel` per system, hand bands | vectorised bands with no alias checks; **built** for the member templates (D542, below) |
| T2 | `total = total + item.x` in a loop | the only shared write is a recognised reduction (sum, min, max, count, and, or) | per-band partials merged in band order | serial | hand partial sums | exact reassociation only where proven safe for integers |
| T3 | `out.add(g(item))` in a loop | appends go only to a list made outside and read only after the loop | per-band lists joined in written order | serial | per-thread command queues | |
| T4 | two systems called in a row | D505 per field, not per class, from stage 2's effect summary | overlapped | in order | hand stages | |
| T5 | a system's body with two halves touching disjoint fields | the halves share nothing written | two pieces overlapped like T4 | one piece | | |
| T6 | any of T1 to T3 | cost: body estimate times count above a threshold | the parallel form only above it, one branch on count if the count is unknown | serial | hand grain sizes | **built** with T1 (D542) |
| T4b | `for runner in runners { runner.run() }` over a list never changed after it is built | the list's element classes and order are known while compiling | unrolled into a row of calls, then T4 applies | the loop | a stage's hand row of calls | **built** in the form below (D539) |
| T8 | a loop sharing one scratch every pass overwrites before reading | each pass writes the scratch before it reads it | a private scratch per band, then T1 | serial | per-thread searchers | |
| T7 | a shared id counter `next_id = next_id + 1` | the counter is the only shared state of the writers | atomic, or a range handed to each band | lock | locked id counter | |

T4b is **built** as a table instead of an unrolling (D539,
[optimizations](../docs/optimizations.md#a-loop-over-a-list-of-different-classes-runs-them-at-once),
[proofs](../docs/proofs.md#classes-in-a-list-that-share-nothing-written)). The naive engine's stages are filled at
run time by comparing the strings each runner reports (`place` and `conflicts` in `App`), so neither their count
nor their order is known while compiling, and an unrolling would need the setup evaluated while compiling. Instead
the compiler decides, for every two classes the list's `type` can reach, whether their calls are independent, and
the loop reads its elements' classes at run time against that table. The engine's stage loop is reached and stays
in order: the two stress systems both write a `Profile.Timing` (`timing.record` in `run_once`), the `stamps` of a
`ColumnIndex` (found through `Columns.headers` by name) and the items of `List<Integer>`s passed as parameters, which
the per-class facts count as one. Measured by hand on the stress C (one file, `clang -O3`), overlapping the two on a
raw thread took the update stage from about 30.5 to 17.5 ms with the counts left as they were, and the tick to
about 20 to 25 ms with every count atomic (the machine was loaded; medians of 9 swung by a third). So the next steps
for the engine are facts that tell objects of one class apart where each runner owns its own (an attribute made for
its object and never assigned again), and count disciplines per group of overlapped calls (S4).

T1 is **built** for the member template (D542, [optimizations](../docs/optimizations.md#a-loop-whose-passes-write-only-their-own-item-runs-in-bands),
[proofs](../docs/proofs.md#passes-that-write-only-their-own-item)): `list.each_function()` and
`list.each(own_function)` over a `List` or `Vector` of a class run in bands when every pass writes only its own
element (a self-indexed key in the overlap facts: what a function reaches through `self` of the element, and of
every function it calls on `self`, is told apart from any other object of the class) and counts no reference. T6 is
built with it: the pass is weighed while compiling and the smallest count at which bands pay is a constant in the
C, compared with the list's count when the loop runs; for a `List`, the loop also checks that every element is held
by the list alone (a list may hold one object twice). Not built: T2's reductions, T8's scratch per band, a loop
written as a `while`, and passes that count references (which would make their classes atomic for the whole
program until S4 exists). The naive engine has no such loop: its systems' row loop is a `while` over combinations
that writes the runner's own scratch (`chosen`, `position`, `skipping`) and stores through the matchers, so it
stays in order.

## Layout

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| L1 | `List<Particle>` read field by field in hot loops | no reference to an item escapes; the slot is its only name; loops read a subset of fields | structure of arrays, one array per field | array of objects | hand parallel arrays per field | |
| L2 | fields always read together | co-read sets from every loop over the list | those fields share one array (hybrid layout) | L1 or plain | | |
| L3 | a class with hot and rarely read fields | access counts by loop depth | hot part inline, cold part in a side array | one struct | | |
| L4 | a component read by disjoint systems | the systems' field sets are disjoint | the component split into columns per set | one column | hand column headers | |
| L5 | removing from a list whose order is never read | no loop or read depends on order | swap-remove | ordered remove | hand swap-remove | |
| L6 | a list cleared every tick then refilled | no item is read before it is written again | reset the count, keep the storage | free and remake | generation stamps instead of clearing | |
| L6b | a list of lists cleared and refilled with `List<T>()` every pass (a grid rebuilt per tick) | the inner lists live in their slots (M8b), so no one else holds them; a cleared slot is marked and its block kept, and the outer block grows only by the library's growth, zeroed past the old room | a slot cleared keeps its block; the next empty list put there takes it | free and remake | the hand grid's arrays cleared and refilled in place | **built** (D553, [optimizations](../docs/optimizations.md#a-list-of-lists-filled-again-keeps-each-lists-room)) |
| L8 | a list built by one loop and read once, in order, by the next | the list is read nowhere else | the two loops fused, the list never made | both loops | a matcher that filled rows in place | |
| L8b | a list an attribute holds, cleared and filled by one call (`candidates.append(row.candidates())`), then read once in order by the caller's next loop (`choose(combination)` in the runner) | the list is read nowhere else; the consumer loop's body writes nothing the producer loop reads (per key: stamps and rows of the entity in hand); a call the body repeats with the same arguments and nothing changed between (`matches(entity)` in `fill_into`) answers and writes the same | the producer's loop inside the consumer's, the list never made, the repeated call dropped | both loops | the hand engine's driver column walked once | by hand on the stress C after B1: 37.9 to 31.0 ms a tick; S1 covers it and pays more |
| L7 | a plain-value list in a loop | values fit a vector lane, no alias | SIMD over the arrays of L1 | scalar | | explicit vector code without C intrinsics |

## Memory

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| M1 | objects made in a frame loop's pass | never outlive the pass (the pass ends in a wait) | frame arena reset at the end of the pass | heap | hand scratch pools | |
| M2 | a list used only as a queue | append at one end, remove at the other, with a visible bound | ring buffer | growable list | hand rings | |
| M3 | many objects let go together | their `drop`s are independent | freed in slices between passes | freed at once | budgeted deferred freeing | |
| M5 | `return Shape(...)` | the caller keeps the result in its own frame or a slot it already owns | built straight into the caller's slot | heap copy | | |
| M8b | a `List<List<T>>` filled with `List<T>()` and read only through its slots (a grid's buckets, an index by key) | no inner list is named but through its slot (each put in is fresh and dead after, each read is tested, used at once or named once with no resize before its last use), the outer list never aliased, no other storage holds such lists, no compiler-written code reads them | each inner list's whole object inside the outer list's block: a read is the slot's address | a list of references | hand index arrays per bucket | **built** (D552, [optimizations](../docs/optimizations.md#a-list-held-only-by-another-list-lives-in-its-slot), [proofs](../docs/proofs.md#a-list-held-only-in-a-slot-of-another-list)); per element type, not per list yet |
| M7 | `var copy = original.deep_copy()` | neither the copy nor the original, nor anything reachable from either, is written, resized or let go differently while both live, and no identity question is asked of the copy | the copy shares the original; where only part of the graph is written, only the path to it is copied | a real deep copy | hand copy-on-write | |
| M8 | an object assigned once from a fresh construction to an attribute, read only through its owner | nothing else names it, it is never compared, copied or handed out | its fields live in the owner's storage (or the owner's columns) | a separate object | hand inlined structs | |
| X1 | `var line = "{a} {b}"` used only for `.length()` or dropped | the text never escapes the iteration | built in a frame buffer with direct copies and literal lengths, no `String` | a joined `String` | hand stack buffers | |
| X2 | `list.sort_by_<member>()` on whole-number keys | the keys' range is proven, the sort is the library's | a stable radix sort of (key, position) pairs | the library's merge sort | hand radix sorts | |
| X3 | `crash list[i]` then `list[i].field` | same index expression, nothing between writes the list or the slot | one read, bounds-checked once, uncounted | two reads | | |
| E1 | a part of the program that reads no input, clock or IO | every value it computes comes from literals and exact operations | its answer worked out while compiling and written as a constant | the computation as written | | the backend can evaluate it in its own IR |
| M4 | an object made and dropped in one call to the OS | no escape past the foreign call | in the frame, aligned for the target | heap | per-call OS structures | |

## Counting references

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| B1 | `var row = list[index]` then `row` passed to calls, compared or kept (reading and writing its attributes, and passing it to calls, is built, above) | nothing writes the list or that slot before the local's last use | no reference counted for the read | counted read | hand borrowed reads | per generic instance: a write into `Column<$T>.values` is not this list when no instance's item is this list's item |
| B1t | `list[index].count()`, `f(list[index])`: an item read with `[]` and used at once | the call it is used by cannot write the list | the slot's pointer passed, no count | counted read | | **built** (above) |
| B3 | `crash list[index]`, `assert list[index]` or `if list[index]` before reading the item | the read is used only for the test, and nothing runs between the read and the test | the slot's pointer tested, no retain and release (with B1 for the read that follows, when it holds) | the counted read | | **built** (above); a `Dictionary` entry and an attribute tested through an item (`crash rows[i].owner`) are not |
| B2b | a row filled from slots in one function, the system called, the row stored back in another (the engine's runner) | the stored value is the one read, across the calls, and the system never assigns the row's attributes | no write-back, no count | the store | | |
| B2c | `var row = list[i].copy()`, changed, `list[i] = row` | no other name holds the stored object (L1's proof) | the slot changed in place, no copy | copy and write back | | |

## Waiting

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| W1 | a waiting call whose answer is read later in the pass | nothing between the call and the first read depends on it | started early, collected at the first read | waits in place | `Concurrent` per IO wait | |
| W2 | several waiting calls in a row | they share nothing written (T4's proof plus IO ordering per resource) | all in flight at once | in order | "drain up to 8 a frame" | |

## Code generation (for the C now, the backend later)

| # | Naive code | Proof | Faster form | Backend |
|---|---|---|---|---|
| C1 | any function over lists | no two parameters alias | `restrict` on the C pointers | alias facts per field, not per pointer |
| C2 | a loop whose bound is proven | bounds proven, count known | unrolled or vectorised by the C compiler with hints | own unrolling by cost |
| C3 | a hot path with rare branches | a branch only reaches a crash or a cold call | the branch marked cold, its code moved out (built for crashes) | layout by profile from benchmarks |
| C4 | a call to a small function | its body fits a size bound | inlined in the generated C, across the parallel C units too (the split build lost about 5 ms of stress) | not built; placing single-caller functions in their caller's unit measured no gain, and inlining the runner's template calls at the Spite level is blocked by templates expanding inside the generator ([the ninth pass](naive_programs.md#ninth-pass-2026-10-08)) |
| A1 | `total = total + values[index]` (a `Long` total of `Integer` items), `seed * 48271 % 2147483647`, `index * 37` under `index < 100000`, `round % 3 + 1` | the range of every whole-number local at each point: literals, assignments, remainders, `clamp`, `minimum`, `maximum`, counts, conditions in force, ranges widened at loop entry, passes counted from a counter stepped once a pass, a total of one term a pass | the plain operator, no overflow check | **built** ([optimizations](../docs/optimizations.md#arithmetic-a-range-proves-is-not-checked), [proofs](../docs/proofs.md#a-range-proves-arithmetic-fits)); ranges of attributes, of list items and of a call's answer are not, and an `Integer` total of list items needs R6's speculate and replay |

## Open threads in this catalogue

- Which of these the stage 1 baseline actually needs is unknown until it is measured; rows will be reordered then.
- The third pass ([naive_programs.md](naive_programs.md#third-pass-2026-10-07)) measured B3 and L8 on hand-edited C
  after M6: B3 takes a stress tick from 43.1 to 39.7 ms, and dropping the first match (what L8 would remove) from
  43.1 to 37.8 ms. The engine's candidate list is an attribute of its runner read through `choose`, not a local
  list read in order by the next loop, so L8 as written does not reach it yet.
- B1t (an item used at once without a name) measured within noise by hand after B1 (37.9 to 37.8 ms): not worth
  building before S1.
- Rows that change observable order (T3's merge, M3's timing of `drop`) keep written order or are refused; any that
  cannot will go to Mortaro.
