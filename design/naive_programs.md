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

1. **The naive form is the reference.** A step starts by writing the naive version of a piece of the engine (in a
   worktree folder that `load`s the engine and reopens only what it changes, D156) and measuring it against the
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
- Write the docs for D506 and D507: a section that teaches writing the plain program and lists what the compiler
  does with it, linked from the reading order (where it lives is question Q1 below).
- Clear the inbox (done with this plan: D507 row recorded).

### Stage 1: the naive engine and its baseline (medium, the most important stage)

- A worktree folder beside the engine package, `naive/`, that loads the engine and reopens the classes that use
  `Parallel`, `Concurrent`, `Memory` and hand allocators, rewritten as plain lists, loops and classes.
- Run every engine benchmark for both forms; record the table in this file. The table is the work queue: every
  row where naive loses is a pair to build.
- Expected (from the proposal and the stress measurements): locks, per-field layout and loop parallelism dominate;
  memory matters only in frame loops that allocate (render, UI).

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

## Questions for Mortaro

- **Q1.** Where do D506 and D507 live in the docs: a new page in the reading order ("Writing for speed: write it
  plainly"), or the top of `docs/optimizations.md`? Recommend a new short page, since it is how to write code,
  and `optimizations.md` stays the list of what the compiler does.
- **Q2.** May an automatic optimisation decide at run time between two compiled forms, by one branch on a number
  only the run knows (a list's length for parallel or serial)? Recommend yes: it is a branch, not a scheduler, and
  the page says so.
- **Q3.** Does the naive engine replace the engine package's `main` once it is as fast, with the hand version kept only as a
  tagged baseline? Recommend yes; until then the naive form lives in a worktree folder.
- **Q4.** D505's conditions beyond the decision itself (only bare calls, only calls that reach a loop, only
  objects told apart by class) are proposed by Claude, unconfirmed. Confirm, or say what to change.
