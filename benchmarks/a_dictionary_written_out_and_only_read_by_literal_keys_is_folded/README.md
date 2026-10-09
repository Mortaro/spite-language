# A dictionary written out and only read by literal keys is folded

`var points = {"gold": 50, "silver": 20, "bronze": 5}` read only as `points["gold"]` and its like answers a value
known while compiling at every read. The compiler never makes that dictionary: each read is the value written for
its key, still a `T?`, so nothing is allocated or hashed when the function runs.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-dictionary-written-out-and-only-read-by-literal-keys-is-folded).
- The proof: [A dictionary only read by literal keys is never made](../../docs/proofs.md#a-dictionary-only-read-by-literal-keys-is-never-made).

## The four forms

- [`naive/`](naive/): 200 000 results of golds, silvers and bronzes, and 40 rounds adding up their points, each
  result's points worked out by `points_for`, which writes the dictionary of what a medal is worth and reads it.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per result, `malloc`
  per object, and `points_for` making a small hash table of three entries on every call, hashing each key to set it
  and again to read it, and freeing it.
- [`expert.c`](expert.c): the same work tuned by hand: the three points as constants in the sum, the results as three
  columns of plain values, each round one loop the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `points_for` and the loop that calls it.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_points_for___held_0` has no dictionary in it: each `points["gold"]` is the literal
`((Nullable_Integer){ .has_value = true, .value = 50 })`, so the three `crash` lines test a constant `has_value`
that the C compiler drops, and what is left is three multiplications and two additions with their overflow checks.
`naive.c` makes and frees a table of eight slots on every one of the 8 000 000 calls and hashes six texts for it;
`expert.c` writes 50, 20 and 5 where they are used and reads columns instead of objects.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 13 240 | 200 704 |
| naive C: `naive.c`, `clang -O2` | 311 926 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 5 577 | 139 264 |

Spite takes 0.04 times naive C's time and 2.37 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=13240 naive=311926 expert=5577 -->
<!-- /timings -->
