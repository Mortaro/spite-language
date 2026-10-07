# Objects of one class sit together

Every object of a class a list holds is made from that class's own pool: blocks the size of one object, side by
side, handed out in order and taken back for the class's next object. So objects of two classes made in turn do not
interleave in memory, and a loop over one class's list reads its memory in order.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#objects-of-one-class-sit-together).
- The proof: [Objects a list holds, made on one thread](../../docs/proofs.md#objects-a-list-holds-made-on-one-thread).

## The four forms

- [`naive/`](naive/): 300 000 points and 300 000 notes made in turn, each kept in its own list, then 30 rounds
  that add up the points past a bound that rises with the round.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per point and per note in turn, so the C library puts each note between two points, and a growable array of
  pointers per list.
- [`expert.c`](expert.c): the same work tuned by hand: the points as two columns of numbers, the notes as an array
  of their own, and each round's sum one branchless pass over the columns.
- [`highlights.c`](highlights.c): `Point`'s pool, the allocation of a point and of a note, and the loop that sums.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Point___allocate` and `Note___allocate` take their object from `Point___pool_take()` and `Note___pool_take()`, not
from `SPITE_MALLOC`: the next block of the class's own run (`Point___pool_next`), or the last one given back.
`Point___pool_grow` makes each run, doubling it up to 256 KiB and starting it on a cache line, and
`Point___pool_give` takes a point back with two writes. So in `Naive_sum_points` the pointers in `points` lead to
points side by side, never past a note. `naive.c` reads the same pointers into blocks `malloc` handed out in turn
with the notes; `expert.c` reads no pointers at all.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 13 272 | 182 784 |
| naive C: `naive.c`, `clang -O2` | 27 335 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 2 756 | 139 776 |

Spite takes 0.49 times naive C's time and 4.82 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=13272 naive=27335 expert=2756 -->
<!-- /timings -->
