# A counted loop of calls to one singleton takes its lock once

A `while` that calls a locked singleton's functions many times takes the singleton's lock once around the whole
loop and calls the functions' unlocked bodies inside it. It does so only when the loop is counted, locks nothing
else and waits for nothing, so the lock held longer can neither deadlock nor wait.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once).
- The proof: [A counted loop takes a singleton's lock once](../../docs/proofs.md#a-counted-loop-takes-a-singletons-lock-once).

## The four forms

- [`naive/`](naive/): four `Parallel` workers, each calling `tally.add(index % 3)` 5 million times on one shared
  `Tally` singleton that adds the amount, counts the call and keeps the largest amount, then the program reads the
  three back.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one tally behind a mutex, four
  threads, and every call to `add` taking and letting go of the mutex, as an object threads share is guarded in C.
- [`expert.c`](expert.c): the same work tuned by hand: each thread makes every call's addition, count and comparison
  into a tally of its own on its own cache line, with no lock and no atomic operation, and the program's thread adds
  the four tallies together once they are done.
- [`highlights.c`](highlights.c): the worker's loop and the unlocked body of `add` it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Counter_count_up` calls `spite_coarse_1_enter()` before its `while` and `spite_coarse_1_leave()` after it, and the
loop calls `Tally_add___unguarded`, so a worker takes the tally's lock once for its 5 million calls. While one worker
holds it the other three wait for the whole of its loop, so the four loops run one after another. `naive.c` takes the
mutex 20 million times, handing it from core to core; `expert.c` takes nothing.

The unlocked body still writes `total` and `calls` with `__atomic_fetch_add` (sequentially consistent) and reads and
writes `largest` through `SPITE_SINGLETON_LOAD` and `SPITE_SINGLETON_STORE`, because `Tally___atomic` is 1: those
attributes are atomic on their own, so another class could read them without the lock. Inside the loop the lock is
held, so the loop pays one lock and two locked additions per call; the additions are what is left between the Spite
and a plain loop under one lock.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 71 587 | 233 472 |
| naive C: `naive.c`, `clang -O2` | 334 091 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 084 | 140 288 |

Spite takes 0.21 times naive C's time and 23.21 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=71587 naive=334091 expert=3084 -->
<!-- /timings -->
