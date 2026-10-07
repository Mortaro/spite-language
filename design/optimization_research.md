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
- Measure against the hand-tailored form, on the engine package's benchmarks and `benchmarks/versus_c`, in an
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

## Representation per use, not per class (D518)

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
  biggest measured gap in the naive engine (about 70% of its stress tick).

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

## Outside the constraints (recorded, not pursued)

Ideas that would need a runtime or could change a result, kept so they are not rediscovered as new:

- A work-stealing scheduler shipped with the program.
- Just-in-time recompilation from run-time profiles.
- Garbage collection of cycles at run time.
- Floating-point reassociation that changes results.
