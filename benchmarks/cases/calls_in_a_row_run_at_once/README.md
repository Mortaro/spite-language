# Calls in a row run at once

Statements in a row that each call a function on an object of the program, and share nothing one of them writes,
run on the thread pool at once. The compiler works out what each call reads and writes from the code of every
function it reaches, and overlaps the row when the two have nothing in common.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#calls-in-a-row-run-at-once).
- The proof: [Calls that share nothing written](../../../docs/proofs.md#calls-that-share-nothing-written).

## The four forms

- [`naive/`](naive/): two objects, `Evens` and `Odds`, each with a `count()` that adds up 40 million numbers into
  its own `total`, called one after the other with no `Parallel` written.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: two structs and the two counts
  called one after the other on the program's one thread.
- [`expert.c`](expert.c): the same work tuned by hand: the even count on a second thread while the odd count runs on
  the program's own, each summing into a local and storing it once, on a cache line of its own.
- [`generated.c`](generated.c): the row of calls and the two counts.

## What to look at in generated.c

`Naive_count_both` hands `evens.count` to `Parallel__Nothing___make`, which starts it on the thread pool, calls
`Odds_count` on the program's own thread meanwhile, and waits for the first with `Parallel__Nothing__join` before
the function returns. The blank lines are what is left of the row's other copy, the one in order, which the
compiler wrote too and dropped. `Evens_count` and `Odds_count` are the plain loops, each writing only its own
object's `total_`. `naive.c` runs the two loops one after the other; `expert.c` overlaps them on two threads as the
Spite does, without its overflow checks.

## Timings

<!-- timings -->
<!-- /timings -->
