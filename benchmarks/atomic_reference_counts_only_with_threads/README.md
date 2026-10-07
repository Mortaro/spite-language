# Atomic reference counts only with threads

Retaining and releasing an object is plain arithmetic in a program that can never share an object between threads:
one that makes no `Concurrent` or `Parallel`, runs no `parallel_each_` pass and is not built for the REPL or live
reload. Only a program that does is compiled with atomic counts, decided from what it uses.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#atomic-reference-counts-only-with-threads).
- The proof: none of its own; it is the whole-program case of
  [No other thread counts a class](../../docs/proofs.md#no-other-thread-counts-a-class), where there is no other
  thread at all.

## The four forms

- [`naive/`](naive/): 200 000 points, and 20 rounds that each keep the points past a bound in a new list, counting
  each kept point once more as it is appended and once less when the list goes.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc` per
  object, a growable array of pointers per list, and a count per point that is a plain addition and subtraction,
  since a C program with one thread has no reason for atomics. It is what this optimisation brings the Spite
  program to, so the two take about the same time.
- [`expert.c`](expert.c): the same work tuned by hand: the points as two columns of numbers with no count at all, and
  each round's kept list positions written branchlessly into one reused buffer.
- [`highlights.c`](highlights.c): the two definitions of the count macros, a point's retain and release, and the loop
  that keeps the points.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The C has `SPITE_COUNT_UP` and `SPITE_COUNT_DOWN` twice: as `__atomic_add_fetch` and `__atomic_sub_fetch` under
`#ifdef SPITE_THREADS`, and as `((count) = (count) + 1)` and `- 1` otherwise. This program's C never defines
`SPITE_THREADS` (one that makes a `Parallel` starts with `#define SPITE_THREADS` on its first line), so
`Point___retain`, called for every kept point in `Naive_count_near___held_0`, and `Point___release`, called for each
by `List_Point___release(kept_)` at the end of a round, are a plain addition and subtraction. `naive.c` counts the
same way; `expert.c` counts nothing.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 20 865 | 178 176 |
| naive C: `naive.c`, `clang -O2` | 28 207 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 280 | 139 776 |

Spite takes 0.74 times naive C's time and 9.15 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=20865 naive=28207 expert=2280 -->
<!-- /timings -->
