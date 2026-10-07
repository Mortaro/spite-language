# Objects of one class sit together

Every object of a class a list holds is made from that class's own pool: blocks the size of one object, side by
side, handed out in order and taken back for the class's next object. So objects of two classes made in turn do not
interleave in memory, and a loop over one class's list reads its memory in order.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#objects-of-one-class-sit-together).
- The proof: [Objects a list holds, made on one thread](../../../docs/proofs.md#objects-a-list-holds-made-on-one-thread).

## The four forms

- [`naive/`](naive/): 300 000 points and 300 000 notes made in turn, each kept in its own list, then 30 rounds
  that add up the points past a bound that rises with the round.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per point and per note in turn, so the C library puts each note between two points, and a growable array of
  pointers per list.
- [`expert.c`](expert.c): the same work tuned by hand: the points as two columns of numbers, the notes as an array
  of their own, and each round's sum one branchless pass over the columns.
- [`generated.c`](generated.c): `Point`'s pool, the allocation of a point and of a note, and the loop that sums.

## What to look at in generated.c

`Point___allocate` and `Note___allocate` take their object from `Point___pool_take()` and `Note___pool_take()`, not
from `SPITE_MALLOC`: the next block of the class's own run (`Point___pool_next`), or the last one given back.
`Point___pool_grow` makes each run, doubling it up to 256 KiB and starting it on a cache line, and
`Point___pool_give` takes a point back with two writes. So in `Naive_sum_points` the pointers in `points` lead to
points side by side, never past a note. `naive.c` reads the same pointers into blocks `malloc` handed out in turn
with the notes; `expert.c` reads no pointers at all.

## Timings

<!-- timings -->
<!-- /timings -->
