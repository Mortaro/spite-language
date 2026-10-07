# A singleton's reading functions do not exclude each other

A singleton a `Parallel` reaches is locked, but a function of it that only reads (`at(row)`) does not take the
lock: it adds one to a count of its own thread's, on a cache line of its own, checks that no writer holds the lock,
reads and takes the one away. A writing function takes the lock and waits for every count to be zero, so readers on
different threads never touch a line in common, and never run beside a writer.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-singletons-reading-functions-do-not-exclude-each-other).
- The proof: [Reading functions share the lock](../../../docs/proofs.md#reading-functions-share-the-lock).

## The four forms

- [`naive/`](naive/): the shape of `benchmarks/singleton_reads`: a `Transforms` singleton holding 15 000
  transforms, and 20 ticks that each start eight `Parallel` systems, each reading every row through
  `transforms.at(row)` and adding up `across`; between ticks the program inserts one more transform, so the
  singleton has a writing function and keeps its lock.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the singleton a global with a
  list of pointers and a mutex that each of its functions takes, a thread started per system each tick and joined.
  Eight threads then queue on one mutex for every read.
- [`expert.c`](expert.c): the same work tuned by hand: the transforms as one column of numbers and no lock at all,
  since the column is written only between ticks, when every thread has been joined; each thread sums the rows in a
  vectorised loop.
- [`generated.c`](generated.c): what the compiler writes for `run`, the reading `at`, the writing `insert` and the
  readers' and writers' sides of the lock.

## What to look at in generated.c

`Transforms_at` (when tasks are running) calls `spite_read_enter`: a locked add on
`readers[spite_reader_index].count`, the count of this thread's slot, then a load of `guard->owner` to check no
writer holds it, then the read through `Transforms_at___unguarded`, then `spite_read_leave`, a subtract on the same
slot. `Transforms_insert___held_0` instead calls `spite_guard_enter_writing`, which takes the lock and waits until
every slot's count is zero. When no task runs at all, both skip to the unguarded function (the section [While no
task runs, a singleton's lock is skipped](../../../docs/optimizations.md#while-no-task-runs-a-singletons-lock-is-skipped)).
Without the optimisation every `at(row)` would take the singleton's one lock, a compare-and-swap on a line all
eight threads write, which is what `naive.c`'s mutex does; `expert.c` takes nothing. `System_run` still counts each
transform it is answered (`Transform___release` at the end of the loop body): that count is not this
optimisation's, and `expert.c` has none.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 17 384 | 238 080 |
| naive C: `naive.c`, `clang -O2` | 102 988 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 5 819 | 140 288 |

Spite takes 0.17 times naive C's time and 2.99 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=17384 naive=102988 expert=5819 -->
<!-- /timings -->
