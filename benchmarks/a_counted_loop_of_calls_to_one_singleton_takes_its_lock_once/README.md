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

The unlocked body adds to `total` and `calls` and compares `largest` plainly, its `if (Tally___atomic)` tests
folding away: `Tally___atomic` is 0 (in `generated.c`), because the
program reads the three only through `sum`, `call_count` and `largest_amount`, which take the lock, so nothing reads
them past it ([An attribute read only under its singleton's lock is a plain
number](../../docs/optimizations.md#an-attribute-read-only-under-its-singletons-lock-is-a-plain-number)). What is left
between the Spite and `expert.c` is that the four loops run one after another under the one lock, where `expert.c`
runs them at once, each on a tally of its own.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 15 756 | 240 640 |
| naive C: `naive.c`, `clang -O2` | 284 999 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 2 962 | 140 288 |

Spite takes 0.06 times naive C's time and 5.32 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=15756 naive=284999 expert=2962 -->
<!-- /timings -->
