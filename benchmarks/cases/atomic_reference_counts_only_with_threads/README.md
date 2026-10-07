# Atomic reference counts only with threads

Retaining and releasing an object is plain arithmetic in a program that can never share an object between threads:
one that makes no `Concurrent` or `Parallel`, runs no `parallel_each_` pass and is not built for the REPL or live
reload. Only a program that does is compiled with atomic counts, decided from what it uses.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#atomic-reference-counts-only-with-threads).
- The proof: none of its own; it is the whole-program case of
  [No other thread counts a class](../../../docs/proofs.md#no-other-thread-counts-a-class), where there is no other
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
- [`generated.c`](generated.c): the two definitions of the count macros, a point's retain and release, and the loop
  that keeps the points.

## What to look at in generated.c

The C has `SPITE_COUNT_UP` and `SPITE_COUNT_DOWN` twice: as `__atomic_add_fetch` and `__atomic_sub_fetch` under
`#ifdef SPITE_THREADS`, and as `((count) = (count) + 1)` and `- 1` otherwise. This program's C never defines
`SPITE_THREADS` (one that makes a `Parallel` starts with `#define SPITE_THREADS` on its first line), so
`Point___retain`, called for every kept point in `Naive_count_near___held_0`, and `Point___release`, called for each
by `List_Point___release(kept_)` at the end of a round, are a plain addition and subtraction. `naive.c` counts the
same way; `expert.c` counts nothing.

## Timings

<!-- timings -->
<!-- /timings -->
