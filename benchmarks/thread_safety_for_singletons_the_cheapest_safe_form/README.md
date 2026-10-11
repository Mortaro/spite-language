# Thread safety for singletons, the cheapest safe form

For each singleton of the program's own that can change after it is made, the compiler picks the cheapest form that
is as safe as a lock, from what its functions do: nothing for one no `Parallel` reaches, nothing for one that never
changes, single atomic instructions for counters and flags each function touches once, and the lock only when none
of these holds. The source is the same in every form.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form).
- The proof: [Which singletons a `Parallel` reaches](../../docs/proofs.md#which-singletons-a-parallel-reaches) and
  [Read-only and atomic singletons](../../docs/proofs.md#read-only-and-atomic-singletons).

## The four forms

- [`naive/`](naive/): four `Parallel` workers, each reading a step from a `Settings` singleton made once and
  recording it into a `HitCounter` singleton a million times, while a `Journal` singleton holding a `List` is
  written only by the program's own thread, before and after.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the counter and the settings,
  which the threads share, each behind a mutex taken by every call; the journal a plain list.
- [`expert.c`](expert.c): the same work tuned by hand: the settings read plainly, each thread adding its hits in a
  register and storing them once on a cache line of its own, and the program's thread adding the four counts.
- [`highlights.c`](highlights.c): which singletons are atomic, the worker's loop, and the three singletons' functions
  the program calls most.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

No function here takes a lock. `HitCounter_record` is one `__atomic_fetch_add` on `hits_` (the overflow check reads
the value it answers), since `HitCounter___atomic` is 1: its one changing attribute is a whole number, and each of
its functions touches it once. `Journal_note` appends with no lock and no atomic, since no `Parallel` reaches
`Journal`. `Settings_step_size` reads `step_` through `SPITE_SINGLETON_LOAD`, and `Worker_run` calls the two in
turn with nothing around them. `naive.c` takes a mutex twice per round, once for the step and once for the hit;
`expert.c` makes the hits in registers and writes the shared count once per thread.

`Settings___atomic` is 0 (in `generated.c`), so that load is a plain one, as the page says of a singleton that never
changes.
The four workers' atomic additions on one line are what is left between the Spite and `expert.c`.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 34 907 | 236 544 |
| naive C: `naive.c`, `clang -O2` | 157 826 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 389 | 139 264 |

Spite takes 0.22 times naive C's time and 89.74 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=34907 naive=157826 expert=389 -->
<!-- /timings -->
