# A proven read tests only its bounds

Every `[]` answers a `T?`, and a read proven by a bound (`at + 2 < values.count()`, or `index < count` after
`var count = values.count()`) needs no narrowing written. The compiler takes such a read straight from the list's
`get_at` with one branch the C compiler is told is never taken, which halts naming the read if the index was
outside the list after all, and tests no presence of the `T?` beyond that.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-proven-read-tests-only-its-bounds).
- The proof: [A proven count or bound proves a read](../../docs/proofs.md#a-proven-count-or-bound-proves-a-read),
  and what such a read does when its proof covered only the top of the index,
  [A proven read halts outside its list](../../docs/proofs.md#a-proven-read-halts-outside-its-list).

## The four forms

- [`naive/`](naive/): a `List<Integer>` of 100 000 numbers and 300 rounds of two loops that are not the counted
  loop's shape: a sum over windows of three (`while at + 2 < values.count()`, reading `values[at]` to
  `values[at + 2]`) with a weight that changes with the round, and a count of the items that rise above the one
  before by more than a threshold (`while index < count`, the count kept in a `var`).
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of ints, the
  same two loops reading it with no check at all.
- [`expert.c`](expert.c): the same work tuned by hand: a plain `restrict` array and both loops written so the C
  compiler vectorises them, the rise counted without a branch.
- [`highlights.c`](highlights.c): what the compiler writes for the two loops, and the list's `get_at` they read
  through.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Each read in `Naive_rise_count___held_0` is `List_Integer_get_at(values_, index_)` followed by `if
(__builtin_expect(!spite_temp_<n>.has_value, 0)) spite_outside_list("values[index]", ...)`: once `get_at` is
inlined, its own `index_ >= 0 && index_ < item_count_` compare is the only test, and the branch that halts is marked
as never taken. No narrowing is written in `naive/`, and none is compiled beyond that compare: the bound is a count
kept in a `var`, `index < count`, which proves the read but is not the counted loop's shape.

`Naive_window_total___held_0` is the counted loop's shape since a counter plus a literal counts too: `while at + 2 <
values.count()` runs while `at_ < spite_temp_<n> - 2`, with the count read once, and `values[at]`, `values[at + 1]`
and `values[at + 2]` are `spite_temp_<m>[at_]` to `spite_temp_<m>[at_ + 2]`, with no compare and no overflow check
on `at + 1` and `at + 2` (before, each was a `get_at` with its compare and `values.count()` was read each pass).
What both loops still pay for is the checked arithmetic of their sums and of `value - previous`, which keeps the C
compiler from vectorising them as it does `naive.c` and `expert.c`: the items are numbers the list was given in
another function, so no range bounds them. `rises + 1` has no check (it adds at most one a pass, and the passes are fewer
than the count), nor have `round % 3 + 1` and the `Long` `total + windows + rises` in `rounds`
([a range proves them](../../docs/optimizations.md#arithmetic-a-range-proves-is-not-checked)).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 36 851 | 175 616 |
| naive C: `naive.c`, `clang -O2` | 7 545 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 7 613 | 140 288 |

Spite takes 4.88 times naive C's time and 4.84 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=36851 naive=7545 expert=7613 -->
<!-- /timings -->
