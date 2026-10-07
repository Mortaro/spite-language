# A singleton's attribute that never changes is read in place

Reading an attribute of a singleton from another class would count the attribute's object for the expression and,
where a `Parallel` reaches the singleton, take its lock around the read. When the attribute holds an object that
nothing assigns after the singleton is made, the object stays the singleton's for the rest of the program, so the
read is the attribute's address: no count and no lock.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place).
- The proof: [A singleton attribute that never changes is read in place](../../../docs/proofs.md#a-singleton-attribute-that-never-changes-is-read-in-place).

## The four forms

- [`naive/`](naive/): a `Shelf` singleton holding a `List<Box>` filled with a million boxes, and one `Parallel`
  reader that walks the shelf four times, reading `shelf.boxes[place].weight` at a place that moves by seven each
  row, and adds the weights up.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the shelf is shared with the
  reader's thread, so it sits behind a mutex, and every read through it takes the mutex and counts the box it hands
  out with an atomic operation; the box is let go once its weight is read.
- [`expert.c`](expert.c): the same work tuned by hand: the weights as one column of numbers, read on the reader's
  thread with no lock and no count.
- [`generated.c`](generated.c): the reader's loop, the list's read and its item read, and the shelf's locked
  `count`.

## What to look at in generated.c

In `Reader_read_round`, both the `crash shelf.boxes[place]` line and the read take `(self->shelf_)->boxes_`
straight from the singleton: no `spite_guard_enter`, no readers' side and no count on the `List`, though `Shelf` is
locked: `Shelf_count`, called once for `rows`, takes the readers' side (`spite_read_enter`) while the reader runs. `boxes` is assigned nowhere after `Shelf` is made, so the list stays the shelf's.

What the read still pays is the box itself: `List_Box_get_at` answers `TypedMemory__Box_read_value`, which is
`Box___retain`, and the loop calls `Box___release` once it has the weight, two atomic operations per row, since the
reader's thread counts boxes and so `Box` is counted atomically. The page's promise holds (the attribute is read in
place), but none of the uncounted list reads ([read only to test](../../../docs/optimizations.md#a-list-item-read-only-to-test-it-is-not-counted),
[held by a name](../../../docs/optimizations.md#an-item-a-name-holds-from-its-list-is-not-counted)) applies to a
`list[place].weight` read through a singleton's attribute, so the count of the item is most of what is left
between the Spite and `expert.c`. `naive.c` takes a mutex and makes the same two counts per row.

## Timings

<!-- timings -->
<!-- /timings -->
