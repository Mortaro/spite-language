# A singleton no other thread reaches takes no lock

While compiling, a singleton whose function is made into a value counts as one a `Parallel` might reach, and gets a
lock. Once the program is written out, the compiler sees which code can actually run on another thread, and a
singleton none of it touches keeps no lock and no atomic attribute.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-singleton-no-other-thread-reaches-takes-no-lock).
- The proof: [docs/proofs.md](../../docs/proofs.md#no-other-thread-touches-a-singleton).

## The four forms

- [`naive/`](naive/): one `Parallel` runs a `Worker` that sums numbers of its own, `ledger.is_large` is handed to
  `filter`, and then the program's own thread calls `ledger.note(index)` ten million times, reading
  `ledger.milestones.count()` after each call. The `Ledger` singleton holds a list, so it is neither read-only nor
  atomic.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the worker thread touches
  nothing of the ledger, so the ledger is a plain global struct with no mutex, its milestones a growable array.
- [`expert.c`](expert.c): the same work tuned by hand: the ledger's total, count and milestones live in locals for
  the whole loop, a countdown replaces the remainder, and the struct is written once at the end.
- [`highlights.c`](highlights.c): the loop, `Ledger_note` and its body, and the flag that makes the body's
  attributes atomic.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Ledger_note` is only `Ledger_note___unguarded` between the two changes of the count of locks held: no test for work
on the pool, no push on `spite_skipped`, no `spite_guard_enter_writing`. `Ledger___atomic` is 0, so the body's
`total = total + amount` and `notes = notes + 1` are plain additions (`if (Ledger___atomic)` folds away), where the
first walk had made them `__atomic_fetch_add`. Built with the compiler before this optimisation, the same program
wraps `Ledger_note` in the skip-or-lock wrapper and keeps `Ledger___atomic` at 1, and its loop takes 41.1 ms
instead of 24.3 (medians of seven, the C compiled without link-time optimisation). What is left between the Spite and
`naive.c` is mostly the checked arithmetic: three additions per call that halt on overflow.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 21 943 | 230 912 |
| naive C: `naive.c`, `clang -O2` | 4 926 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 253 | 139 264 |

Spite takes 4.45 times naive C's time and 6.75 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=21943 naive=4926 expert=3253 -->
<!-- /timings -->
