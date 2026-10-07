# Calls in a row run at once

Statements in a row that each call a function on an object of the program, and share nothing one of them writes,
run on the thread pool at once. The compiler works out what each call reads and writes from the code of every
function it reaches, and overlaps the row when the two have nothing in common.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#calls-in-a-row-run-at-once).
- The proof: [Calls that share nothing written](../../docs/proofs.md#calls-that-share-nothing-written).

## The four forms

- [`naive/`](naive/): two objects, `Evens` and `Odds`, each with a `count()` that adds up 40 million numbers into
  its own `total`, called one after the other with no `Parallel` written.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: two structs and the two counts
  called one after the other on the program's one thread.
- [`expert.c`](expert.c): the same work tuned by hand: the even count on a second thread while the odd count runs on
  the program's own, each summing into a local and storing it once, on a cache line of its own.
- [`highlights.c`](highlights.c): the row of calls and the two counts.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_count_both` hands `evens.count` to `Parallel__Nothing___make`, which starts it on the thread pool, calls
`Odds_count` on the program's own thread meanwhile, and waits for the first with `Parallel__Nothing__join` before
the function returns. The blank lines are what is left of the row's other copy, the one in order, which the
compiler wrote too and dropped. `Evens_count` and `Odds_count` are the plain loops, each writing only its own
object's `total_`. `naive.c` runs the two loops one after the other; `expert.c` overlaps them on two threads as the
Spite does, without its overflow checks.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 38 672 | 224 768 |
| naive C: `naive.c`, `clang -O2` | 46 390 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 24 947 | 139 776 |

Spite takes 0.83 times naive C's time and 1.55 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=38672 naive=46390 expert=24947 -->
<!-- /timings -->
