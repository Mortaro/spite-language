# A proven read tests only its bounds

Every `[]` answers a `T?`, and a read proven by a bound (`at + 2 < values.count()`, or `index < count` after
`var count = values.count()`) needs no narrowing written. The compiler takes such a read straight from the list's
`get_at` with one branch the C compiler is told is never taken, which halts naming the read if the index was
outside the list after all, and tests no presence of the `T?` beyond that.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-proven-read-tests-only-its-bounds).
- The proof: [A proven count or bound proves a read](../../../docs/proofs.md#a-proven-count-or-bound-proves-a-read),
  and what such a read does when its proof covered only the top of the index,
  [A proven read halts outside its list](../../../docs/proofs.md#a-proven-read-halts-outside-its-list).

## The four forms

- [`naive/`](naive/): a `List<Integer>` of 100 000 numbers and 300 rounds of two loops that are not the counted
  loop's shape: a sum over windows of three (`while at + 2 < values.count()`, reading `values[at]` to
  `values[at + 2]`) with a weight that changes with the round, and a count of the items that rise above the one
  before by more than a threshold (`while index < count`, the count kept in a `var`).
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of ints, the
  same two loops reading it with no check at all.
- [`expert.c`](expert.c): the same work tuned by hand: a plain `restrict` array and both loops written so the C
  compiler vectorises them, the rise counted without a branch.
- [`generated.c`](generated.c): what the compiler writes for the two loops, and the list's `get_at` they read
  through.

## What to look at in generated.c

Each read in `Naive_window_total___held_0` and `Naive_rise_count___held_0` is
`List_Integer_get_at(values_, at_)` followed by `if (__builtin_expect(!spite_temp_9.has_value, 0))
spite_outside_list("values[at]", ...)`: once `get_at` is inlined, its own `index_ >= 0 && index_ < item_count_`
compare is the only test, and the branch that halts is marked as never taken. No narrowing is written in
`naive/`, and none is compiled beyond that compare. These loops are not the counted loop's shape, so unlike
[the counted loop](../a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/) they keep the
compare per read, read `values.count()` again each pass of `window_total`, and check `at + 2` and `at + 1` for
overflow, each a branch the expert's loops do without. `naive.c` reads the array with no compare at all (and halts
nowhere if an index is wrong); `expert.c` does the same and vectorises both loops.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 36 216 | 176 128 |
| naive C: `naive.c`, `clang -O2` | 7 503 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 7 447 | 140 288 |

Spite takes 4.83 times naive C's time and 4.86 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=36216 naive=7503 expert=7447 -->
<!-- /timings -->
