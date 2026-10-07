# Maths on constants is worked out while compiling

`0.5.cosine()` and `0.5.sine()` have one answer each, so the compiler works them out with the same C library
function the program would call and writes the answer into the C as a hexadecimal float. The loop that turns the
points multiplies by two constants and calls nothing.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#maths-on-constants-is-worked-out-while-compiling).
- The proof: [Maths on constants is worked out while compiling](../../../docs/proofs.md#maths-on-constants-is-worked-out-while-compiling).

## The four forms

- [`naive/`](naive/): 100 000 points, each turned by half a radian 200 times, then the sum of their `across`,
  rounded to a whole number.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per point, `malloc`
  per point, a growable array of pointers, and `turn` calling `cosf(0.5f)` and `sinf(0.5f)` where the Spite calls
  `cosine()` and `sine()`.
- [`expert.c`](expert.c): the same work tuned by hand: the two constants written out, the points as two columns of
  plain values, each round one loop the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `turn` and the `each` that calls it.

## What to look at in generated.c

`Naive_turn` multiplies by `(0x1.c152800000000p-1f)` and `(0x1.eaee880000000p-2f)`, the cosine and sine of 0.5 as
the C library answers them, and calls no `cosf` or `sinf`. `naive.c` writes the four calls; `clang -O2` folds them
itself (it knows `cosf` and `sinf` and the machine it compiles on), so at this optimisation level the two programs
do the same arithmetic and the gap between them is the `Point___retain` and `Point___release` the `each` adds per
point. What the fold is worth shows with a C compiler that does not know the library: `naive.c` built with
`clang -O2 -fno-builtin` took 178 ms against 14 ms without the flag (one run each on a busy machine), while the
Spite program's C keeps its constants whatever compiles it. `expert.c` writes the same two constants by hand.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 15 243 | 178 688 |
| naive C: `naive.c`, `clang -O2` | 16 829 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 285 | 139 776 |

Spite takes 0.91 times naive C's time and 4.64 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=15243 naive=16829 expert=3285 -->
<!-- /timings -->
