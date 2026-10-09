# Naive programs, fast language: the plan

Mortaro's direction (D506, extended by D507 on 2026-10-07): **we make the engine naive, we make the language fast.**
A program says what it means with lists, loops and plain classes; Spite decides how it runs. Nobody writing a game
or an engine chooses threads, layouts or memory: the compiler proves what is safe and picks the fastest form, and
every optimisation it makes is documented together with the proof that enables it.

This file is the plan, kept so Mortaro stays in the loop. It changes as stages land; the catalogue of
optimisation and proof pairs it works through is [naive_programs_pairs.md](naive_programs_pairs.md). Everything
below that is not quoted from a decision is proposed by Claude, unconfirmed.

## The goal in one paragraph

A naive engine package (no `Parallel`, no `Concurrent`, nothing from `Memory`, no hand allocators, no hand structure
of arrays) runs at least as fast as today's hand-optimised one, on every benchmark the engine has. Then faster.
Each gap between the naive and the hand form is closed by one optimisation and the proof that allows it, written
into `docs/optimizations.md` and `docs/proofs.md` in the same commit. The generated C gets good enough that the
future own backend (D397) inherits a specification that already beats what LLVM finds on its own, because the
compiler knows things about the program (no aliasing, no escape, independent iterations) that C cannot say.

## Where it stands (2026-10-07)

The cloud session that ran out of quota on 2026-10-04 pushed 8 commits to `origin/master`; all are now on local
`master`. What it did:

- **Recorded D505**: calls in a row that share nothing written run at once, decided by the compiler.
- **Recorded D506**: programs are written naively; later `Concurrent`, `Parallel`, `ThreadPool`, `Lock`,
  `Atomic`, `ThreadLocal` and `Memory` move into an `Optimizer` namespace ordinary programs do not touch.
- **Built D505** for `receiver.function()` rows, then (last commit, `669adde7`) for the class's own functions and
  for rows that mix light and heavy calls. The proof reads the generated C of every function a call reaches.
- **Moved every crash report into a cold function** of its own (14% fewer instructions per tick on a naive entity
  system).
- **Measured** the engine's stress example for D501: a tick was about 60 ms, three quarters of it locking, mostly
  one shared counter; making that counter atomic took a tick to about 20 ms. Memory is not this benchmark's
  bottleneck (about 96 allocations a tick), so frame arenas buy it nothing.

Loose ends it left:

- `design/status.md`'s D505 line still says own-function calls and mixed rows are not built; `669adde7` built
  them. Not verified by a `check.sh` run here yet.
- D506 has a decision row but no docs page: no page teaches "write it plainly, the compiler makes it fast", and
  D506 asked for "how to enter that mode" to be documented.
- D501 also asked for the engine to move to the new compiler with a better benchmark: the engine package's commit `4e13dbc` moved
  it to Spite `3a9d9c40`; the stress numbers above are the "better".

What the engine still decides by hand (the engine package's `main`, 58,213 lines of Spite):

| Uses | Files | Occurrences |
|---|---|---|
| `Parallel` | 16 | 34 |
| `Concurrent` | 3 | 6 |
| `Memory.` | 30 | 60 |
| `allocator` | 11 | 284 |
| `ThreadPool`, `Lock`, `ThreadLocal` | 4, 4, 3 | 11 |

The [automatic memory proposal](proposals/automatic_memory.md) lists the rest: hand column headers, parallel
arrays per field, deferred budgeted freeing, scratch pools, tombstones, per-thread command queues, band sizes.

Repositories: the language's `master` and the engine package's `main` are up to date with their remotes. The game has no
upstream to pull; per D507 it is only a benchmark now and is not changed for features.

## Priority (2026-10-09, D550)

**The naive engine replaces the engine package's main branch as soon as it is at least as fast on every benchmark.**
Until then, compiler work is chosen by what closes the engine's remaining gaps, measured on its stress and physics
benchmarks (the others already match or win): first the runner made direct (S1 step 2 and Spite-level inlining),
then the two stress systems on two cores (per-object facts and per-site counts), then physics (profile first).
Benchmark cases that do not move the engine wait unless they are cheap.

**The target is the task, not the method (D557).** The original engine runs one system per thread and leaves the
other cores idle; the naive engine must not aim to match that thread for thread. Each system's loop over its
entities is split into chunks that fit the processor's cache and run on every core, and systems that share nothing
run at once on top. The measure is the tick against the original's best, and the goal is to beat it by a wide
margin.

**Quiet measurement (2026-10-09, final, nothing else running).** Compiler `579d35c5`, `--optimized --build`, 5
rounds alternating original and naive, medians in microseconds (the spread across rounds stayed within 4%):

| Benchmark | Original (main) | Naive | Naive / original |
|---|---|---|---|
| stress tick | 5,953 | 24,180 | 4.06 |
| stress despawn, sixty ticks | 23,915 | 20,909 | 0.87 |
| physics step | 5,691 | 6,941 | 1.22 |
| physics MoveCharacters | 4,966 | 5,692 | 1.15 |
| physics SortColliders | 275 | 688 | 2.50 |
| physics ReadCharacters | 179 | 362 | 2.02 |

The original's stress tick runs its two systems on two threads (5.6 ms each, at once); the naive one runs them one
after the other (12.2 ms each). One thread each, the gap is about 2.2x per system.

**Also required before the switch (found 2026-10-09):** on the naive branch, `io_systems` finds its database lookups
but the game advances only 3 frames while they wait, against 31 on main: the naive runner drains a waiting system's
queue with a plain call, so frames stop while IO runs. The check stays as it is; the fix is the compiler arranging
the wait (W1, D512): a waiting call inside a system started and collected on a later frame, so the frame loop keeps
running. W1 is therefore on the path to replacing main, after the stress and physics work.

## How every step is judged

1. **The naive form is the reference.** A step starts by writing the naive version of a piece of the engine (on the engine
   package's `naive` branch) and measuring it against the
   hand version with `--optimized`.
2. **A gap names a pair.** Each place the naive form is slower is one missing optimisation and the proof it needs.
   It goes into the pair catalogue before it is built.
3. **Kept only if at least as fast** as the hand form it replaces (D501, D214), on the engine's benchmarks:
   `stress`, `physics_bench`, `navigation_bench`, `render_bench`, `replication_bench`, `animation_bench`,
   `props_bench`, and the five whole programs of `benchmarks/` in this repository.
4. **Zero runtime, tree-shakeable** (D147, D176): every pair is a choice of code made while compiling. Where only
   the run can know something (a list's length), the page says why and the cost is one branch, never a scheduler.
5. **Nothing silent** (D244): an optimisation that cannot be applied falls back to the plain, correct form and
   says so in `--optimization-report`; it never changes what a program prints or computes.
6. **Documented in the same commit** (D185, D276): `docs/optimizations.md` section with an example program,
   `docs/proofs.md` entry, `design/status.md` line removed.

## Stages

Each stage is a set of pairs from the catalogue. Stages 2 to 6 can overlap once stage 1 has given numbers; the
order below is by what the measurements so far say pays most.

### Stage 0: settle what the cloud session left (small)

- Run `bash check.sh` on `master` to confirm `669adde7` is green on Windows.
- Fix the stale D505 line in `design/status.md`.
- Write the docs for D506 to D508 (done: `docs/write_it_plainly.md` and the README's philosophy).
- Clear the inbox (done with this plan: D507 row recorded).

### Stage 1: the naive engine and its baseline (medium, the most important stage)

- A branch of the engine package, `naive`, where the classes that use `Parallel`, `Concurrent`, `Memory` and
  hand allocators are rewritten as plain lists, loops and classes. It replaces the main branch once it is as fast.
- Run every engine benchmark for both forms; record the table in this file. The table is the work queue: every
  row where naive loses is a pair to build.
- Expected (from the proposal and the stress measurements): locks, per-field layout and loop parallelism dominate;
  memory matters only in frame loops that allocate (render, UI).

### Stage 1 results (2026-10-07)

Built on the engine package's `naive` branch (13 commits; the full report is `design/naive_baseline.md` there).
Ryzen 9 5950X (16 cores), Windows 11, Spite `e7261193`, `--optimized`, medians of 5 runs. "Hand" is the main
branch plus two bug fixes it needed to build, now also on main.

| Benchmark | Hand | Hand on several cores? | Naive | Naive against hand |
|---|---|---|---|---|
| stress tick | 7.0 ms | yes: 2 systems on 2 threads (12.8 ms on one) | 27.3 ms | 3.9x slower |
| physics tick | 6.4 ms | no | 9.8 ms | 1.5x slower |
| navigation, one thread | 10,761 / 639 queries/s | no | 11,905 / 700 | same |
| navigation, 20,000 queries in batches | 2,533 queries/s on 32 threads | yes, and 4.2x slower than one thread | 11,882 queries/s | 4.7x faster |
| navigation, 10,000 bots at once | 3.8 s, worst tick 186 ms | yes | 0.89 s, worst tick 856 ms | faster overall, worse worst tick |
| animation | 17.2 ms | no | 17.5 ms | same |
| replication send | 804 us | no | 801 us | same |

Only two hand forms really used several cores: stress (two systems at once, 1.85x) and the navigation pool, which
was slower than one thread because each of its 32 searchers kept its own 4-million-cell scratch. That is the
case for the compiler choosing: the expert's parallel form lost.

The gaps, by time, are the work queue for stages 2 to 5 (pairs in [naive_programs_pairs.md](naive_programs_pairs.md)):

1. Systems in a stage are a loop over a list of runners, so D505 does not see a row of calls (stress, about
   6 ms): T4b, then T1 inside each system's 200,000-row stream.
2. Reading an object out of a list counts a reference, atomically because asset loading still starts threads
   (stress about 5.5 ms): B1 and C5.
3. The split build does not inline the new small calls across units (stress about 5 ms): C4 across units.
4. About 4 ms of stress not yet explained (`Regenerate` slowed far more than `Move`).
5. Despawning allocates per removal and trims copy the kept log: L1, L5, M3.
6. The physics grid makes a fresh list per bucket each tick: L6 for nested lists, or M1.
7. A returned object is built in the frame and then copied to the heap: M5.
8. Colliders as objects instead of parallel arrays: L1 across several lists of one class, then L2.
9. 10,000 navigation requests in one tick: T8, then T1 with a cost model that counts memory.

**Second pass (2026-10-07, engine `naive` at 81b0e20).** The runner's hand paths by row shape became one plain
loop over the matched rows, and each column a plain `List<T>`. Behaviour is the same; speed is not:

| | stage 1 | plain loop | plain lists | hand |
|---|---|---|---|---|
| stress tick (ms) | 25.3 | 99.4 | 112.1 | 7.0 |
| stress despawn, sixty ticks (ms) | 59.9 | 59.1 | 143.4 | 22.8 |
| physics tick (ms) | 10.7 | 15.7 | 16.1 | 6.4 |

Stress is now 16x the hand form. That is the honest size of what the compiler must do. Measured on hand-edited C:

- **C5** (plain counts where no task reaches the class) alone takes the one-file plain-lists build from 101.2 to
  65.6 ms, the largest single cost. Built: the compiler's own per-class proof takes the split build's tick from
  105 to 73 ms (`List<Integer>` stays atomic, since the asset loader's `Parallel` uses it too).
- **C6** (a release inlined in every C unit): the "unexplained 4 ms" was one function placed in another C unit,
  where link-time inlining refused the release. Regenerate and Move are the same C; one file gives both 11.5 ms.
  Built: 73 to 66 ms a tick on the split build.
- **B2** (a copy written back to its own slot is the slot): the plain loop copies each component out and back,
  800,000 allocations and frees a tick. Built (D515) for a read and its write-back in one block, with B1's
  uncounted read: a plain loop changing each item runs 2.5 times faster. It does not move stress: since the plain
  lists the runner holds the stored objects (a tick allocates about 47 objects, not 800,000), and its read and
  store are in different functions with the system between, which is pair B2b. Eliding just the store by hand in
  the C saves about 3 ms of the 59 ms one-file tick, so the rest of stress is the runner's own machinery.
- **L8** (a list built by one loop and read once in order by the next is one loop): each entity is matched twice
  and a 200,000-entry list is built every tick.
- **L1** for `List<T>` columns: despawn is 2.4x slower from freeing 800,000 component objects.

So the compiler order is now: C5, C6, B2, L8, then T4b and T1 (running at once only pays after the single-core
form is close to the hand one), then L1, B1, L6, M3.

Language gaps it found, and what was decided (under D509, D512):

- **Work that may finish in a later frame** (a synchronous asset load inside a system): the compiler arranges it
  (stage 6, W1); until then asset loading keeps its `Parallel` on the naive branch.
- **File and socket bytes come only through `Memory.Address`**, and **foreign structs holding pointers or
  arrays** have no plain declaration: both stay `Memory` until the library gives a plain form (a bytes value
  and a foreign struct declaration); queued as library work ([proposal](proposals/plain_bytes_and_foreign_structs.md)).
- **Thread affinity of operating system handles** is an engine convention: the compiler must learn it (a class
  pinned to its creating thread is already a fact D505 reads) before it runs systems at once.
- **Stages stay** in the engine: queued commands flush between stages as today, so when systems see each other's
  commands does not change.
- **Compiler bug** (on main too): the crash-report C names some locals' types wrongly; with the check.sh fixes.

### Third pass (2026-10-07)

Engine `naive` at 81b0e20, Spite at e229034a, Ryzen 9 5950X, Windows 11, `--optimized`, medians of 5 interleaved
runs. Before: stress 64.0 ms a tick (split build; 60.5 ms as one C file), physics 9.5 ms.

**How it was profiled.** No sampling profiler is installed, so the one-file C of `stress` was built with
`clang -O3 -g -gcodeview` beside a small sampler: a thread that suspends the program's thread every 0.1 ms or so
while the 20 ticks run, walks its stack with `RtlVirtualUnwind` and records the addresses; `llvm-symbolizer
--inlining` names them, inlined functions included. About 22 000 samples over three runs; the sampled program
ticks at the same speed as the plain one. Percentages are of the tick, inclusive (a function's callees included),
summed over both systems.

| What the time is in | % of tick | The Spite code | What the hand engine did instead |
|---|---|---|---|
| Reading each component, `Column.value_at` through `Slot.fetch` and `Row.fill_attribute` | 31 | `crash values[row]` then `return values[row]`: each of 800 000 component objects is a heap block of its own, and its count is the first touch of a cold cache line (the four component releases alone are 34% of the samples) | `Items<T>`: components inline and contiguous, borrowed in place by `Stream` |
| Writing each row back, `Row.store` through `Slot.store` and `Column.write_at`, with `Row.stamp_written` | 24 | `values[row] = value` per attribute, `changes.stamp_written` per header | `Stream` wrote the inline item in place |
| Matching each entity, `Row.matches` and `find_row` per header | 19, half of it in `candidates()` and half again in `fill()` | per header, counted reads of `headers[index]`, `kinds[index]`, `keys[index]` (`String` and `ColumnIndex` counts are 12% on their own) | the driver column walked once, each entity matched once |
| The candidate list, `Row.candidates` | 11 (its matching included) | a fresh `List<Integer>` of 200 000 per system per tick | none |
| Decoding the combination, `Runner.choose` | 6 | `candidates[index].count()` and `candidates[index][picked]` per entity, each a counted read of a `List<Integer>` | none: a one-row system has no combinations |
| Singleton guards | 2 | `spite_guard_enter` on every `Row<T>` call | none |
| The systems themselves | 0.8 | `update_each` | the same |

**The three largest costs, as pairs**, each measured on hand-edited C of the one-file build:

1. **M6 (new): objects of a class a list holds come from that class's own pool.** Proof: the class is the item of
   a `List` (anything built on `TypedMemory<T>`), and no code that can run on another thread counts, makes or frees
   one (C5's proof, extended to making). Faster form: a free list and a bump pointer per class, runs of blocks
   aligned to the cache line. Falls back: the C library's `malloc`. Hand-edited (the four component classes
   pooled): 60.3 to 43.1 ms. The rest of the gap the component reads showed was where the objects lay, not their
   counts: removing only the counts moved the same cache misses into the next read (59.4 to 53.4 ms, item 2).
2. **B3 (new): a list item read only to test it is not counted.** `crash values[row]`, `crash keys[index]`,
   `crash headers[index]` each retain and release the item just to test it. Proof: the value is used only by the
   test, with nothing between. Faster form: the slot's pointer tested. Falls back: the counted read. Hand-edited
   (all 58 such reads): 59.4 to 53.4 ms, and 43.1 to 39.7 ms after M6.
3. **L8: the candidate list and the second match.** Dropping the first match by hand (every stress entity matches,
   so the program still prints the same): 59.3 to 53.7 ms, and 43.1 to 37.8 ms after M6. The list is an attribute
   of the runner read through `choose`, not a local read in order by the next loop, so L8 as written does not reach
   it; it needs the runner's loop and `candidates()` seen as one producer and one consumer.

Writing each row back (24%) is larger than B3 or L8, but the last pass measured eliding just the store at about
3 ms; most of it is the per-attribute machinery around the store (`Row.store`, header reads, `stamp_written`), the
same reads and counts B3 and B1 remove.

**Built: M6** (D517), the largest and general: any program that keeps many objects of a few classes in lists,
made in turn, gets them side by side.

| | before | after |
|---|---|---|
| stress tick, split build | 64.0 ms | 47.6 ms |
| stress tick, one C file | 60.5 ms | 43.2 ms |
| stress despawn, sixty ticks | 113 ms | 21 ms (hand: 22.8) |
| stress spawn | 163 ms | 98 ms |
| stress peak memory | 96 MB | 67 MB |
| physics step | 9.5 ms | 8.5 ms |
| the five whole programs of `benchmarks/` (`vector_maths`, `particles`, `number_dictionary`, `text_building`, `sorting`) | | the same C, byte for byte: no class there is both held in a list and made as an object |

Pooling every class instead of the ones a list holds was measured too: `vector_maths` ran in 0.57 of the time (its
`Vector3` temporaries reuse one block), but `particles` ran 4% slower, traced to where its single `Vector` header
landed, and `number_dictionary` moved by as much from code placement alone. So only listed classes are pooled; the
rest is in [status.md](status.md).

After M6 the profile of the stress tick is: writing rows back 32%, matching 28% (half of it in `candidates()`),
filling rows 18% (reading the components themselves 5%), the candidate list 15%, `choose` 9%. Next, by the
measurements: B3 with B1 for the held locals of `find_row` and `fill_attribute`, then the runner's matching once
(L8's shape, or T4b's unrolling, which would make each system's row loop plain code), then C4 across units (the
split build is still 4 ms behind the one-file build).

### Fourth pass (2026-10-07)

Engine `naive` at 81b0e20, Spite from 303bfbc3, the same machine, `--optimized`, medians of 5 interleaved runs, the
same sampler (about 14 000 samples over three runs of the one-file build). Before: stress 48.1 ms a tick (split
build; 43.3 ms as one C file), physics 8.4 ms.

**Built: B3** (D518), a list item read only to test it is not counted: the matcher's `crash headers[index]`,
`crash kinds[index]`, `crash keys[index]` and the columns' `crash values[row]` test the slot in place.

| | before | after |
|---|---|---|
| stress tick, split build | 48.1 ms | 43.8 ms |
| stress tick, one C file | 43.3 ms | 40.3 ms |
| physics step | 8.4 ms | 8.1 ms |
| the five whole programs of `benchmarks/` (`vector_maths`, `particles`, `number_dictionary`, `text_building`, `sorting`) | | the same C, but for the numbers of its temporaries |

The profile after B3, inclusive, both systems summed: writing rows back 36%, matching 23% (`find_row` 17%), filling
rows 19%, the candidate list 13%, `choose` 9%. The counts left are in reads that are not tests: `choose` reads
`candidates[index]` twice per entity, each a counted `List<Integer>` (4% of the tick in its count and release), the
matcher passes its `rows` attribute to `match_into` (counted, 4%), `stamp_written` passes `headers[index]` to
`changes.stamp_written` (3%), and the `header` locals of `find_row`, `fill_attribute` and `store_attribute` are
counted reads (B1).

**Built: B1 for named items** (D519): the `header` of `find_row` and `fill_attribute` is the slot's item, passed to
`present_in`, `passes_tracking` and `slot.fetch` as held. What made it possible is a finer call effect: a store
with `[]` now says which list it writes, so `found[index] = row` (a `List<Integer>`) and `fill_from`'s attribute
writes no longer count as letting go of anything the matcher holds. `store_attribute` keeps its count: it calls
`Column<$component_type>.write_at`, whose list of the generic class's own item could, as far as the per-class
effects know, be any list.

| | before | after |
|---|---|---|
| stress tick, split build | 43.6 ms | 41.2 ms |
| stress tick, one C file | 40.0 ms | 38.1 ms |
| physics step | 8.1 ms | 8.2 ms (noise: its changed system, `SortColliders`, 662 to 650 µs; the unchanged `MoveCharacters` moves 1% between builds) |
| the five whole programs of `benchmarks/` (`vector_maths`, `particles`, `number_dictionary`, `text_building`, `sorting`) | | the same C |

The profile after B1, inclusive, both systems summed: writing rows back 38%, matching 21% (`find_row` 15%), filling
rows 20%, the candidate list 12%, `choose` 9%, `stamp_written` 8%. The `ColumnIndex` counts are now all in
`stamp_written`, which passes `headers[index]` straight to a call; with `choose`'s two reads of `candidates[index]`
(4% in their `List<Integer>` counts) they are items used at once without a name (pair B1t, proposed).

**Measured by hand after B1, not built: the runner matching each entity once.** Three hand edits of the one-file C
(the same C otherwise, `clang -O3`, medians of 5 interleaved runs on a quiet machine, against 37.9 ms):

| Hand edit | What it stands for | Stress tick |
|---|---|---|
| the 23 items read with `[]` and used at once as a receiver (`choose`'s two among them) read uncounted | B1t | 37.8 ms: noise |
| `Runner<Move>` and `Runner<Regenerate>` walk the driver column themselves, match each entity once and fill, run and store it at once: no candidate list, no `choose`, no second `matches` in `fill_into` | L8b, the narrow form of L8 for the runner | 31.0 ms |
| the same two runners specialised for the one configuration their matcher's `headers`, `kinds` and `keys` hold after setup: the smaller column walked, the other looked up with `row_of`, the components read in their slots, the system's body inlined, each entity stamped as written; no matcher, row object, candidate list or write-back | S1 (D518's partial evaluation per configuration) | 4.0 ms |

S1 is the pair that pays: a single thread at 4.0 ms against the hand engine's 7.0 ms on two threads (12.8 ms on
one), the matcher's whole machinery gone. B1t is not worth building before it. L8b is a special case of S1 (it
removes the matching twice but keeps the generic matcher), so it is not built either.

What S1 needs, phrased per use: the runner's loop and the matcher it calls are driven by `headers`, `kinds` and
`keys`, lists filled once by `prepare()` from the row's attribute classes and never written after (so their
contents are a function of the configuration, the row class, known while compiling); a specialised copy of the
runner's `run_<phase>_each` for that configuration, with every read of those lists folded to the column it names;
then the existing proofs run on the folded code (the item read in its slot, the write-back of the same object
dropped, the call inlined). The fallback is the generic loop, for a configuration that cannot be folded (a list
written after setup, a key computed from data). [naive_programs_pairs.md](naive_programs_pairs.md) has the rows.

### S1 design (2026-10-07)

Proposed by Claude, decided under D509 for the parts built. The aim is general: a table a program fills with a few
known values and then only reads (a parser's character classes, a pipeline's steps, a state machine's transitions,
a matcher's kinds), not anything an entity system owns.

**What the 4.0 ms hand edit is made of.** Measured on the same one-file C (`clang -O3`, medians of 5 interleaved,
against 38.5 ms that day):

| Hand edit of the two matchers (`Row<Moving>`, `Row<Mending>`) | Stress tick |
|---|---|
| `headers.count()` and `kinds.count()` read as 2 | 37.1 ms |
| every comparison of a kind with a number it can never be folded (kinds are 0 or 3), including `passes_tracking`'s `kind` | 34.5 ms |
| the same without `passes_tracking` (a parameter) | 36.8 ms |
| the kinds and counts read as the constants 0, 0 and 2 | 32.8 ms |

So folding the configuration into the matcher buys 15%; the rest of the 4.0 ms is the runner itself (no candidate
list, no row object, no write-back, the system's body inlined), which is L8b, B2b and keeping the row in registers,
not S1. S1 is built in three steps.

**What "setup" is: no phase, an invariant.** Proving that a phase ends before the hot loop needs the whole program's
order of calls. Instead the compiler proves a fact that holds at every moment: every value the list can ever hold
comes from a write it can see, and it can list them. A list attribute qualifies when, in the whole program:

- it is declared `var name = List<T>()` (empty) and never assigned;
- the only calls that put a value into it are `name.append(v)`, `name.prepend(v)` and `name.insert(i, v)`, written
  in its own class (the attribute itself, not a copy of it);
- everywhere else it is only read: `count()`, `is_empty()`, `contains(...)`, `index_of(...)`, `first()`, `last()`,
  `copy()`, a `[]` read, a test of a `[]` read, or shrunk (`clear()`, `remove_...`, `truncate`, `swap`), or passed
  to a program function whose parameter is only read in those ways;
- no reflection reaches its class's attributes (no walk over them, no `attributes[...]`, no run-time attribute
  list, no serialiser for the class).

Then every item it holds, at any time, is one of the values its writes can append. Since nothing is assumed about
when the writes run, there is no window "before setup" to get wrong, no flag and no run-time test.

**Where the values come from.** The value each write appends is worked out while compiling, as a set of constants
or "unknown": a whole-number or enum literal; an item of another such list; a parameter of a function every call
of which the compiler sees (its set is the union of what each call passes); a local (the union of everything
assigned to it in its function); or what a function returns (the union of its `return`s, following the branches a
codegen question decides, so `Slot<Position>.tracking_kind()` returns only 0). Anything else is unknown, and one
unknown write makes the whole list unknown. Sets hold at most 16 values.

**Step 1 (built, the fifth pass): tests against values a list never holds fold.** A comparison `item == c` or
`item != c`, where `item` is a `[]` read of such a list, a parameter or a local whose set is known and `c` is a
literal outside the set, is `false` or `true`; a set of the one value `c` decides it the other way. The comparisons
are written into the C as they are and decided once the whole program has been generated (every write and every
call seen), the way tree shaking's guards are, so there is no second pass. Falls back: the comparison is tested
at run time, as before. Cost: nothing at run time; the branch a folded test removes stays in the C text and the C
compiler drops it, so a function only that branch calls is still compiled.

**Step 2 (designed, not built): the configuration folded, a copy per configuration.** Exact contents need the
count and the order, so they need setup after all: the writes run in a function that runs at most once per object
(its constructor, or one that starts `assert not prepared` then `prepared = true`, with `prepared` written nowhere
else), each write unconditional at the top level of its function. Then a list whose count is the number of writes
holds the writes' values in order and never changes again. Each function that reads the lists gets one copy per
configuration (the product of the positions' sets, at most 4), in which a read is the constant and the count is a
number, and a compiled dispatch at the functions other code calls picks the copy (`count == 2 and items == 0, 0`)
or the generic body: a branch between forms compiled in advance. Measured by hand: 34.5 to 32.8 ms over step 1.

**Step 3 (other pairs): the runner made direct.** With the matcher folded, the runner's loop is still a candidate
list, a row object filled and written back, and a call: L8b, B2b and the row kept in registers take it the rest of
the way to the hand edit's 4.0 ms.

### Fifth pass (2026-10-07)

Engine `naive` at 81b0e20, Spite from 3bc157e9, the same machine (slower this afternoon: the base tick measured 39.0 ms
as one C file against 38.2 earlier), `--optimized`, medians of 7 interleaved runs.

**Built: S1 step 1** ([the design](#s1-design-2026-10-07)): the values a list only its class fills can hold are
listed while compiling, and a `==` or `!=` against any other value is decided. In the engine the matcher's `kinds`
of every row with plain components are 0 or 3 (`tracking_kind()` returns 0 for them, and `describe_attribute`
may set 3), and `passes_tracking`'s `kind` is only ever passed an item of `kinds`, so the tests for kinds 1, 2, 4
and 5 in `find_row`, `smallest_header`, `removal_candidates` and `passes_tracking` are gone (87 tests over the
stress program, the setup's included).

| | before | after |
|---|---|---|
| stress tick, one C file | 39.0 ms | 35.4 ms |
| stress tick, split build | 41.9 ms | 41.3 ms |
| physics step | 9.2 ms | 8.9 ms |
| the five whole programs of `benchmarks/` (`vector_maths`, `particles`, `number_dictionary`, `text_building`, `sorting`) | | the same C |

The split build gains less: `passes_tracking` and `find_row` sit in units of their own there and are not folded
into their callers, which is pair C4. Every headless check of the engine's baseline prints the same before and after, but for
timings and the counts that follow them and differ between two runs of one build too (`animation_check`'s skipped
throttle ticks, `replication_check`'s busy one, `unreliable_check`'s datagrams, `clock_check`'s game times);
`stream_bench`'s rows are 3 to 6% faster and `relations_check`'s tick 10.2 to 9.8 ms.

What remains of S1 is step 2 (a copy per configuration with a compiled dispatch, 35.4 to about 33.5 ms by the hand
edit's ratio) and step 3, the runner made direct (L8b, B2b, the row kept in registers), which is where the rest of
the hand edit's 4.0 ms is.

### Sixth pass (2026-10-07)

Engine `naive` at 81b0e20, Spite from 54ecc98f, the same machine (shared with other sessions all day, so compare
numbers within one table), `--optimized`, the one-file C built with `clang -O3`, medians of 5 interleaved runs.
Before: stress 40.4 ms a tick as one C file.

**What the 4.0 ms hand edit is made of.** The S1 hand edit replaces each runner's loop with the driver column walked
once, the other column looked up, the components read in place, the system's body inlined and two stamps; it ran in
4.7 ms that day. It was taken apart into hand edits that each stand for one general optimisation, applied in turn
to the same C (`edits.py` in the session's scratch folder):

| Hand edit | What it stands for | Stress tick | Saved |
|---|---|---|---|
| none | | 40.4 ms | |
| F: the runner walks the driver column and matches each entity once, no candidate list, no `choose` | L8b, the loop fused | 39.0 ms | 1.4 |
| M: the second `matches(entity)` in `fill_into` dropped | a call repeated with the same arguments and nothing changed between | 35.7 ms | 3.3 |
| W: `store` keeps its stamps and writes nothing back | B2b, a write-back of what was read, across calls | 23.9 ms | 11.8 |
| R: the row object never filled, the components fetched into locals, the system's body on them | scalar replacement of the row, with the call inlined | 10.7 ms | 13.2 |
| G: the runner calls the row's unguarded functions | the singleton's lock | 10.6 ms | 0.1 |
| K: `kinds` read as 0 and two headers | S1 step 2, the configuration folded | 9.8 ms | 0.8 |
| the S1 hand edit itself | the matcher reduced to the other column's `row_of` | 4.7 ms | 5.1 |

Taken alone, W saves 10.8 ms (29.6), M and F need each other, and the write itself (`set_at` with its counts) is
only 2.5 of W: the rest is the per-attribute path to it (cursor, header reads, `Slot.store`, the column's lock). Every
singleton lock removed at once (each guarded wrapper calling its body, every outside access plain) took the tick
from 76.4 to 62.7 ms on a busy machine (18%), and from 44.0 to 34.3 ms on top of F, M and W: the locks were the
largest single general cost, spread over every call the runner makes to a matcher or a column. They are there
because the program makes a `Parallel` (a recipe catalog loading on the pool) and the walk that decides locks
counts every function made into a value as something a thread might run: the matchers' `found.filter(matches)`
makes every matcher reachable, and the columns with it.

**Built: no lock for a singleton no other thread reaches** (D529), the general form of that measurement: the walk
already made over the written-out C for plain counts (C5) decides it again, and a singleton none of the code that
can run on another thread names keeps no lock and no atomic attribute.

| | before | after |
|---|---|---|
| stress tick, one C file | 43.3 ms | 36.9 ms |
| stress tick, split build | 50.3 ms | 42.7 ms |
| physics step | 13.4 ms | 12.6 ms |
| the case, ten million calls (medians of seven) | 41.1 ms | 24.3 ms |

Every singleton of the stress program but the recipe cookbook (which the cook task calls) loses its lock.

**The counts left, measured again after the locks** (base 34.5 ms that round). The largest self time was now in
releases: the four component classes' releases 15.7% of the tick, `List<Integer>` releases 7.7%. Three hand edits
of the counts the runner's calls add for their callees:

| Hand edit | Stress tick |
|---|---|
| none | 34.5 ms |
| the matcher's `rows` passed to `match_into` uncounted | 34.5 ms |
| each component passed to `Slot.fetch` and `Slot.store` uncounted (`row.attributes[attribute]`) | 32.6 ms |
| the row passed down `fill`, `fill_into`, `fill_attributes` and each `fill_<attribute>` uncounted | 32.4 ms |
| all three | 29.2 ms |

Each is an attribute, or a path of attributes, passed to a call that cannot assign it: the general rule is built
below. With the same edits on top of F and M (the fused loop and the single match) the tick is 23.0 ms; W and R are
still the largest parts of what is left.

**Built: an attribute a call cannot assign is passed without counting** (D531): the held argument extended from
names to attributes and paths of attributes, checked against the callee's call effects, through shapes' attributes
read with their reading function, and passed on by a held parameter.

| | before | after |
|---|---|---|
| stress tick, one C file | 36.0 ms | 30.5 ms |
| stress tick, split build | 42.3 ms | 36.7 ms |
| physics step | 12.5 ms | 12.0 ms |
| the case, ten million calls | 28.4 ms | 5.4 ms |

**Measured again after held attributes** (base 28.3 ms that round): the write-back alone (W) is now 6.6 ms, the
row in locals with the system inlined (R) another 6 on top of F, M and W, the fused loop (F) 2.5, the single match
(M) 1.5, and the items `choose` and `stamp_written` read with `[]` and use at once (B1t) about 1. Of W, storing the
same object back into its slot with a count up and two down is 2.2 ms (a run-time identity test skipping it,
measured by hand) and the per-attribute path to it the rest.

**Built: an item used at once is not counted** (D538), pair B1t: `candidates[index].count()`,
`candidates[index][picked]`, `headers[index].stored` and `changes.stamp_written(headers[index], entity)` read the
slot in place.

| | before | after |
|---|---|---|
| stress tick, one C file (medians of nine) | 27.3 ms | 25.9 ms |
| the case, ten million passes | 23.9 ms | 20.7 ms |

**Built: storing an object into a list counts it only when it changes the slot** (D544), the run-time form of the
write-back measured above (J): `values[row] = value` from a held name is written in place, the slot compared with
the new object and counted only when it changes. It is a test the run makes because whether `store` writes back
what `fill` read is what B2b would have to prove across the runner's calls, and that proof is not built.

| | before | after |
|---|---|---|
| stress tick, one C file (medians of nine, a busy machine) | 38.0 ms | 33.3 ms |
| stress tick, split build | 45.7 ms | 43.3 ms |
| physics step | 16.2 ms | 16.1 ms |
| the case, ten million stores | 39.6 ms | 9.8 ms |

**What is left of the hand edit, and why it is not built yet.** Four general optimisations are built in this pass;
the parts of the gap still open are each a proof about the runner as a whole, not a rule about one call:

- **R, the row kept in locals with the system inlined** (the largest part left, about 6 ms of a 28 ms tick): the
  row is `Row.current`, an attribute of a singleton that lives for the whole program, filled by template calls
  through `cursor`, read by the system through a shape, and stored back. Scalar replacement needs the object to be
  short-lived; making it so needs the runner's calls inlined into one body at the Spite level (pair C4) and
  `cursor`'s values followed through them, which is partial evaluation (S1 step 2 and beyond), not escape analysis.
- **W, the rest of the write-back** (about 3 ms after the stored-in-place store): `store_attribute`'s path to the
  store (cursor, `headers[position]`, still counted because a generic class's own list is not told apart per
  instance, `Slot.store`, the asserts). B2b proper needs `values[found]` in `store` proven to be what `fill` read
  across the system call: the same partial evaluation, since `found` is `rows[cursor]` in both.
- **F and M, L8b's fused loop and its single match** (about 4 ms): the candidate list is a `List<List<Integer>>`
  attribute of the runner, built by `gather_arguments` and decoded by `combinations` and `choose`, so the producer
  and consumer are not one list read once in order by the next loop, the shape L8 can fuse. Fusing it needs the
  argument count known (one argument, so one candidate list and `choose(c)` reading `candidates[0][c]`), which is
  again a fact of the configuration the runner was built for.
- **K and the matcher reduced to one lookup** (about 0.8 and 5 ms): S1 step 2, designed above.

So the next step is S1 step 2 with C4 across the runner's calls: one copy of `run_<phase>_each` per configuration,
with the template calls inlined, in which R, W, F, M and K all become the existing per-call rules applied to one
body.

### Seventh pass: systems on several cores (2026-10-08)

Engine `naive` at 81b0e20, Spite from 1afb00d9, the same machine (busy with other sessions: the processor sat
around 85% before any run), `--optimized` split builds, medians of 5 interleaved runs unless said.

**What the cores are worth by hand.** The stress stage loop changed by hand in the C to run `Runner<Move>` on a raw
thread while `Runner<Regenerate>` runs on the program's own (the hand engine's `Parallel` per system): the update
stage went from about 30.5 to 17.5 ms with every count as the compiler left it (plain for the classes C5 found no
thread counting), and the tick from about 29 to 21 to 25 ms with every count made atomic (medians of 9 swung by a
third on the busy machine). So two cores are worth about 1.75 times on the stage, and atomic counts take back a
good part of it: pair S4, or count disciplines per group of overlapped calls, matters as much as the overlap.

**Built: a loop over a list of different classes runs them at once** (D539, pair T4b). `stage.each(run_runner)` in
`App.run_stage` is reached. The stages are built at run time by `place` and `conflicts`, comparing the strings each
runner reports, so the pair's unrolling (count and order known while compiling) does not apply; instead the
compiler writes, for every two classes the shape's call reaches, whether their calls are independent, and the loop
reads the classes when it starts. The same commit makes D505's facts see what its rule always allowed: a clock read
through a foreign call (`QueryPerformanceCounter`) is not a call out, nor is a singleton's outside-access guard, nor
a crash's report to the error output; before it, every runner looked as if it called out of the program.

The engine's two systems still share, as far as per-class facts see: the four fields of `Profile.Timing`
(`timing.record` in `run_once`, each runner's own object), the items of `ColumnIndex.stamps` (each column's own
index, found through `Columns.headers` by name), and the items of `List<Integer>` reached through parameters (the
candidate lists). So the table has no pair for them and the stage runs in order.

| | before | after |
|---|---|---|
| stress tick | 30.5 ms | 30.2 ms (the same C) |
| physics step | 9.2 ms | 9.1 ms (the same C) |
| the case, three voices | 257 ms | 91 ms |

What would let the stress systems overlap, in order of what it takes: facts that tell apart objects an attribute
holds when the attribute is made for its object and never assigned again (the `Profile.Timing`); the same for the
list items reached through such an attribute (`chosen`, the candidate lists); and either the matcher's columns
folded to the column they name (S1 step 2) or a fact that each runner's rows name columns no other runner names.

**Built: a loop whose passes write only their own item runs in bands** (D542, pairs T1 and T6), for
`list.each_function()` over a `List` or `Vector` of a class. The overlap facts gain a self-indexed key: what the
function reaches through its own `self`, and through `self` of every function it calls on `self`, is told apart
from any other object of the class. The cost model was measured with the bands forced on (the machine busy, medians
of 3): starting the bands costs about 80 to 90 µs; a pass weighing 203 (a 20-step loop) ran 48 to 86 µs over
1 000 elements, 482 to 122 over 10 000 and 48 to 6.5 ms over a million; a pass weighing 7 (six statements) was
slower at every count, four times at a million, and `image_filter_over_planes`'s two passes over 4 million pixels
(weighing about 50) went from 186 to 230 ms. Hence the two constants: a pass under 256 stays in order, and above
it bands start when the count times the weight reaches a million. The bands' function value borrows the list, so
the list's release does not become a root of C5's walk: before that, the item class lost its plain counts and its
pool, and the image case went from 184 to 464 ms.

| | before | after |
|---|---|---|
| the case, 100 000 orbits ten times | 800 ms | 51 ms (expert C on every core: 49) |
| stress tick, physics step | | the same C |
| every other case | | the same C (four gain blank lines) |

The engine has no loop of this form: its systems' row loop is a `while` over combinations that writes the
runner's own scratch (`chosen`, `position`, `skipping`) and stores through the matchers. Reductions (T2), scratch
per band (T8) and loops written as a `while` are not built.

### Ninth pass (2026-10-08)

Engine `naive` at 81b0e20 in a measuring copy moved to `Dictionary<String, Value>` (D541) by hand, with its few
address-based file reads stubbed (none is on the stress or physics tick; the engine's own move is in progress on
its branch), Spite from 1a69e005, `--optimized`, medians of 5 interleaved runs. **Every number in this pass was
measured while the machine was in other use and is provisional**; a quiet re-timing is to follow.

**The hand edits again, on today's C** (one C file, `clang -O3`; base 20.6 ms): F (the fused loop) 1.3 ms, M (the
repeated match) 1.4, W (no write-back) 4.1, R (the row in locals with the system inlined) 5.5, K (the configuration
folded) 0.8; with all five the tick is 7.4 ms. W alone saves 3.5.

**Built: a table filled once is read as constants** (S1 step 2). The design's "setup" is not proven but traced: the
one function of the class that fills the list and that no other filling function calls (its constructor, or one
that starts `assert not prepared` then `prepared = true`), walked in the C through the calls it makes at its top
level, gives the count and the order; each `append`'s values are step 1's. Nothing relies on the guess: each
function that reads the list (and that can reach none of its writers) gets a copy per combination of the values
(at most four) reading the list as a constant, or only its count for a list of objects, and the function starts
with a test of the object's list that picks the copy. A wrong guess costs only the copy's size. In the engine, the
six matchers' `kinds` (each item 0 or 3, so four combinations for two attributes) and `keys` are traced through
`prepare` to `describe_<attribute>`.

| | before | after |
|---|---|---|
| stress tick, one C file | 27.7 ms | 25.7 ms |
| stress tick, split build | 32.0 ms | 30.0 ms |
| physics step | 12.1 ms | 12.3 ms (within the noise) |
| the case, ten million prices | 28 ms | 28 ms (an earlier, noisier pair said 40 to 36) |
| stress C, one file | 3 280 363 bytes | 3 926 005 bytes (+20%) |
| stress executable, one file | 1 325 568 bytes | 1 498 112 bytes (+13%) |

(Medians of seven on a machine running games and other sessions' checks; an earlier pair of runs on a quieter
machine gave 20.8 to 20.1 ms one file and 25.6 to 24.9 split.) 440 functions are copied in the stress program: the
matchers' (four combinations of `kinds` each, three of which this program never makes, the price of not knowing
`is_marker` while compiling) and the runners' that call them. A copy is written only beside a table whose values are
known: copies that knew only counts (the runners' `writing`, a parameter's step) grew the C for nothing measurable.
Each unit of a split build keeps its own copy of the constant lists, so they fold there too. Compiling the compiler
itself takes as long as before (no class of it qualifies; the walks over its call graph are bounded); a first
version that walked each table's writers by scanning every function of the class took the compiler's own build from
5 to 15 minutes, which the bound and the call index fixed. The other cases of `benchmarks/` compile to the same C.

**Not built: the runner's calls inlined into one body at the Spite level (pair C4).** What it would take, found
while trying:

- The runner's per-entity path is made almost entirely of member templates (`fill_arguments()`,
  `fill_attributes(row, entity)`, `store_attributes`) and calls into other classes (`Row<T>`, `Slot<T>`,
  `Column<T>`). The generator expands a template while writing C, with the member's symbol bound in its own state
  (`bind_symbol_of`), not as a syntax tree; so an inliner working on the Spite tree cannot see what
  `argument.class`, `row.attributes[attribute]` or `argument.index` stand for, and one working inside the generator
  has to save and restore about thirty pieces of per-function state (scope, held and lent parameters, frame
  candidates, the item-value function) around every inlined body, and turn the callee's `return`s and every
  `assert` (an early return) into jumps out of the inlined block.
- Even inlined, the rules that would then apply are written for names, not for the attribute paths the runner uses:
  B2 drops `list[i] = x` only after `var x = list[i]` in the same block, while the runner stores `row.current.position`
  back into `values[rows[cursor]]` with `cursor` reset to 0 between, so W needs B2 extended to paths and a value
  followed through an attribute (`cursor`); R needs `Row.current`'s attributes proven dead between passes, which is
  whole-program, since `current` lives as long as its singleton.
- So R, W, F and M do not follow from C4 alone: each still needs its own fact (paths in B2, attribute constants,
  the candidate list's producer and consumer fused). They are the next work, in that order of size (W 4.1 ms, R 5.5,
  M 1.4, F 1.3 on a 20.6 ms tick).

**Measured and dropped: a function called from one place compiled in its caller's unit.** The split build (25.6 ms)
is slower than one C file (20.8 ms) on the same C. Placing every function that has one caller in its caller's
translation unit, so the C compiler sees the pair together, changed nothing (25.0 to 25.1 ms, medians of seven while
the machine was in other use), so the gap is not calls split across units by the name hash. Not committed.
### Eighth pass: objects told apart, counts per group (2026-10-09)

Engine `naive` at 81b0e20 (a copy with its one-type `Dictionary`s written `Dictionary<String, ...>` and its four
pointer reads of files stubbed, since today's library refuses both; neither runs in stress or physics), Spite from
68982022, `--optimized` split builds, medians of interleaved runs. **Every number here was measured while the
machine was in other use (games and other sessions' builds): provisional, to be timed again on a quiet machine.**

**Built: objects made for their owner are told apart** (D556, [optimizations](../docs/optimizations.md#objects-made-for-their-owner-are-told-apart),
[proofs](../docs/proofs.md#an-object-made-for-its-owner-is-told-apart)). The overlap facts gain owner tags: an
attribute only ever given an object constructed where it is given (read from every assignment in the C, struct
copies and `copy()` included) holds an object no other such attribute holds, so a key reached through it carries
the attribute (`f:Profile_Timing#runs_@Runner__System_Move#timing_`), and keys with different innermost tags never
touch. An object passed to a function that only reads and writes through its parameter, passes it to a list's own
functions or to another such function, or compares it, is followed into it (the parameter's keys are written again
with the argument's tags). An attribute that is besides kept nowhere else (never read into anything but those uses)
is sealed: a key with no tag cannot reach it. An item read out of a singleton's list attribute carries an item tag
(`@Row__System_Move_Moving#headers_[]`); two different item tags touch only when the two lists share an object,
which the T4b loop checks when two such classes meet (a few pointer comparisons). The pass also taught the facts
the in-place store of D544 (`items_)[i] = v`, which they had read as a write through an address, making every runner
look as if it wrote raw memory), and fixed `made_here` (a local assigned fresh once and later reassigned from a call
was taken for a list made in the function).

Everything the seventh pass found the two stress systems sharing is now told apart: each runner's `timing`, its
`chosen` and `candidates` lists (sealed), the matcher's `rows` (followed into `match_into` and `find_row`), and the
`stamps` of the column indexes each matcher's `headers` holds (checked when the stage starts: Move's headers name
Position and Velocity, Regenerate's Health and Regeneration).

**Built: counts per group of calls run at once** (D555, pair S4, [optimizations](../docs/optimizations.md#counts-stay-plain-for-what-one-of-the-calls-run-at-once-counts),
[proofs](../docs/proofs.md#what-one-of-the-calls-run-at-once-counts)). The walk that decides plain counts (C5) and
singleton locks (D529) no longer takes a T4b piece as code that runs anywhere: it follows each class's call on its
own (calls only, a singleton's teardown left out, the piece itself counting only its elements), and a class is a
meeting point only when two calls the loop's table lets run together both count it, or both make or free it.
Everything else keeps plain counts, its pool and no lock (the overlap facts already prove the calls touch nothing
in common). A meeting point counts atomically only while the loop runs at once (a flag the loop sets, read with a
predictable branch), and its pool is kept per thread. In stress the meeting points are `ColumnIndex`, `Removal`,
`Removals`, `List_Integer` (already atomic for the asset loader), strings and reflection; the components, rows and
matchers each runner counts stay plain.

| | before | after |
|---|---|---|
| stress update stage, split build | 41.6 ms | 26.1 ms |
| stress, each system inside the stage | 20.5 and 21.0 ms | 22.5 and 24.6 ms (run at once) |
| stress, sixty ticks after despawning everything | 27.9 ms | 33.0 ms |
| physics step | 13.4 ms | 13.7 ms (within the noise of that run) |
| the case, two tracks with a meter each (medians of 5) | 124.2 ms | 65.3 ms (expert C on two threads: 62.5) |
| the case, two collections counting their objects (medians of 5) | 21.7 ms | 14.1 ms |

On a less loaded hour of the same day the stage went from 28.1 to 15.0 ms split, and the one-file C from 23.8 to
12.5 ms, under the hand edit's 17.5 ms. The two systems each run about a fifth slower side by side than alone
(memory and the second core's caches), so the stage is a little more than half its old time.

**What this costs.** Despawning got slower: the first tick after despawning everything (the flush) reads back the
columns the worker last wrote, from the other core's caches, and every later tick starts and joins the pool for
two systems with nothing to do (about 20 µs each). Measured by hand on the one-file C: with the stage forced in
order the sixty ticks are as before; with the meeting points counted plainly they barely move. D214 asks for at
least as fast everywhere, so this is listed for Mortaro under "To confirm" with D555. A cost model that leaves
calls with no rows in order (pair T6 for T4b) would take the empty ticks back.

Not built, and next: `ColumnIndex` counts in the matcher's `store_attribute` (`var header = headers[position]` is
counted per entity because B1's call effects still see any `List<ColumnIndex>` written by a call; the owner facts
of this pass would let B1 see it is not); per-creation-site counts (R9), which would let a meeting point's objects
that only one call ever sees stay plain; and a function value's owner, still taken as let go on another thread,
which makes every class the owner reaches atomic in a program that hands its work to `Benchmark`.

### Physics pass (2026-10-09)

Engine `naive` at 3100303 and `main` at 346efda, Spite from 4a4c6120 (rebased onto 106d5e56), the same machine,
`physics_bench` (5 000 characters, 10 000 boxes, 200 triggers) built `--optimized`. Hand edits were made to the
one-file C and built with `clang -O3`; "best" is the best of 9 to 11 interleaved runs, which on this shared machine
is steadier than the median. The coordinator's quiet measurement on master 8ba6a9fa: step 5,691 µs original against
6,941 naive; `MoveCharacters` 4,966 against 5,692, `SortColliders` 275 against 688, `ReadCharacters` 179 against 362.

**How it was profiled.** The third pass's sampler, written again (`sampler.c` and `prof.py` in the session's scratch
folder): a thread suspends the program's thread about every 0.1 ms while the 100 ticks run, walks its stack with
`RtlVirtualUnwind`, and `llvm-symbolizer --inlining` names the frames. About 35 000 samples over three runs of each
form. Inclusive microseconds per tick, the naive form scaled to an 11.6 ms tick and main to 8.3 ms (that hour's
loaded machine; the shares are what matter):

| Function | main | naive | What main does by hand |
|---|---|---|---|
| `Colliders.collect`, the broad phase | 1 350 | 2 905 | the grid is a counting sort into three flat `List<Integer>` (starts, entries) and a stamp per slot |
| `Grid.gather` within it | 1 098 | 2 836 | reads two neighbouring starts and a run of slot numbers; naive reads a pointer to each bucket's list object, the object, then its items, and counts each list and each collider it passes on |
| `Grid.build` (`SortColliders`) | 217 | 1 443 | clears and refills the same three arrays; naive frees 65 536 bucket lists and makes 65 536 new ones a tick, each growing from nothing |
| `set_aside_candidates` and `forget_candidates` | 0 | 690 | a new stamp per query; naive keeps a `visited` list and clears `gathered` flags |
| the runner's fill and store of a row (`MoveCharacters`, `ReadCharacters`) | 0 | 1 100 | a `Stream` over the columns; naive's runner, the subject of S1 step 3 |
| narrow phase (`Geometry.sweep`, `squared_distance_to`) | 6 279 | 5 655 | the same code; naive is no slower here |

So the gaps are the grid's representation and what the runner does per row; the narrow phase, colliders as objects
and `seen_from`'s returned shape (built in the caller's slot already, M5) cost nothing measurable.

**What each gap is worth, by hand.** Each is an edit of the naive build's C (`edits.py` in the scratch folder).
Ticks are the best of 9 to 11 interleaved runs; each row is read against the unedited build of its own run (in
brackets), since the machine's speed moved between runs:

| Hand edit | What it stands for | Tick µs | `SortColliders` | `MoveCharacters` |
|---|---|---|---|---|
| A: `gather` reads its buckets and their items uncounted | B1 and B1t on the bucket reads | about 2% faster (loaded machine) | same | about 2% |
| G: a pooled list keeps its storage when freed | storage kept with the pooled object | about 3% (loaded machine) | 1 404 to 1 101 | same |
| B: the buckets keep their lists and storage from one build to the next | L6 for a list of lists | 7 339 (7 674) | 336 (684) | 6 218 (6 230) |
| I: each bucket's whole list object inside the outer list's block | M8b, built below | 7 236 (7 498) | 658 (667) | 5 858 (6 057) |
| H: each bucket as an inline record of count, room and items, kept from build to build | M8b with a 16-byte record, plus B | 6 890 (7 674) | 239 (684) | 5 895 (6 230) |
| D: the grid as one flat array of colliders and an array of starts (a counting sort) | what main does | 6 485 (7 674) | 175 (684) | 5 581 (6 230) |
| R: I, plus clearing the list of lists keeps each slot's room for the next empty list put there | M8b plus L6 inside the slots | 7 105 (7 610) | 411 (675) | 5 956 (6 139) |
| main, the hand engine | | 6 069 to 6 162 | 249 to 256 | 5 349 to 5 411 |

D, the counting sort, is the hand engine's form, and it is faster than main's own `SortColliders`: it closes most of
the gap. It is not general: building it needs the grid's filling loop run twice (once to count), which only a
proof that the loop's other effects can be repeated allows. H, its general approximation, is the inner lists merged
into the outer list (M8b) with their room kept (L6): within 0.4 ms of D. I measures M8b alone and R adds L6 inside
the slots, the two steps below.

**Built: a list held only by another list lives in its slot** (D552, pair M8b). A typed study of every function the
program compiles (`study_nested_lists`, after its body is generated, with the locals' types the scopes recorded)
proves that no inner list of a `List<List<T>>` is named but through its slot; then a pass over the written C
(`nested_lists.spite`, before function folding) checks that only those functions and the list's own touch the slots,
and rewrites `TypedMemory<List<T>>`'s five functions and every slot read for the inline form. In the engine program
`List<List<Physics.Collider>>` (the grids' buckets and `owned`) is inlined; `List<List<App.Runnable>>` (a stage is
passed to `conflicts`), `List<List<String>>` (the recipe watcher keeps one) and `List<List<Integer>>` (the runner's
candidates come from a call) are not.

| | before | after |
|---|---|---|
| physics tick, one C file, best of 9 (quiet) | 7 674 µs | 7 358 µs |
| `MoveCharacters`, one C file | 6 230 µs | 5 926 µs |
| physics step, split build, best of 11 | 7 260 µs | 7 136 µs |
| the case, 400 000 orders in 65 536 lists ten times | about 320 ms | 227 ms (naive C 252, expert C 37) |
| every other case | | the same C |

The split build gains less: the slot's memory functions sit in another unit than the code reading the slots, so it
is link-time inlining that removes the calls (C4 across units).

**Built: a list of lists filled again keeps each list's room** (D553, pair L6b): R of the hand edits, made general.
Clearing marks each slot and keeps its block; the next empty list put there takes it; growth zeroes the room it
adds and letting the list go frees the marked blocks. Against master at 106d5e56 (best of 11, interleaved):

| | master | both built |
|---|---|---|
| physics tick, one C file | 7 628 µs | 7 156 µs |
| physics tick, split build | 7 793 µs | 7 345 µs |
| physics step, split build | 7 402 µs | 6 955 µs |
| `SortColliders`, split | 698 µs | 445 µs |
| `MoveCharacters`, split | 6 091 µs | 5 896 µs |
| `ReadCharacters`, split | 390 µs | 392 µs |
| the hand engine, split: tick, step, `SortColliders`, `MoveCharacters`, `ReadCharacters` | 6 018, 5 869, 293, 5 153, 188 µs | |
| the case, 200 000 orders in 32 768 lists forty times | 316 ms | 106 ms (with D552 alone 267) |

(Best of 11 interleaved runs on a quiet machine, every form built from the same engine commits; the stress
program's C is byte for byte the same before and after both changes.)

So the physics step is 1.09 ms behind the hand engine's, down from 1.53 ms. What is left, by the profile of R: the
broad phase's counted reads of each collider it passes on (the `entries[at]` read and `note`'s append: B1t across a
call that appends to another list of the same class, about 0.1 ms by hand on the quiet machine, Q in the scratch
folder), `set_aside_candidates` and `forget_candidates` (the naive form's visited list against main's stamps, about
0.3 ms), and the runner's fill and store of each row in `MoveCharacters` and `ReadCharacters` (about 0.5 ms), which
is S1 step 3's runner made direct. D, the counting sort, would take `SortColliders` from 448 to about 180 µs and
`MoveCharacters` a further 0.3 ms; it needs a proof that the grid's filling loop can be run twice.

The profile of the built form against main (about 25 000 samples each) puts what is left of the broad phase in its
first touch of each bucket: `gather` is 1.37 ms of a 7.16 ms tick against main's 0.72, and its self time sits in
the count up and down of the slot it reads, the first access to a 40-byte record where main reads 4-byte starts. A
slot is a whole list object (header, `heap`, `values`, items, count, room); H's 16-byte record was worth about 0.3 ms
more. Two general steps would close part of it: attributes bound to a singleton and never assigned not stored in
the object (every `List` and `Dictionary` holds two, so a slot would be 24 bytes and every list 16 bytes smaller),
and a slot record of the items, count and room alone where nothing asks the inner list for its header.

Tried and not kept: letting a held item pass to a call that grows a list of its class (`note(entries[at], found)`
appends to `found`, which the item's own list could be, and growing never lets an item go). It made `note`'s
argument uncounted and changed the C of most programs, and the tick moved by nothing measurable (7 169 to 7 201
µs, best of 11), since the counts were only the first touch of a line the next read pays for anyway.

### Eleventh pass: a system's rows in chunks on every core, measured by hand (2026-10-09)

Engine `naive` at 5574e26 and `main` at 4ad2129, unchanged; Spite at d2ed6188, `--optimized`; the naive stress C
written as one file (`SPITE_TRANSLATION_UNITS=1`, `--c-source`) and built with `clang -O3`. **Measured while the
machine was in other use (a game was running): provisional.** Medians of 5 interleaved rounds, stress tick in µs:

| Build | tick |
|---|---|
| original (main), split build | 6,588 |
| naive, one file, as generated (Move and Regenerate already at once, D555 and D556) | 10,903 |
| naive, by hand: header reads in `store_attribute` borrowed (B1), runner loop as written | about 10,000 |
| naive, by hand: B1, and each runner's row loop in chunks of 4,096 rows on the pool | 6,232 |
| the same, chunks of 2,048 | 6,061 |
| the same, chunks of 1,024 | 5,491 |

The hand edit (D557's form, only in the generated C): the `while combination < total` loop of `run_update_each`
runs as chunks submitted to `ThreadPool` (both runners submit at once, 31 workers); every chunk but the last gets
a private copy of the runner (its own `chosen`) and of the `Row<T>` singleton (its own `rows`, its own `current`
object with empty attributes), reached through a thread-local the singleton's accessor tests first; the last chunk
runs on the shared ones, so the scratch the loop leaves behind is the one the plain loop leaves. Two things had to
change besides: the private scratch is made on the submitting thread (an object made inside a chunk comes from a
class pool that is not per thread), and the counted read of the shared `ColumnIndex` in `store_attribute` had to be
borrowed (B1): with it counted, every chunk counts the same two headers atomically and the tick was 28 ms, three
times slower than in order. The results were not compared value by value.

Where the rest goes (cycle counts by hand): the candidate gathering (`Row.candidates`, a filter over every row of
the driving column calling `matches`) is about 1.2 ms per system and still runs on one thread; the chunked loop
itself scales only about 2.6 times on 16 cores, because the pass touches about 250 bytes per row (two column
slots, two component objects whose counts it writes, two row lookups, two stamps), so two systems at once stream
about 100 MB a tick and memory, not the cores, is the limit. Gathering in chunks too (pair T3, per-chunk lists joined
in order) would take the tick to about 4.3 ms; fewer bytes per row (no count written on a component a pass only
reads, components stored in place: L1) would take more.

**Why it is not built yet: two proofs the overlap facts cannot give.**

1. *Scratch overwritten before it is read (T8).* Most of the shared scratch is plainly written first in every pass
   (`chosen` cleared, `position`, `skipping`, `related`, `cursor`, `filled_entity`, the attributes of `current`).
   `Row.rows` is not: `find_row` sets `rows[index]` on every path but one (`entity >= header.rows.count()`), and
   that one returns false, which ends `match_into` and makes `fill_into` crash. So every slot `fill_attribute`
   reads was written in the same pass, but only through a fact about the answer of `matches` (true means every slot
   was written). The facts would need conditional must-write summaries (what a function surely wrote when it
   answered true), which nothing in the compiler has.
2. *Passes write different slots (T1 through an index).* Each pass writes `values[row]` of two columns, the fields
   of the component objects held there, and `stamps[entity]` of two indexes, where `entity` is the pass's candidate
   and `row` is `ColumnIndex.rows[entity] - 1`, carried from `find_row` to `store_attribute` through `Row.rows`.
   Two passes touch the same slot unless the candidates are distinct and `rows` maps different entities to
   different rows. Both are invariants of `ColumnIndex` (`rows[entities[r]] == r + 1` for every row, kept by
   `add_row` and `remove_row`), and the program even checks each pass's half of it (`crash
   index.entity_at(row) == entity` in `Slot.fetch`), but the compiler proves no invariant of a class, and a check
   at run time would have to recompute the index chain the passes compute, which means slicing it out of five
   functions.

Neither can be checked after the fact: a wrong guess has already lost a write, and the passes cannot be undone. What
would make it buildable, in order of size: (a) conditional must-write summaries for the scratch, and an inverse-pair
invariant for two integer lists of one class, proved from the functions that write them (`add_row`,
`remove_row`), with the candidates' distinctness following from `entities` being one side of the pair; (b) the same
summaries, with the pair checked at run time once per loop (two sequential passes over the index, in chunks, far
cheaper than the loop) instead of proved; (c) a decision from Mortaro that a chunked loop may crash with a named
cause when a run-time check finds two passes reaching one object where the plain loop would have run, which trades
"never a wrong value" (D244) for a loud failure that a correct program would not have.

### Stage 1b: plain bytes and plain foreign structs (medium, library and the foreign call)

The two language gaps that keep the naive engine on `Memory` (D512): bytes from files and sockets become a
`List<Byte>` with reads by position, and a foreign struct holding text, pointers and arrays becomes a `type`. Proposed,
with its questions (305 to 315): [plain_bytes_and_foreign_structs.md](proposals/plain_bytes_and_foreign_structs.md).

### Stage 2: effects as a language-level fact (medium, foundation)

D505's proof reads the generated C. Every later pair needs the same facts earlier and finer: for every function,
which attributes of which classes it reads and writes (per field, per generic instance), which list items, whether
it waits, prints or calls out, and whether a reference to a list item escapes. Building this once as a summary on
the compiler's own tree (and keeping the C reading as a cross-check while both exist) makes stages 3 to 6 cheap.

### Stage 3: loops run in parallel when it pays (large)

- **Proof**: the iterations of a loop over a list are independent (each writes only its own item, or a reduction
  the compiler recognises: sum, min, max, count, append to a private list merged in order).
- **Optimisation**: the loop runs on the pool in bands, results merged in written order.
- **Cost model**: the one-list study measured a blind parallel `each` 2.4x slower on light bodies. The compiler
  estimates a body's cost while compiling; when the count is unknown, one branch at run time picks serial or
  parallel by count (see Q2).
- This retires the engine's 34 `Parallel` uses and its hand band sizes.

### Stage 4: layout decided by the compiler (large)

- **Structure of arrays**: a list of a class whose items have no identity outside it, and whose hot loops read a
  subset of fields, stores each field (or each group of fields always read together) in its own array.
- **Splitting classes**: fields read together go into one part, fields read rarely into another (hot and cold);
  the class stays one class in the source.
- **Splitting components and systems**: a component whose fields are read by disjoint systems becomes several
  columns; a system whose body has independent halves becomes several systems that stage 3 and D505 run at once.
- Retires the engine's hand column headers, parallel arrays per field and swap-remove code.
- Measured for programs that are not games, with the rules a compiler could prove and a build order:
  [proposals/data_oriented_layout.md](proposals/data_oriented_layout.md) (2026-10-08).

### Stage 5: memory placed by the compiler (medium, D501's order)

Frame arenas, ring buffers, deferred freeing, as in the proposal. Built where stage 1 shows allocation costs time
(the render and UI paths), not on `stress`, which does not allocate.

### Stage 6: waiting overlapped by the compiler (medium)

The compiler already knows which functions wait. Waiting calls in a frame loop whose results are not needed until
later start early and are collected where first read, as the hidden async/await state machines already do inside
a `Concurrent`. Retires the engine's 6 `Concurrent` uses and its "8 a frame" drain.

### Stage 7: the `Optimizer` namespace (small, when the engine needs none of it)

When the naive engine matches the hand one, the threading and memory classes move to `Optimizer` (D506, "later,
not now"), and the engine and the game use none of them.

### Later: the own backend

The pair catalogue is the specification the own backend (D397) rebuilds from. Its last column says, for each
pair, what the backend can do that C cannot express (no-alias facts per field, proven bounds, layout freedom).

## Answered (2026-10-07, D508)

- **Q1, docs**: the moron philosophy sits in the docs README's philosophy and on a new page,
  `docs/write_it_plainly.md`, second in the reading order. Optimisations and their proofs stay on
  `optimizations.md` (each section names its proof) and `proofs.md`.
- **Q2, run-time choices**: as much as possible at compile time, because Spite is general purpose (operating
  systems to web pages) and a runtime would hurt embedded and WebAssembly targets. A branch between forms
  compiled in advance remains the only run-time choice allowed, as proposed by Claude and unconfirmed beyond that.
- **Q3, the engine**: the naive engine lives on its own branch of the engine package (`naive`) until it is as
  fast, then replaces its main branch. This supersedes the worktree folder of stage 1.
- **The game** is only a benchmark until real games are built: it should look like the original so the comparison
  with the engine the original was made in is honest. It is not changed for features.
- **Scope**: concurrent and parallel code, CPU alignment and every optimisation technique there is. Theories go
  into [optimization_research.md](optimization_research.md), where no idea is a bad one: the question is how far
  moron-written code can beat expert hand-tailored code.
- Language changes are pushed to the remote as they land.

## Still open

Nothing. D505's limits and the run-time branch were decided by Claude under D509 (D510) and wait in
`mortaros_missing_decisions.md` under "To confirm".
