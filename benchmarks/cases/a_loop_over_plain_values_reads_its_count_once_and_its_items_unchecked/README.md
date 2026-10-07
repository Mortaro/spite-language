# A loop over plain values reads its count once and its items unchecked

`while index < from.count()` over a `List<Float>`, with `into[index] = from[index] * 1.5 + offset` in it, reads as a
call to `count()` and a checked read and write each pass. The compiler proves the counter stays in range and the
lists keep their size, so it reads the count and the item addresses once, checks the second list's size once before
the loop, and writes the plain C loop a C compiler turns into vector instructions.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked).
- The proof: [A counted loop reads its items unchecked](../../../docs/proofs.md#a-counted-loop-reads-its-items-unchecked).

## The four forms

- [`naive/`](naive/): two lists of 100 000 `Float`s and 2 000 rounds, each scaling `from` into `into` with an
  offset that changes with the round and adding `into` up. Every value is a whole number of quarters and every sum
  stays below 2^24 quarters, so a sum is exact in any order and all three programs print the same total.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of floats per
  list, a function per loop, the items read through the list's pointer and count each pass.
- [`expert.c`](expert.c): the same work tuned by hand: `restrict` arrays, and each round's scaling and sum fused into
  one pass with eight running sums.
- [`generated.c`](generated.c): what the compiler writes for the two loops.

## What to look at in generated.c

In `Naive_scale_list___held_0_1` the count of `from` and both lists' item addresses are read into locals before
the loop (`spite_temp_1` to `spite_temp_4`), and `if (spite_temp_1 <= spite_temp_3)` checks once that `into` is
long enough. Inside, `spite_temp_4[index_] = ((spite_temp_2[index_] * 1.5f) + offset_)` is a plain store with no
range check and no `T?`; the `else` keeps the loop as written, through `List_Float_set_at`, for a short `into`. In
`Naive_add_up___held_0` the sum is `total_ + spite_temp_2[index_]` under `#pragma clang fp contract(fast)
reassociate(on)`, which lets clang vectorise a `Float` sum. `naive.c` gets the scaling loop vectorised too (clang
checks the overlap of its two arrays itself), but its `Float` sum stays in order, one addition after another;
`expert.c` makes one pass where these two loops make two.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 39 530 | 202 752 |
| naive C: `naive.c`, `clang -O2` | 159 928 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 17 720 | 139 264 |

Spite takes 0.25 times naive C's time and 2.23 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=39530 naive=159928 expert=17720 -->
<!-- /timings -->
