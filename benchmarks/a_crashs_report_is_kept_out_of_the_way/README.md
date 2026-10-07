# A crash's report is kept out of the way

Every `crash`, failed `assert`, read outside a list and overflow has code that writes its report, and that code
runs at most once. The compiler moves each report out of the function that holds it into a cold function of its
own, given exactly the values the report prints, so what is left where the check is written is one comparison and
a call that is never made.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-crashs-report-is-kept-out-of-the-way).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the report prints only values the
  function can hand over as arguments, and a report that would print one it cannot name stays where it is.

## The four forms

- [`naive/`](naive/): 1 300 values and 100 000 picks, and three hundred rounds of adding up the values the picks
  name, each pick shifted by the round and checked with `crash values[pick]` before it is read. No check fails.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: two growable arrays, and the
  check's report, an `fprintf` of the place and every value, written inside the loop where the check is.
- [`expert.c`](expert.c): plain arrays, the check one unsigned comparison marked unlikely, and the report in a
  `noinline, cold, noreturn` function of its own.
- [`highlights.c`](highlights.c): the round's loop and the cold report function the compiler wrote for its check.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_round_total___held_0_1` the `crash values[pick]` line is

```c
if (!(((List_Integer_get_at(values_, pick_)).has_value))) {
    spite_failed_1(pick_, values_, round_, total_, index_);
}
```

and everything it would print (the site, `values[pick] is missing: index ..., count ...`, and `round`, `total`
and `index`) is in `spite_failed_1`, marked `SPITE_CRASH_REPORT`, which is `noinline, cold, noreturn`: the loop
holds the test and a call the C compiler moves out of the hot path. That is what `expert.c` writes by hand; in
`naive.c` the `fprintf` with its five values sits inside the loop, where the C compiler may or may not move it
out. The overflow checks on `picks[index] + round` and `total + values[pick]` call `spite_overflowed`, another
function kept out of the loop, with the operands and the site.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 22 791 | 177 152 |
| naive C: `naive.c`, `clang -O2` | 14 668 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 12 454 | 139 776 |

Spite takes 1.55 times naive C's time and 1.83 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=22791 naive=14668 expert=12454 -->
<!-- /timings -->
