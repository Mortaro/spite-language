# A singleton's attribute that never changes is read in place

Reading an attribute of a singleton from another class would count the attribute's object for the expression and,
where a `Parallel` reaches the singleton, take its lock around the read. When the attribute holds an object that
nothing assigns after the singleton is made, the object stays the singleton's for the rest of the program, so the
read is the attribute's address: no count and no lock.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place).
- The proof: [A singleton attribute that never changes is read in place](../../docs/proofs.md#a-singleton-attribute-that-never-changes-is-read-in-place).

## The four forms

- [`naive/`](naive/): a `Shelf` singleton holding a `List<Box>` filled with a million boxes, and one `Parallel`
  reader that walks the shelf four times, reading `shelf.boxes[place].weight` at a place that moves by seven each
  row, and adds the weights up.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the shelf is shared with the
  reader's thread, so it sits behind a mutex, and every read through it takes the mutex and counts the box it hands
  out with an atomic operation; the box is let go once its weight is read.
- [`expert.c`](expert.c): the same work tuned by hand: the weights as one column of numbers, read on the reader's
  thread with no lock and no count.
- [`highlights.c`](highlights.c): the reader's loop, the list's read and its item read, and the shelf's locked
  `count`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Reader_read_round`, both the `crash shelf.boxes[place]` line and the read take `(self->shelf_)->boxes_`
straight from the singleton: no `spite_guard_enter`, no readers' side and no count on the `List`, though `Shelf` is
locked: `Shelf_count`, called once for `rows`, takes the readers' side (`spite_read_enter`) while the reader runs. `boxes` is assigned nowhere after `Shelf` is made, so the list stays the shelf's.

The box itself is read in its slot too: `shelf.boxes[place].weight` is
[an item used at once](../../docs/optimizations.md#an-item-passed-to-a-call-that-cannot-change-its-list-is-not-counted),
read for one attribute with nothing of the program's running in between, so the loop loads
`((Box**)(intptr_t)(list)->items_)[place]` after checking the index and reads `weight_` from it, with no
`Box___retain` and `Box___release` around it (before that optimisation, two atomic operations per row, since the
reader's thread counts boxes). `naive.c` takes a mutex and makes the same two counts per row.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 32 297 | 236 032 |
| naive C: `naive.c`, `clang -O2` | 179 131 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 533 | 139 776 |

Spite takes 0.18 times naive C's time and 9.14 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=32297 naive=179131 expert=3533 -->
<!-- /timings -->
