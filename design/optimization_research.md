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

## Outside the constraints (recorded, not pursued)

Ideas that would need a runtime or could change a result, kept so they are not rediscovered as new:

- A work-stealing scheduler shipped with the program.
- Just-in-time recompilation from run-time profiles.
- Garbage collection of cycles at run time.
- Floating-point reassociation that changes results.
