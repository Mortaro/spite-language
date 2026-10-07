# Thread safety for singletons, the cheapest safe form

For each singleton of the program's own that can change after it is made, the compiler picks the cheapest form that
is as safe as a lock, from what its functions do: nothing for one no `Parallel` reaches, nothing for one that never
changes, single atomic instructions for counters and flags each function touches once, and the lock only when none
of these holds. The source is the same in every form.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form).
- The proof: [Which singletons a `Parallel` reaches](../../../docs/proofs.md#which-singletons-a-parallel-reaches) and
  [Read-only and atomic singletons](../../../docs/proofs.md#read-only-and-atomic-singletons).

## The four forms

- [`naive/`](naive/): four `Parallel` workers, each reading a step from a `Settings` singleton made once and
  recording it into a `HitCounter` singleton a million times, while a `Journal` singleton holding a `List` is
  written only by the program's own thread, before and after.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the counter and the settings,
  which the threads share, each behind a mutex taken by every call; the journal a plain list.
- [`expert.c`](expert.c): the same work tuned by hand: the settings read plainly, each thread adding its hits in a
  register and storing them once on a cache line of its own, and the program's thread adding the four counts.
- [`generated.c`](generated.c): which singletons are atomic, the worker's loop, and the three singletons' functions
  the program calls most.

## What to look at in generated.c

No function here takes a lock. `HitCounter_record` is one `__atomic_fetch_add` on `hits_` (the overflow check reads
the value it answers), since `HitCounter___atomic` is 1: its one changing attribute is a whole number, and each of
its functions touches it once. `Journal_note` appends with no lock and no atomic, since no `Parallel` reaches
`Journal`. `Settings_step_size` reads `step_` through `SPITE_SINGLETON_LOAD`, and `Worker_run` calls the two in
turn with nothing around them. `naive.c` takes a mutex twice per round, once for the step and once for the hit;
`expert.c` makes the hits in registers and writes the shared count once per thread.

One thing differs from the page: the page says a singleton that never changes is read plainly, but `Settings___atomic`
is 1 too, so `SPITE_SINGLETON_LOAD` reads `step_` with a sequentially consistent `__atomic_load_n`. On x86 that is
the same plain load, so it costs nothing here; on a machine with a weaker memory order it is an ordered load, slower than a plain one.
The four workers' atomic additions on one line are what is left between the Spite and `expert.c`.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 12 015 | 235 008 |
| naive C: `naive.c`, `clang -O2` | 165 715 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 1 050 | 139 264 |

Spite takes 0.07 times naive C's time and 11.44 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=12015 naive=165715 expert=1050 -->
<!-- /timings -->
