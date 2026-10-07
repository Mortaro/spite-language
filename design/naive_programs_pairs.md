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

S1 measured by hand on the stress C after B1 (the fourth pass of [naive_programs.md](naive_programs.md#fourth-pass-2026-10-07)):
`Runner<Move>` and `Runner<Regenerate>` specialised for the configuration `prepare()` leaves in their matchers'
`headers`, `kinds` and `keys` take the tick from 37.9 ms to 4.0 ms, against the hand engine's 7.0 ms on two
threads. For the runner the configuration is a function of the row class alone (`prepare()` reads only the
attribute classes), so the specialised copy is one per runner instance, and the loop over entities stays a run-time
loop.
| S2 | a class whose fields fall into groups read or written by different loops | access sets per field from every loop | one storage per group | one storage |
| S3 | the same list read field by field in one phase and whole in another | the phases and the conversion cost | each phase reads its own shape; a compiled conversion between them | one shape |
| S4 | objects of one class made where threads reach them and where they do not | per creation site flow | two classes after compilation, each with its own counts and pool | one class |
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
| B3: a list item read only to test it is not counted, the slot tested in place (stress 48.1 to 43.8 ms a tick, 43.3 to 40.3 as one C file, physics 8.4 to 8.1 ms) | [optimizations](../docs/optimizations.md#a-list-item-read-only-to-test-it-is-not-counted), [proofs](../docs/proofs.md#a-list-item-read-only-to-test-it-is-not-counted) |

## Threads

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| T1 | `for item in list { item.x = f(item) }` | each iteration writes only its own item and reads nothing another writes | bands on the pool, joined after the loop | serial loop | `Parallel` per system, hand bands | vectorised bands with no alias checks |
| T2 | `total = total + item.x` in a loop | the only shared write is a recognised reduction (sum, min, max, count, and, or) | per-band partials merged in band order | serial | hand partial sums | exact reassociation only where proven safe for integers |
| T3 | `out.add(g(item))` in a loop | appends go only to a list made outside and read only after the loop | per-band lists joined in written order | serial | per-thread command queues | |
| T4 | two systems called in a row | D505 per field, not per class, from stage 2's effect summary | overlapped | in order | hand stages | |
| T5 | a system's body with two halves touching disjoint fields | the halves share nothing written | two pieces overlapped like T4 | one piece | | |
| T6 | any of T1 to T3 | cost: body estimate times count above a threshold | the parallel form only above it, one branch on count if the count is unknown | serial | hand grain sizes | |
| T4b | `for runner in runners { runner.run() }` over a list never changed after it is built | the list's element classes and order are known while compiling | unrolled into a row of calls, then T4 applies | the loop | a stage's hand row of calls | |
| T8 | a loop sharing one scratch every pass overwrites before reading | each pass writes the scratch before it reads it | a private scratch per band, then T1 | serial | per-thread searchers | |
| T7 | a shared id counter `next_id = next_id + 1` | the counter is the only shared state of the writers | atomic, or a range handed to each band | lock | locked id counter | |

## Layout

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| L1 | `List<Particle>` read field by field in hot loops | no reference to an item escapes; the slot is its only name; loops read a subset of fields | structure of arrays, one array per field | array of objects | hand parallel arrays per field | |
| L2 | fields always read together | co-read sets from every loop over the list | those fields share one array (hybrid layout) | L1 or plain | | |
| L3 | a class with hot and rarely read fields | access counts by loop depth | hot part inline, cold part in a side array | one struct | | |
| L4 | a component read by disjoint systems | the systems' field sets are disjoint | the component split into columns per set | one column | hand column headers | |
| L5 | removing from a list whose order is never read | no loop or read depends on order | swap-remove | ordered remove | hand swap-remove | |
| L6 | a list cleared every tick then refilled | no item is read before it is written again | reset the count, keep the storage | free and remake | generation stamps instead of clearing | |
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
| M4 | an object made and dropped in one call to the OS | no escape past the foreign call | in the frame, aligned for the target | heap | per-call OS structures | |

## Counting references

| # | Naive code | Proof | Faster form | Falls back | Retires | Backend |
|---|---|---|---|---|---|---|
| B1 | `var row = list[index]` then `row` passed to calls, compared or kept (reading and writing its attributes, and passing it to calls, is built, above) | nothing writes the list or that slot before the local's last use | no reference counted for the read | counted read | hand borrowed reads | per generic instance: a write into `Column<$T>.values` is not this list when no instance's item is this list's item |
| B1t | `list[index].count()`, `f(list[index])`: an item read with `[]` and used at once | the call it is used by cannot write the list | the slot's pointer passed, no count | counted read | | |
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
| C4 | a call to a small function | its body fits a size bound | inlined in the generated C, across the parallel C units too (the split build lost about 5 ms of stress) | |

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
