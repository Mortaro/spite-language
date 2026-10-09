# Optimisation research: theories to investigate

**Not user facing.** This is the open notebook for the question Mortaro set on 2026-10-07 (D508): *how far can a
language guided by a moron beat expert hand-tailored code?* Every idea is welcome here, including ones that look
impractical; the project is for the sake of science. Nothing in this file is decided, and nothing here is
promised to a user: an idea leaves this file when it becomes a pair in
[naive_programs_pairs.md](naive_programs_pairs.md) (planned work) and then a section of
[docs/optimizations.md](../docs/optimizations.md) and [docs/proofs.md](../docs/proofs.md) (built).

How to use it, for agents sent to investigate:

- Pick an idea, write what you found under it (measurements, a prototype branch, why it fails), with the date.
  Do not delete an idea that failed: write why it failed, so nobody retries it blind.
- Every idea must stay inside the two constraints that are not up for research: **nothing silent** (an
  optimisation never changes what a program prints, computes or crashes on, D244) and **zero runtime** (nothing
  ships beside the program; a run-time choice is at most a branch between forms compiled in advance, D147, D176,
  D508). An idea that needs a runtime goes under "Outside the constraints" with what it would need.
- Measure against the hand-tailored form, on the engine package's benchmarks and the cases of `benchmarks/` (each against naive and expert C), in an
  `--optimized` build. Write the numbers here.

Each idea: **Idea**, **Proof it needs**, **Why it might win**, **Risks**, **Status**.

## Why a compiler can beat an expert at all

The argument the whole project rests on, to be tested:

1. **The compiler sees everything at once.** An expert tunes one system with partial knowledge of the rest; the
   compiler knows every caller, every field read, every alias, after tree shaking and per-class specialisation.
2. **Spite forbids what C must assume.** No pointer arithmetic in programs, no aliasing that the compiler did not
   create, no hidden mutation through globals the analysis cannot see. Facts C compilers have to guess or give up
   on are simply true here, and an own backend can use them where C cannot even say them.
3. **The compiler re-decides every build.** A hand layout is frozen the day it is written. The compiler picks
   again whenever the program changes, and per target.
4. **The expert's tricks are mechanical.** Structure of arrays, banding, hot and cold splitting, arena scratch,
   lock elision: each is a rewrite with a precondition. A precondition is a proof.

The limit is where a hand optimisation depends on knowledge no analysis can recover (intent about future data,
domain facts). Finding where that line is, is the research.

## Representation per use, not per class (D520)

Mortaro, 2026-10-07: optimising "by class" is too coarse. A class is meaning; its storage is chosen per use.

- **Class splitting by access sets.** Partition a class's fields by the loops that read and write them; each
  partition becomes its own storage (and its own pool, its own count discipline). A field set written by one
  system and read by another may become two columns even inside one "component".
- **Different shapes of one list in different places.** A `List<Monster>` walked field by field in one phase and
  read whole in another: keep both forms, or convert at the phase boundary when the conversion is cheaper than
  the slow form. The cost model decides; the conversion is a compiled loop, never a runtime.
- **Per-site variants of a class.** C5 (plain counts) and M6 (pools) are decided per class today. A class used by
  threaded work in one place and only on one thread in another could be two classes after compilation: objects
  made where no thread reaches them get plain counts and a pool; the others do not. Requires knowing, per object
  creation site, where the object can flow.
- **Objects that only one owner holds merged into it** (inline), per owner field, not per class.
- **Procedural programs restructured into passes.** A record-by-record script (parsing a file, transforming each
  record, writing it) can become a pipeline of column passes, an ECS in all but name, when the data per record is
  uniform and the steps are independent. The program never mentions entities.
- **Specialising generic machinery.** A generic loop driven by lists that never change after setup (an ECS
  runner's headers, kinds and keys) is partially evaluated per configuration into direct code. This is the
  biggest measured gap in the naive engine (about 70% of its stress tick). Found while building its first step:
  most of that gap is not the configuration but the runner around it (folding the matcher's kinds and counts by
  hand bought 15%); the configuration's domain, the values each list can hold, is a whole-program fact that needs
  no "setup" phase at all, and is enough to fold tests against values a list never holds.

Research questions: how to represent "the same value in two shapes" in the compiler's IR; when a split pays
(access counts, cache-line footprints, write sharing); how to prove the conversion points preserve every observable
result (identity, `==`, reflection, `--debug-memory` counts).

## Learning from test runs (D534)

A profiling build records what only a run can know, and the next build reads it to pick among proven-equivalent
forms. Candidates: list sizes (frame array or heap, ring capacity, band count for T1), branch frequencies (hot and
cold layout, which configuration S1 specialises first), dictionary key sets and densities (D533: array, perfect
hash, sorted table, folded), how often a phase converts between two shapes of one list (S3), which waiting calls
overlap in practice (W1). Questions: how to keep the profile small and stable under code changes (key choices by
source position and name, drop entries that no longer match); how a test suite or a benchmark doubles as the
profiling run; how to show in `--optimization-report` what a profile changed.

## Data-oriented design for any program (Mortaro, 2026-10-08)

Mortaro: be very aware of data-driven design. A point of ECS is to use the CPU cache instead of RAM; a lot of code
that is not meant to be ECS can be compiled into data-driven loops and SIMD, and the compiler must prove when each
form is best, for general-purpose programs, before the own backend, which must know CPUs better.

- **The forms to choose between, per use (D520).** Array of objects (today), array of structures inline, structure
  of arrays, hybrid (AoSoA: blocks of 4, 8 or 16 of each field, matching vector lanes), hot and cold split, and
  index lists over a column (a filtered subset kept as indices instead of copies).
- **What decides it, and what must be proven.** The access pattern of every loop over the data (which fields,
  in what order, sequential or gathered), the working set against the cache sizes of the target (L1, L2, L3 per
  core, line size), the write pattern (two threads writing one line), how often a whole object is needed at once
  (passing it, printing it, `==`), and the conversion cost at phase boundaries (S3). The proofs are the ones S2
  needs (no whole-object use crosses the split) plus a cost model of memory traffic: bytes touched per pass for
  each form, from the loops' field sets and the counts (known, proven, or profiled, D534).
- **SIMD.** After SoA, a loop over plain fields with no cross-lane dependence becomes vector code: lanes from the
  target (SSE, AVX2, AVX-512, NEON), masks for an `if` in the body, gathers where an index list is read, a
  remainder loop. The C backend can lean on the C compiler's vectoriser given `restrict` and aligned columns; the
  own backend must do it itself, so the proof (no alias, no dependence, lane-safe operations, the decimal rule)
  has to be explicit in the compiler, not left to the C compiler.
- **Not only games.** Parsing (a column of token kinds and a column of offsets), text processing (byte columns),
  databases and reports (columns of records), image and audio (planes and channels), simulations, compilers
  themselves (the Spite compiler's own lists of nodes are a candidate).
- **Measure it.** Every pair here gets a case in `benchmarks/` that is not a game (D521): naive Spite, naive C
  written with objects, expert C written data-oriented by hand.
- **Measured, 2026-10-08:** [proposals/data_oriented_layout.md](proposals/data_oriented_layout.md). A sweep of eight
  layouts by seven loops by eight sizes on a Ryzen 9 5950X, and five non-game cases (a report, a tokeniser, an
  image filter, a spreadsheet recalculation, a sort of records) each with inline-struct and column hand forms.
  Columns never lost an in-order pass (10 times inline structs and 14 times Spite's pool for one field of eight in
  memory); inline structs win only random reads of two or more fields (1.8 times); a reordered list walked through
  its pool costs 3 to 11 times a walk in memory order; overflow checks cost a vectorisable sum 4 to 7 times. Eight
  rules, each with its proof and fallback, mapped onto the compiler's existing facts, and a build order whose first
  step is dropping the overflow checks a sum provably cannot need.

## The GPU, without the moron ever thinking about it (Mortaro, 2026-10-08)

Mortaro: research using the GPU where it makes sense, with no program ever mentioning it; a possible "compile
where it runs" mode that, instead of parallelising on the CPU and uploading results, does most of the work on the
GPU. Not the only mode and not the default; research that could unlock large speed-ups, not only for games.

- **Which loops qualify.** The same proof as T1 (independent passes, self-indexed writes, recognised reductions),
  plus what a GPU needs: no recursion, no allocation inside the body, no IO, no calls the GPU cannot run, bounded
  control flow, data that fits plain arrays (after L1/S2 the columns already are). A loop over 200,000 entities
  that only reads and writes its own row is the textbook case; so are image filters, physics broad phases,
  pathfinding grids, audio mixing, decoding, sorting, matrix work, simulations in a script.
- **Where the data lives.** The cost is moving data, not computing. A loop on the GPU pays only if its columns
  stay there across passes and frames. "Compile where it runs" would place whole columns in GPU memory and run
  every qualifying pass there, moving data back only where the CPU reads it (IO, printing, a non-qualifying
  pass). The compiler sees every reader, so it can decide placement per column and per phase (D520).
- **What it compiles to.** A compute kernel per qualifying loop, emitted while compiling (SPIR-V for Vulkan,
  or C for a portable fallback), with the CPU form kept. Zero runtime means no shader compiler or scheduler
  shipped: kernels are compiled ahead, and dispatch is generated code. The device API (Vulkan compute) is a
  foreign library like any other.
- **Same result.** Integer work is exact. Decimal work already allows reassociation (a decimal's last bits are
  never a promise), but GPU maths functions may differ from the C library's in their last bits too; that needs a
  rule. Crashes inside a kernel (a proven read cannot fail; an overflow can) must still be reported with their
  site (D244).
- **When it pays.** A cost model like T6 with transfer cost: size times passes, against bytes moved. A profile
  (D534) can settle it for sizes only the run knows.
- **Questions.** Which targets have a usable GPU and how a build chooses (a mode flag, a target, a profile); how
  to test kernels in `check.sh` without a GPU (a CPU executor of the same kernel); whether the engine's renderer
  and this mode share one device; what the moron sees in `--optimization-report`.

## Archetypes, chosen by the compiler (Mortaro, 2026-10-09)

Some entity systems ask the programmer to mark a component as "sparse" or "table" so the engine can group entities
by which components they have (archetypes: one table per set of components, rows contiguous). Mortaro: no moron
should ever think of that, and the optimisation must not be limited to games; it should come out of the box.

The general shape is not entities at all: any collection of objects whose items differ in which optional parts they
have (an attribute that is `null` for some items, a variant held as a union, a part added and removed over time).
Questions to research:

- **Grouping by shape.** When does it pay to keep the items of one list in several tables, one per set of present
  parts, so a loop that needs parts A and B walks only the tables that have both, contiguously? What must be proven
  (no order of the whole list is observed, or the order is kept by an index; identity and `==` still answer the
  same), and what does moving an item between tables cost when a part is added or removed?
- **Table or sparse, per part, decided by the compiler.** A part added and removed often is cheaper kept apart
  (sparse, a map from item to part); a part that stays is cheaper in the table. The compiler sees every place a
  part is added, removed and read, and how often in loops; where only the run knows the rate, a profile (D534)
  decides. The moron writes neither word.
- **Queries as loops.** A loop with a test of which parts an item has (`if item.velocity and item.position`) is a
  query; with archetypes it becomes a walk over the matching tables with no test.
- **Relation to what exists.** Per-use representation (D520), columns (L1), class splitting (S2), the
  data-oriented study's rules, and the naive engine's own columns and matcher, which do this by hand today.


## Threads and parallelism

- **Independent loops in bands.** Idea: a loop whose passes write only their own item runs on the pool. Proof:
  per-pass write set disjoint by index; reads not written by any pass. Status: planned (T1).
- **Reductions recognised.** Sum, min, max, count, any, all, histogram (per band, merged). Floating point: only
  where reassociation is proven harmless, or merged in fixed band order so the result is the same every run.
- **Automatic task graphs from effects.** Whole-program read and write sets per field make every system call a
  node; edges are conflicts. The compiler emits a static schedule (a fixed order of joins), not a scheduler.
  Question: how close does a static schedule get to a work-stealing one on uneven loads?
- **Splitting a system into independent halves** that then overlap (T5).
- **Speculative parallelism without a runtime**: run a loop in bands assuming no conflict, with a compiled check
  of a cheap condition first (for example, all indices distinct). One branch, two compiled forms.
- **False sharing avoided by layout**: per-band outputs padded to the cache line, decided from the target's line
  size.
- **Grain from a compile-time cost model**: count instructions and memory touches of a body, multiply by a known
  or branch-tested count. Calibrate the model per target with the benchmarks, at compile time only.
- **Pipeline parallelism**: a chain `a -> b -> c` over a stream split into stages on different cores, when each
  stage only reads what the previous wrote.
- **Lock elision by proof**: already partly built (singleton locks). Push further: a singleton reached only from
  one thread's call graph has no lock at all; a lock held across a whole counted loop; locks replaced by
  ownership transfer when a value moves between bands.
- **Atomics chosen per access**: relaxed where only a count is needed, acquire and release only at the joins that
  publish.

## Waiting and concurrency

- **Start early, collect late**: a waiting call whose answer is first read later starts at the call and is
  collected at the read (W1).
- **Batched IO**: several reads of independent files submitted together through the system's batch interface
  (completion ports, `io_uring`) chosen per target.
- **Waits folded into state machines** (already how hidden waiting works): investigate fusing several machines
  into one so a frame loop has one switch, not many.

## Layout and the CPU

- **Structure of arrays, hybrid layouts, hot and cold splitting** (L1 to L3).
- **Field order by access**: fields read together placed in the same cache line; fields written by different
  threads on different lines.
- **Alignment per target**: struct and array alignment to the cache line or the vector width where loops read
  them; packed (no padding) where the class is only stored, never looped over.
- **Narrowing representations**: a field proven to stay in `0..255` stored as a byte; a Boolean list as bits;
  an enum in the fewest bits; an index proven below 2^32 stored in 32 bits.
- **Pointer compression**: references inside one list stored as 32-bit indices into it.
- **Tagged and niche representations**: a `T?` whose `T` never uses some bit pattern stores "nothing" there with
  no flag.
- **Removal without holes**: swap-remove where order is never read (L5), tombstones with compaction scheduled at a
  compile-time-chosen point.
- **SIMD** over the arrays of a structure of arrays (L7), including masked passes for `if` inside a loop.
- **Prefetch hints** for loops that walk an index list into another list (gathers).
- **Loop tiling** for nested loops over two lists, sized to the target's cache.
- **Branch layout**: cold paths (crashes, built) and rare branches moved out of the hot code; branchless selects
  where both sides are cheap and proven safe to evaluate.

## Memory

- **Frame arenas, rings, deferred freeing** (M1 to M3, the automatic memory proposal).
- **Reference counting removed by ownership proof**: a value with exactly one owner at each point needs no count;
  extend the lent-argument proofs until counts appear only where sharing is real.
- **Objects merged into their owner**: a field that always holds a fresh object never shared becomes inline.
- **Allocation sinking**: an object made in a loop and dropped in the same pass reuses one slot.
- **A copy that copies only what is written** (after M7, D555): where a deep copy's window writes one path (the copy's
  `lines`, say) and only reads the rest, copy the path to what is written and share everything else, counted once
  more. Needs M7's window walk to say which attributes are written and the copy function per site to stop at the
  shared ones. Not built: M7 shares all or nothing. Also not built: a window that compares two results of calls
  (`copies.sum_id() + copies.count()`), since the walk types a call's result only for a list's `sum_`, `count_`,
  `any_` and `all_`.
- **Whole-program static memory**: a program whose allocations are all bounded (common in firmware) gets every
  object a fixed address; no allocator linked at all. Valuable for embedded and WebAssembly.

## Code generation

- **Aliasing facts to C** (`restrict`, C1) now; per-field no-alias facts in the own backend later.
- **Specialisation by value**: a function called with a constant argument compiled for that constant; a loop
  bound known per call site unrolled there.
- **Profile-guided decisions without a runtime**: run the benchmarks at build time (opt in), record branch
  frequencies, feed them into the next compile. The shipped program carries nothing.
- **Superoptimisation of small hot functions**: search for the shortest instruction sequence equivalent to a tiny
  function, checked by an equivalence proof. Research only.
- **Equality saturation** for arithmetic rewrites in the own backend.
- **Inlining by cost model** rather than size alone, informed by the effect summaries.

## Whole-program and compile-time evaluation

- **Run what can run while compiling**: any function with constant inputs and no IO is evaluated at compile time
  and replaced by its answer (tables, parsed constants, generated meshes).
- **Data baked into the binary**: configuration and assets known at build time folded into constant data.
- **Dead state removal**: a field written but never read, removed with its writes.

## Targets with no room for a runtime

Spite is general purpose (D508): operating systems, firmware, WebAssembly pages, servers, games. Research per
target: what the smallest program costs in bytes; what thread, waiting and memory forms exist there (no threads on
some chips, one thread on the web unless workers are used); how the same plain program picks a different form per
target from `target_operating_system` and the target's facts, all at compile time.

## Findings, 2026-10-07 (read-only research by free models; unverified until measured)

Each finding below came from a model reading the code without building or running it. File and line citations
were spot-checked, not proven. A Claude agent measures before any becomes a pair.

**R3, `number_dictionary` against its C twin** (the README row says 1.96x; the recent run 2.9x). Differences by
likely cost:
1. The program reads `scores[key]` twice (test, then read); C does one lookup. Pair: reuse a dictionary read whose
   dictionary and key are unchanged in the block (the read writes nothing; nothing between writes either).
2. A probe reads three arrays (slot table, `entry_keys`, `entry_values`, each through a checked, nullable `get_at`);
   C reads one interleaved slot. Pair: a number-keyed dictionary stored as one interleaved slot table (layout is
   unobservable when only `keys()`, `values()`, `count()` are read).
3. A nullable round trip inside the probe (`entry_keys[stored - 1] == key` compares `T?` with `T`) where the slot is
   proven in range.
4. Load factor 0.5 and a table four times the count: about twice C's memory. Grow at 0.75 (capacity is never
   observable).
5. Growth zeroes slot by slot; C uses one zeroed allocation.
6. Insert probes twice (exists, then place) and hashes twice.
7. Checked arithmetic on loop counters with a provable range.
8. Bounds checks per probe step, provable from the table's invariant.
9. A hash fragment is written and read for number keys although the docs say number keys keep none: check it.
Reference counts are not a cost here (`Integer` keys and values are not counted).

**R4, `vector_maths` against its C twin.** The loop is the frame rule's own example, and `Vector3<Float>` holds only
numbers, so its temporaries should live in the frame. Two suspects:
1. Every call in the loop is to a library function, and the uncounted `___held_` argument form applies only to the
   program's own functions, so each call counts its receiver and arguments up and down (about 20 count operations
   an iteration). Pair: pass a frame-owned object uncounted to a library callee proven not to keep it.
2. A failed caller-slot claim falls back to a heap object copied into the frame slot and released, with no line
   in `--optimization-report` (the status page admits the assignment case is not reported). That is a silent
   fallback (D244): pooling every class made this benchmark 1.75x faster, which says something still allocates.
   Pair: claim the slot for member callees too, and report every fallback.

**R2, splitting a class by access sets (S2).** Reusable facts: per-function member reads and writes
(`function_effects`, `call_effects`), field-level reach sets of the D505 proof, thread reachability (C5), pool
eligibility (M6), escape facts (frame objects, borrows, lent copies), and `field_place`, through which every field
read already goes (it already redirects for hot-reload moves). Whole-object uses and what each needs: identity `==`
(keep one header, parts behind it), custom `equals` / `to_string` / `to_debug` / serializers (a compiled join at the
call), `copy` and `deep_copy` (per-part copy), `Weak` (refuse while a `Weak` of the class exists, unless the box keys
on the header), run-time reflection (join at that point), crash reports (follow the part addresses),
`--debug-memory` counts (one entry for the parts, or refuse in those builds), foreign calls (class instances never
cross). Proposed IR: one header and count per logical object, `Class__Part0` and `Class__Part1` storages, a fat join
used only at conversion points, `field_place` choosing the part by field; S4 picks the header kind per creation site.

**R5, record loops into column passes and back (S5).** Splitting is allowed when records are uniform and not
reordered during the passes, each pass writes only its own record and reads nothing another pass writes, and no
step prints, writes a file or can crash in an order the split would change (a decode crash on record 5 must still
come before a transform crash on record 3 if the record loop reached it first); column buffers are compiled loops,
and an unknown record count is a branch between compiled forms. Fusing (the existing chain fusion, L8) is allowed
when the intermediate is never named and per-element order is kept. A hand-written `while` that only does what a
template says is already refused, so record loops tend to arrive as chains. Open: whether the effect analysis is
per field enough to prove record independence for a hand-written loop, and whether a written-order merge for file
writes exists (only T3 plans one).

**R6, overflow checks proven unneeded.** Today only `counter + 1` / `counter - 1` under a `<` / `>` narrowing and
constant operands skip the check (`proven_to_fit`, `checked_overflow` in generator.spite); index bounds are tracked
as symbolic paths in `Scope`, not intervals. Proposal: an interval `[lo, hi]` per local seeded from types and
literals, propagated through `+ - *` with intersection on the true branch of a comparison and widening at loop
joins; `while index < bound` with a known bound gives `[0, bound - 1]`, so `index * 7` under 500,000 fits an
`Integer`. Plugs into `proven_to_fit` after the existing test; every kept check still reported. Open: the cost of
cloning intervals per block, one table of every number type's limits.

**Built 2026-10-08** (Claude, branch `proven-ranges`): `range_refusal` beside `proven_to_fit`, a range per local in
`Scope` (`set_range`, `assign_range`, which widens every enclosing scope up to the declaring one, so a branch never
needs cloning), `enter_loop_ranges` before a loop's condition (counters, any other local widened with the loop's
own locals at their type's range, passes from a counter stepped once a pass, totals of one term a pass), and every
kept check in `--optimization-report`. The compiler's own C went from 1 513 to 1 263 checks. The cases' hot loops
did not move: `arithmetic_is_checked_in_every_build` (20.7 to 19.9 ms, noise), `a_proven_read_tests_only_its_bounds`
(33.9 to 34.4), `report_over_records` (315 to 324) and `smaller_ones` (26.9 to 26.7), since what they add up is an
`Integer` list item or an attribute, which no local range bounds: the next steps are attribute ranges (every write
in the program, like `item_values.spite`) and the speculate-and-replay loop below.

Measured 2026-10-08 (Claude, for `benchmarks/arithmetic_is_checked_in_every_build`, whose sum an interval cannot
bound): **speculate, then replay.** A counted loop whose body only reads plain lists and assigns plain locals can
run once with every check turned into a flag (`wrapped |= x > INT32_MAX / 3 || x < INT32_MIN / 3`) and each
`acc = acc + term` summed with wrapping in 32-bit lanes, beside the largest and smallest term; after the loop,
`acc + n * largest <= INT32_MAX` and `acc + n * smallest >= INT32_MIN` prove no prefix sum overflowed, so the wrapped
sum is the true one. When the flag is set or the bound fails, the locals are put back and the loop runs again with
its checks, halting at the exact operation with the exact operands: nothing changes but speed. In plain C on this
machine (clang -O3, no `-march`, 400 rounds of 100 000): checked 18.2 ms, speculated 10.3 ms (vectorised 4 wide),
wrapping C 2.7 ms. The largest and smallest cost most under SSE2 (no `pmaxsd`); with `-march=native` the speculated
form was within 3 times of plain C in an earlier int64 variant. Summing in int64 lanes instead was slower than the
checked loop under SSE2 (2 lanes, no 64-bit multiply). Not built: the body generator would need a speculating mode
for every checked operation, and the loop shape is the counted loop's (`generate_counted_loop`).

**R7, independent loop passes on several cores (T1).** D505's facts are keyed by class and field, so every pass of
one loop collides with itself. Add a key for writes whose receiver is the loop's own item (`self_indexed`); then a
pass is independent when each write is self-indexed, a recognised reduction (T2) or a per-band scratch (T8), and
`io`, `raw` or `unknown` refuse. The emitted form already exists: `parallel_each_` writes a `piece(first, end)` loop
run through `ThreadPool.run_pieces`. Reductions exact in any order: integer sum, count, min, max, and, or; decimal
min, max, count. (The model said a banded decimal sum is not allowed; the docs say otherwise: a decimal's last bits
are never a promise, and loops already reassociate decimal sums unless the loop compares decimals with `==`. A
banded decimal sum is therefore allowed under the same rule.) Cost model: weight the body (loop 8, call its own
weight, count 2, load/store 1, check 1), split when `weight * count` passes a threshold measured at build time
(light bodies lost 2.4x, a 200-step body won 24x); a count known while compiling folds the branch. Warning: today
`parallel_each_` sets threads for the whole program, which makes every count atomic unless C5 keeps it local.

**R8, small functions not inlined across C units (C4).** Folded functions are called through a `static` pointer
that the unit splitter turns into an external, writable global defined only in unit 0, so link-time optimisation
sees an indirect call. Units are cut by a hash of the function name, so nearly every call crosses units. Fixes:
(A) emit folded pointers as `static const` in the shared header, which any `-O1` build turns into a direct call;
(B) a `static inline` trampoline instead of a pointer (not in hot-reload or REPL builds); (C) cut units by call-graph
affinity using the folder's call edges, with stable tie-breaks for the object cache; (D) a ThinLTO cache directory
and skipping the link when no object changed.

**R9, two count disciplines per class by creation site (S4).** One struct type (layout and `class_id` stay one), two
count pairs `X___retain`/`X___release` and `X___retain_plain`/`X___release_plain`; each count site picks from the
origin set of its value; a value whose origins mix counts atomically (correct for both, no tag, no branch). A list
holding both kinds counts atomically. Pools per site only when every creation site of the class is pooled (one free
path), otherwise a header bit; never promote by copying (changes identity). Nodes: creation sites (`___allocate`
calls, boxes, frames); edges: stores, arguments, returns, reads, captures; anything leaving the analysis (foreign,
reflection, `type`, `Weak`) keeps all-atomic and is reported.

**R10, a stage's systems as a row (T4b) and loop passes in bands (T1).** The naive engine runs a stage as
`stage.each(run_runner)` over a list built once in `App()` and only appended to. D523's list-values proof sees
that, but each element is a fresh generic instance or a local, so T4b also needs the count, the order and each
element's class at its append site (unconditional appends at the top of a function with finitely many call sites);
then the loop becomes bindings plus a row of calls, and D505 settles it. T1 needs a self-indexed key in the overlap
facts; the band loop already exists for `parallel_each_`. Handed to the agent building T4b and T1.

**R11, waiting inside a frame loop (W1, W2).** The engine's asset loading keeps `Parallel` and polls
`finished_value()` each tick, because a plain blocking load would make every system that reaches it a waiting
system, which may not write inline components. W1's "first needed" is the first implicit join (store, operand,
text hole, condition, narrowing, end of scope); across frames the started wait must outlive the system's return,
so it lives in a slot the compiler makes, keyed like today's `loads` and `load_slots`, collected on a later tick.
The resumable-copy machinery for `Concurrent` and the helper thread for blocking IO already do the waiting. Open:
the lifetime of a wait never needed, which line a failure a tick later reports, a bound on waits in flight, and
whether the waiting-system rule is lifted (a decision).

**R12, results built into the caller's slot.** Already fixed (`d083a22b`) by the time the model read it:
`normalized()` and `Matrix4.multiply` now fill a fresh frame slot and copy its fields into the target. The next
step is building straight into the target itself, which needs a proof that the callee does not read the target
(it does for `accumulated * step_matrix`: the product would overwrite its own input, and the result pointer is
`restrict`).

**R13, the GPU (two models).** Qualifying loops: stress's `Move` and `Regenerate` (self-indexed, 200,000 rows,
about 5 to 10 MB a tick, under a millisecond resident), the navigation grid's 4-million-cell passes (about 21 MB,
two orders of magnitude over its bake), plain number loops in `benchmarks/`. Not qualifying as written: physics
(shared buckets, scratch, commands), A* (writes neighbours), sorting (recursion), text and dictionaries. The
engine already compiles GLSL to SPIR-V ahead of time and dispatches compute through a plain foreign library, so a
kernel needs no shipped shader compiler, and a build with no qualifying loop links no Vulkan. Cost model: T6 plus
transfer; the threshold is passes over resident data, not rows. Crashes: each dispatch writes a status slot (site
and operands) that generated code reads after the pass. Smallest experiment: hand-write the two kernels of
`benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked` (resident across 2000 rounds)
and compare times and the decimal sum's bits with the CPU; then stress's two systems. Open: the Vulkan binding is
headerless; whether compute shares the renderer's device; the decimal rule for device maths functions. One model
flagged a discrepancy to check: navigation's `next_stamp` zeroes 16.8 MB per query, which does not fit the measured
11,905 queries a second.

**R13 experiment, the first GPU run (2026-10-08).** Research only, nothing built into the compiler. The work of
`benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked`, written by hand in C over
Vulkan compute: `vulkan-1.dll` loaded at run time, four GLSL kernels compiled ahead to SPIR-V with the Vulkan SDK's
`glslangValidator -V --target-env vulkan1.3` (the engine package's shader recipe), both lists in device-local memory
for all rounds. `from` is uploaded once; each round is the scale pass (one item per thread), then the sum pass (1,024
groups of 256 threads each add a strided slice and reduce it in shared memory to one partial sum, then one group adds
the 1,024 partials into that round's slot); the 2,000 sums are read back once and turned into quarters on the CPU.
Variants: a fused pass (scale and add up in one kernel, like `expert.c`); one submission and fence wait per round
(what a CPU that needs each round's sum before the next would do); everything recorded into one command buffer and
submitted once; and the data moved every round (`from` uploaded before the passes and `into` read back after them).

How measured: Windows 11, AMD Ryzen 9 5950X (16 cores, 32 logical processors), NVIDIA GeForce RTX 3090 (24 GB,
discrete, driver 610.88, Vulkan 1.4.341), clang 19.1.5. CPU forms: Spite `naive/` built `--optimized` by
`.spite/spite_development.exe`, `naive.c` and `expert.c` at `-O2` and at `-O3 -march=native`, each program timing
its own rounds as `benchmarks/run.sh` does, best of five runs (three at 10,000,000 items). GPU: the same clock from
the first upload to the last read back, device setup left out as the CPU forms leave out filling the lists, best of
five repeats in one process (three at 1,000,000 and 10,000,000); GPU busy time from timestamp queries. Sizes change
only the item count; rounds stay 2,000. Two other sessions were compiling the whole time, so the CPU numbers are
slower than the case's own timings (Spite 56 ms here, 39.5 ms there) and ratios are good to about 20%. The GPU was
warmed by an earlier run; the table of fixed costs shows what a cold one costs.

At the case's size, 100,000 items (400 KB a list):

| form | total µs | µs per round | total in quarters |
|---|---|---|---|
| Spite `naive/`, `--optimized` | 56,023 | 28.0 | 3,899,940,000 |
| `naive.c`, `-O2` | 211,509 | 105.8 | 3,899,940,000 |
| `naive.c`, `-O3 -march=native` | 194,029 | 97.0 | 3,899,940,000 |
| `expert.c`, `-O2` | 20,922 | 10.5 | 3,899,940,000 |
| `expert.c`, `-O3 -march=native` | 20,250 | 10.1 | 3,899,940,000 |
| GPU, two passes, one submission | 22,697 | 11.4 | 3,899,940,000 |
| GPU, fused pass, one submission | 17,872 | 8.9 | 3,899,940,000 |
| GPU, two passes, a wait per round | 149,117 | 74.6 | 3,899,940,000 |
| GPU, fused pass, a wait per round | 143,635 | 71.8 | 3,899,940,000 |
| GPU, two passes, data moved every round | 237,104 | 118.6 | 3,899,940,000 |

Microseconds per round across sizes ("not run" where that form was not timed at that size):

| items | Spite `--optimized` | `naive.c -O2` | `expert.c -O3 -march=native` | GPU two passes, one submission | GPU fused, one submission | GPU two passes, a wait per round | GPU data moved every round |
|---|---|---|---|---|---|---|---|
| 10,000 | 1.6 | not run | 0.8 | 10.9 | 8.3 | 71.5 | not run |
| 30,000 | 4.3 | not run | 2.5 | 11.4 | 8.4 | 72.1 | not run |
| 100,000 | 28.0 | 105.8 | 10.1 | 11.4 | 8.9 | 74.6 | 118.6 |
| 300,000 | 48.1 | not run | 25.3 | 12.9 | 9.3 | 72.9 | not run |
| 1,000,000 | 296.7 | 1,025.7 | 104.7 | 22.7 | 15.8 | 86.5 | 472.5 |
| 10,000,000 | 7,862.7 | 11,091.9 | 1,651.6 | 177.6 | 124.1 | 242.9 | 3,954.8 |

Fixed costs of the GPU, warm unless said:

| what | cost |
|---|---|
| setup: load `vulkan-1.dll`, instance, device, buffers, four pipelines | 218 to 241 ms (723 and 921 ms on the first two runs of the night) |
| empty submission and fence wait | 47 to 56 µs |
| one tiny dispatch, submission and fence wait | 56 to 68 µs |
| GPU busy per round, two passes (3 dispatches, 3 barriers), 10,000 items | 8.0 µs |
| GPU busy per round, fused (2 dispatches, 2 barriers), 10,000 items | 6.4 µs |
| GPU busy per round, fused, 10,000,000 items (80 MB read and written) | 121 µs, about 660 GB/s |
| a cold device, the first run: a wait per round | 4,162 µs a round, empty submission 322 µs |
| moving both lists every round, 10,000,000 items (80 MB) | about 3.8 ms, about 21 GB/s |

Spinning on the fence instead of a blocking wait changed nothing once the device was warm; the 4 ms rounds of the
first run were the device's low power state, which a wait per round kept falling back into.

The decimal sum. The case's promise that "a sum is exact in any order" holds only at its own size: at 10x and
100x the sums pass 2^24 quarters and every order rounds differently. The exact total is computed in integers (every
item is a whole number of quarters); "correctly rounded" is the exact sum of each round rounded once to the nearest
`Float`:

| items | form | total in quarters | minus the exact total |
|---|---|---|---|
| 1,000,000 | exact, and correctly rounded | 38,999,964,000 | 0 |
| 1,000,000 | GPU, every variant | 38,999,964,000 | 0 |
| 1,000,000 | Spite `--optimized` | 38,999,964,000 | 0 |
| 1,000,000 | `expert.c`, both flags | 38,999,964,000 | 0 |
| 1,000,000 | `naive.c`, both flags | 39,005,970,000 | +6,006,000 |
| 10,000,000 | exact | 389,999,928,000 | 0 |
| 10,000,000 | correctly rounded | 389,999,936,000 | +8,000 |
| 10,000,000 | GPU, every variant | 389,999,936,000 | +8,000 |
| 10,000,000 | Spite `--optimized` | 390,047,976,000 | +48,048,000 |
| 10,000,000 | `expert.c`, both flags | 390,047,984,000 | +48,056,000 |
| 10,000,000 | `naive.c`, both flags | 387,810,048,000 | -2,189,880,000 |

Everything matches bit for bit at the case's size and at 300,000 and below. Above it, the difference is the order of
summation and nothing else: the scaled items are small whole quarters, exact under any rounding, so fused
multiply-add (which glslang leaves to the driver, emitting a separate multiply and add) cannot show, and Vulkan
requires `Float` addition and multiplication to be correctly rounded. The GPU's fixed tree (strided partial sums, then
pairwise in shared memory) was the most accurate form of all: equal to the correctly rounded sum at every size,
because a tree's error grows with the depth, not the count. The CPU forms already disagree with one another today:
Spite's reassociated sum and `expert.c`'s eight lanes differ by 8,000 quarters at 10,000,000, and `naive.c`'s
in-order sum is 0.56% low. The GPU result is the same on every run, since the tree's shape is fixed by the group
count, not by timing.

In passing: Spite `--optimized` at 10,000,000 items is 4.8 times `expert.c`, against 2.8 times at 1,000,000. Two
passes over `into` once it no longer fits the cache explain part of it, not all; worth a look by the loop work.

**Conclusion.** At the case's size the idea barely lives. Resident and submitted once for all 2,000 rounds, the GPU
matches single-threaded `expert.c` (11.4 against 10.1 µs a round; the fused pass 8.9) and is 2.5 times faster than
Spite, and only under two conditions the compiler must prove or arrange: no round's result is read by the CPU before
the next round (here the sums only feed `total`, read after the loop, so they can stay on the device and come back
once), and the 220 ms of device setup is paid by something else (a program whose whole run is 40 ms cannot pay it;
a game that already has a device can). The floor is about 8 µs of GPU time a round for three dispatches with their
barriers, and about 55 µs for every trip from the CPU to the GPU and back.

The crossover: submitted once for all rounds, between 30,000 and 100,000 items against Spite, and just over 100,000
against `expert.c` (just under it for the fused pass); with a wait per round, between 300,000 and 1,000,000. Above
it the GPU keeps paying for as long as the lists fit in device memory (24 GB here): at 10,000,000 items the fused
pass is 13 times `expert.c` and 63 times Spite, because the CPU is bound by its memory (`expert.c` moves 80 MB a
round at about 48 GB/s) while the device runs near its own. It stops paying below about 50,000 items (the dispatch
floor), for any program too short to repay setup, and whenever the data moves every round: moving both lists each
round lost at every size (12, 4.5 and 2.4 times slower than `expert.c` at 100,000, 1,000,000 and 10,000,000), since
this loop does two operations for every eight bytes it would move. So the threshold R13 named, passes over resident
data, needs two more terms: the number of CPU round trips (which batching all rounds into one submission removes)
and a fixed setup cost that only a long run or a shared device repays. Fusing the scale and the sum into one kernel
saved another 20 to 30% at every size; the compiler should fuse passes it moves to the device, as `expert.c` does by
hand.

What the decimal rule needs. Reassociating a recognised reduction is already allowed, and this run shows it is the
only place GPU and CPU results differed. The rule should state four things: (1) a reduction may be summed in any
fixed order, including a tree, and the order is fixed by the compiled program, never by timing (no atomic `Float`
additions, which would make a result change from run to run with nothing to tell the user, a silent failure under
D244); (2) whether contraction into fused multiply-add is allowed in both forms, since the driver may contract a
multiply and an add the CPU form keeps apart; (3) denormals: devices may flush them to zero, so either the rule allows
it or a kernel needs a device that preserves them (`shaderDenormPreserveFloat32`) and keeps the CPU form otherwise;
(4) maths functions: Vulkan's built-in `sin`, `exp` and the rest have looser error bounds than the C library, so a
result must not depend on where it ran. The cheapest answer that keeps zero runtime is for the compiler to emit the
same maths functions as code into both forms (tree-shaken like any other function), so only reductions may differ,
and those are already a decimal's last bits.

**R14, the race in `concurrent_wait_cycle`.** There is no second thread: the two `Concurrent`s are frames stepped by
one thread. `joins` records who waits for whom and sees the cycle; `wait_for`, the in-place join a
`Concurrent.drop` runs, records nothing, so the cycle is invisible to it and the program halts naming a different
cause. Which join runs is chosen at run time by a guard on the handle's reference count, which depends on whether a
frame is released before or after `start_both` returns, which depends on the clock against the 1 ms and 5 ms
sleeps. Fix: one bookkeeping path for both joins, and no timing-dependent choice of join. Sent to a cloud agent.

Found (2026-10-08, branch `fix-wait-cycle-race`, D546): the guard was not what produced the wrong cause. Nothing can
step a frame before `start_both` returns, so the reference counts at the release are the same every run. Replaying
the generated C with a clock that jumps at a chosen call found the two outcomes that differ: a jump of 45 ms or more
at the main thread's first timer checks (a wait returning late under load) ends the 50 ms sleep before either
`Concurrent` runs, the program prints "never printed", and the circle is met at exit, after the `Scheduler` singleton
(made after `Board`, so destroyed before it) had let go of its loops; the drop that joined then read freed memory
and halted at `idle_stepping` with nothing to wait for. A tick inside the first frame's `timer_start`/`timer_over`
pair ends its 1 ms sleep at once, before the second `Concurrent` exists, so no circle forms at all. The fix runs
every unfinished `Concurrent` to its end when the entry function returns, makes the in-place join record its waiter
through `joins` (an in-place drop of one that waits for it at a join used to hang), and rewrites the program so its
`Concurrent`s wait for flags rather than for the clock. The analysis's doubt, `_collect()` straight after `begin`, is
not a bug: it collects only a frame that finished inside `begin`, and does nothing otherwise.

**R15, overflow checks on sums of list items and attributes.** The range proof keeps intervals for locals only;
`computed_range` has no case for an attribute or a list item, and the list-values machinery (D523) knows who fills a
list, not the range of what it holds. Speculate-and-replay needs bounds on each term to make its after-loop test
pass, so it only pays once item and attribute ranges exist (every write site's range, whole program): the two are
one piece of work, ranges first. Replay is safe for a counted loop whose body writes only locals (checkpointed) and
calls nothing with effects.

**R16, the navigation numbers.** They are right. `next_stamp` clears the 16.8 MB table only when the stamp reaches
1,000,000,000: the `assert stamp >= 1000000000` guard returns early on every other query, so a query costs about
84 microseconds of A*. Two notes: the guard reads like an invariant but is the only thing that skips the clear, and
the benchmark's timed window includes picking random goals and smoothing, so the figure is not search alone. If the
clear were real, the compiler's form would be a generation stamp it chooses itself (L6 does not fit: untouched
cells are read).

**R17, a plan for foreign structs (D536's second half).** Today `passes_as_struct` and `shape_struct_code` accept
only all-number types. Steps: a C layout with alignment (small); every attribute kind (Boolean and enum as 32 bits,
text, nested inline, pointer for `type?`, lists, callbacks) built in the caller's frame (large); compiler-written
counts (medium); reading back by name (medium); fixed arrays (medium); a size check against a header wherever one
is named (small). First engine sites to move, since they can read wrong values today: the Vulkan device properties
read at offset 720, the queue families at `index * 24`, the instance creation with its count written by hand, then
`MSG`, `RECT` and `XINPUT_STATE`, which already cross as number-only types.

**R18, scalar replacement of a row across a call.** The escape proof per parameter exists, and a callee that writes
the parameter's fields still qualifies. The obstacles: frames are per function, a singleton's attribute (the row)
cannot be a frame object, nothing proves the object filled in one call, used in the next and stored in a third is
one object, and no per-field write set records which fields a callee assigns. Inlining the runner at the Spite level
removes most of them; handed to the agent building it.

**R19 to R23, the ten cases furthest behind expert C (2026-10-09).** Read-only comparisons of `expert.c` with
`generated.c`; under D549 every expert was judged fair (it prints the same answer), so each gap is work the compiler
must learn. Themes, with the cases they reach:

- **Pools or a per-block arena for classes no list holds.** `class_pools.spite` pools only a list's item class, so
  a linked `Node`, an `Owner` an item points to, a copied order's `Customer` keep `malloc` and `free` per object.
  Pool every class whose makes and frees all run on the program's own thread, or allocate every object proven not
  to outlive its block from one block freed at once (`allocation_is_the_c_librarys_counted_only_where_read`, about
  60 of its 86 ms; `a_deep_copy_...`; `an_allocator_set_after_construction_...`, where an allocator whose `free` is
  empty also needs no give-back call, no count and no hidden pointers). Sent to a cloud agent (branch `memory-wins`).
- **Columns, and an object merged into its only owner.** `defaults_the_constructor_replaces_are_never_made` (84x)
  is a list of items each pointing at a fresh `Owner`; expert keeps two columns. L1 plus the one-owner merge, in one
  fused pass. Its name misleads: the defaults optimisation is built and correct; the 84x is layout.
- **One pass over an unchanged list, reductions combined.** `report_over_records` makes 64 filtered passes over 2
  million sales (82% of naive C's time); expert does one fused pass with the cell's number as a weight, legal since
  the per-cell sums are never printed. Also two sibling sums in `defaults_...`, and `text_building`'s join (measure,
  copy, write and the word loop into one write). Queued after the Spite-level inlining work.
- **Plain attributes under a held or skipped lock; per-thread reductions.** Inside the counted loop's single lock and
  while no task runs, singleton attributes still use SEQ_CST atomics; expert keeps them in registers, and shards a
  sum, count and max per thread with one merge (`a_counted_loop_...`, `while_no_task_runs_...`). A file used in a loop
  could keep its handle open and read into a frame buffer (`concurrency_machinery_only_where_it_is_used`). Sent to
  a cloud agent (branch `singleton-wins`).
- **Copies and lists sized once.** A generated deep copy runs the defaults it then throws away (400,000 wasted
  allocations); a list copy appends one item at a time; a list filled by a loop with a proven trip count grows by
  doubling. Part of `memory-wins`.
- **Text.** The join writes byte by byte through a checked position and a checked narrowing; expert uses `memcpy`
  per piece and keeps its buffer as the result. With the range of `code_at` and the join inlined, both checks fall
  and the copy becomes `memcpy`; a fresh buffer nothing else names can become the `String`'s block.
- **Specialising over a fixed list (S1).** `an_argument_its_caller_holds_is_passed_without_counting` (19x): the
  matcher divides by `headers[index]`, nine million `idiv`s; expert's headers are the constants 1, 2, 3, so clang
  folds the divisions and vectorises. S1 step 2 folds them; the inlining agent is building it.

**R25 to R28, the next cases behind expert C (2026-10-09).** Again every expert is fair under D549. What wins most:

- **Per-field layout is the largest single win across the most cases** (L1, L2, L3, S2, with an object merged into
  its only owner): a list's templates (11.4x), template chains (11.4x), a name's held item (8.6x), the spreadsheet
  (6.9x; the split alone is 3.3x by the data-oriented study's own measurement) and the defaults case (84x). Expert
  keeps columns, so the loops vectorise; Spite walks a pointer array into objects. Fusion only pays after it.
- **Ranges of attributes, list items and call answers** (A1 extended): checked arithmetic sits at the hottest point
  of every items-and-rows case and is what keeps clang from vectorising them.
- **A test and a read of the same item share one read**: `crash cells[cell.first]` then `cells[cell.first].value`
  reads, bounds-checks and counts twice (spreadsheet, sorting, borrowed rows, a played track).
- **Text whose result does not escape needs no `String`**: a joined line used only for its length or added to a
  total and dropped can be built in a frame buffer with direct copies and literal lengths, no allocation per line
  (`text_joined_in_one_piece` 10.4x, `a_number_joined_into_text_is_written_in_place` 5.0x).
- **The library's sort is the compiler's to choose**: `sort_by_placed_at` merge-sorts through checked reads; a
  stable radix sort of (key, position) gives the same permutation for whole-number keys with a proven range
  (`records_sorted_by_one_field`, its largest phase). A program's own hand-written sort is another matter: replacing
  it needs a proof of what it computes, beyond the compiler today.
- **Facts fixed after setup** (S1 step 2): a played track's per-track value recomputed every pass (9x) folds to a
  table of sixteen constants.
- **Smaller:** a scratch list of three items scalar-replaced into locals; a made singleton's address hoisted out of
  a loop; a list's count read once when no call in the loop can append; parameter ranges from every call site.

**R29 and R31, batch eight (2026-10-09).** Layout (L1, L2, L3, S2 with M8) again wins most: it is the whole gap
of `reading_through_a_type_without_counting` and the largest part of `reflection_on_constants_folds_and_unrolls`
and `maths_on_constants_is_worked_out_while_compiling`. New findings:

- **Whole computations worked out while compiling (D549).** `reflection_on_constants_folds_and_unrolls` and
  `a_binary_schema_is_a_constant` read no input: every value comes from literals and `%` over fixed counts, all in
  exact integers. The whole printed answer can be computed while compiling and the program reduced to printing it.
  This is a class of win the compiler does not attempt yet: evaluating a closed part of a program (no input, no
  clock, no IO) in advance. `maths_on_constants_...` is decimal, so it folds only if the compiler's evaluation gives
  the target's bits (rounding, contraction, `round`), which needs a proof or must stay a loop.
- **The dictionary's representation:** a table sized once from a proven count, keys and values in two arrays with an
  empty marker, a probe that reads one key; today it rehashes about nineteen times for 500,000 inserts, reads about
  four values per hit through a checked nullable read, and computes a hash fragment it never uses (D533 allows any
  representation).
- **A predicate call per item inside a count** (`count(is_current)`): inlined and fused into one counted loop.



## Outside the constraints (recorded, not pursued)

Ideas that would need a runtime or could change a result, kept so they are not rediscovered as new:

- A work-stealing scheduler shipped with the program.
- Just-in-time recompilation from run-time profiles.
- Garbage collection of cycles at run time.
- Floating-point reassociation that changes results.
