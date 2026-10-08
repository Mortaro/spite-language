# Records sorted by one field

A data-oriented case (not an optimisation the compiler makes yet): two million orders of eight fields in a
`List<Order>`, sorted by the time each was placed with `sort_by_placed_at()`, then walked in that order twice: a
running total that reads two fields and an audit sum that reads four. Every time is distinct, so every correct sort
gives the same order. It is one of the measurements behind
[design/proposals/data_oriented_layout.md](../../design/proposals/data_oriented_layout.md), and the case where the
best layout depends on what follows the sort: moving whole records, sorting keys and gathering fields on every
walk, or sorting keys and gathering whole records once.

## The forms

- [`naive/`](naive/): an `Order` class of eight fields in a `List<Order>`, `orders.sort_by_placed_at()`, a `while`
  for the running total and `sum_audit()`. `--orders=N` sets how many orders.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per order, one `malloc` each, an
  array of pointers sorted with the C library's `qsort`, the same walks.
- [`expert_aos.c`](expert_aos.c): the orders inline in one array of 40-byte structs, sorted by a radix sort (three
  stable passes of 11 bits) that moves the whole structs each pass, then the walks in order.
- [`expert_soa.c`](expert_soa.c): eight columns, a radix sort of 8-byte (time, position) pairs so the orders never
  move, then the walks gather each field from its column through the position.
- [`expert.c`](expert.c): the radix sort of (time, position) pairs, then one pass that gathers each whole order into
  sorted order, then the walks over the sorted array in order.
- [`highlights.c`](highlights.c): `struct Order`, the library's `sort_by_placed_at` as compiled for `Order`, and the
  running total.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form prints the time of making the orders, sorting and walking after its `microseconds` line, and takes
`--orders=N` too.

## What to look at in highlights.c

`List_Order_sort_by_placed_at` is already the data-oriented half of the sort: it copies the keys into a list of
their own and merge-sorts a list of positions by them, so the comparisons read 8-byte keys in order, never the
orders. But the list it answers holds pointers to the same objects in the new order, so the walks after it read
objects scattered across the pool: `Naive_running_total___held_0` follows a pointer to a random place per order.
The sweep in the proposal measures that cost (a pool walked in shuffled order, up to fourteen times a walk in memory
order) and `expert.c` shows the cure: gather the records into sorted order once.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes, and with each phase apart: [cases.md](../../design/proposals/data_oriented_layout/cases.md#records_sorted_by_one_field).
