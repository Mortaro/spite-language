# Plain reference counts where no thread reaches a class

A program that starts a thread needs atomic counts only for the objects another thread can count, and most of its
classes are never seen there. The compiler finds the code that can run on another thread, follows every call it
makes, and counts every class that code never retains or releases with a plain addition and subtraction.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class).
- The proof: [No other thread counts a class](../../../docs/proofs.md#no-other-thread-counts-a-class).

## The four forms

- [`naive/`](naive/): one `Parallel` sums 20 million numbers on a worker while the program's own thread makes
  200 000 points and, 20 times over, keeps the ones past a bound in a new list, counting each kept point once
  more and once less.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per object, a growable array of pointers per list, the summer on a thread of its own, and every object counted
  with an atomic operation. That is how a reference-counted program with threads is written in C when nobody has
  worked out which objects stay on one thread, so it is the effort the Spite program asks for: counting a point
  plainly is a decision about the whole program that the C programmer would have to make, and keep true, by hand.
  The walk reads each point from the list without counting it, as a C programmer borrows it.
- [`expert.c`](expert.c): the same work tuned by hand: the points as two columns of numbers with no count at all,
  each round's kept list a list of positions written branchlessly into one buffer, the summer on its own thread.
- [`generated.c`](generated.c): what the compiler writes for a point's retain and release, the summer's release,
  and the loop that keeps the points.

## What to look at in generated.c

`Point___retain` and `Point___release` count with `SPITE_PLAIN_COUNT_UP` and `SPITE_PLAIN_COUNT_DOWN`, a plain
addition and subtraction, while `Summer___release` keeps `SPITE_COUNT_DOWN`, the atomic one, since the worker lets
go of the summer when it is done. In `Naive_count_near___held_0`, each kept point is counted once by
`Point___retain` as it is appended, and `List_Point___release(kept_)` counts every one down at the end of the round;
the read `points[index]` itself is not counted. `naive.c` makes the same two counts per kept point with
`__atomic_add_fetch` and `__atomic_sub_fetch`, each a locked instruction; `expert.c` makes none.

## Timings

<!-- timings -->
<!-- /timings -->
