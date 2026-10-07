# Optimisation and proof pairs for naive programs

The work list for [naive_programs.md](naive_programs.md). Each row is one thing a naive program writes, the fact
the compiler must prove about it, and the faster form that fact allows. A pair moves to `docs/optimizations.md` and
`docs/proofs.md` when it is built, and its row here then says **built** and links there. Every row not marked
built is proposed by Claude, unconfirmed.

Columns: **Naive code** is what a person writes. **Proof** is what the compiler must establish. **Faster form** is
what it generates. **Falls back** is what happens when the proof fails (always the plain, correct form). **Retires**
is the hand code in the engine package it makes unnecessary. **Backend** is what an own backend could add that C cannot
say.

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
| B1 | `var row = list[index]` then reads of `row` | nothing writes the list or that slot before the local's last use | no reference counted for the read | counted read | hand borrowed reads | |
| B2 | `var row = list[i]`, change it, `list[i] = row` | nothing else touches the slot between the copy and the write-back | the slot itself is changed, no copy | copy and write back | the runner's `Stream` | |
| C5 | any class in a program with threads | no task can reach an object of the class | plain counts, not atomic | atomic counts | | |

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
| C6 | every program | a release is a count-down with a rare free | the release emitted `static inline` in the header with the free out of line, in every C unit; the same for small list accessors | a call | | own inlining by cost |
| C4 | a call to a small function | its body fits a size bound | inlined in the generated C, across the parallel C units too (the split build lost about 5 ms of stress) | |

## Open threads in this catalogue

- Which of these the stage 1 baseline actually needs is unknown until it is measured; rows will be reordered then.
- Rows that change observable order (T3's merge, M3's timing of `drop`) keep written order or are refused; any that
  cannot will go to Mortaro.
