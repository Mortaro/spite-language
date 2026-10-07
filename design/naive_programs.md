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

## How every step is judged

1. **The naive form is the reference.** A step starts by writing the naive version of a piece of the engine (on the engine
   package's `naive` branch) and measuring it against the
   hand version with `--optimized`.
2. **A gap names a pair.** Each place the naive form is slower is one missing optimisation and the proof it needs.
   It goes into the pair catalogue before it is built.
3. **Kept only if at least as fast** as the hand form it replaces (D501, D214), on the engine's benchmarks:
   `stress`, `physics_bench`, `navigation_bench`, `render_bench`, `replication_bench`, `animation_bench`,
   `props_bench`, and `benchmarks/versus_c` in this repository.
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
  and a foreign struct declaration); queued as library work.
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
| `benchmarks/versus_c` (all five) | | the same C, byte for byte: no class there is both held in a list and made as an object |

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
| `benchmarks/versus_c` (all five) | | the same C, but for the numbers of its temporaries |

The profile after B3, inclusive, both systems summed: writing rows back 36%, matching 23% (`find_row` 17%), filling
rows 19%, the candidate list 13%, `choose` 9%. The counts left are in reads that are not tests: `choose` reads
`candidates[index]` twice per entity, each a counted `List<Integer>` (4% of the tick in its count and release), the
matcher passes its `rows` attribute to `match_into` (counted, 4%), `stamp_written` passes `headers[index]` to
`changes.stamp_written` (3%), and the `header` locals of `find_row`, `fill_attribute` and `store_attribute` are
counted reads (B1).

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
