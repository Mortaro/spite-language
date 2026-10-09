# A deep copy is written per class, with a table only where a graph needs one

The compiler writes one deep copy function per class `deep_copy()` reaches. A class whose attributes can lead back
to itself, or to a `Weak`, gets a copy that looks each object up in a table of what this `deep_copy()` has already
copied, so a graph keeps its shape; every other class gets a plain copy: allocate, copy each attribute, return.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-deep-copy-is-written-per-class-with-a-table-only-where-a-graph-needs-one).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); whether a class can lead back to itself
  is read off the types of its attributes while compiling, and a class that might is given the table.
- Since each round here only reads its copy, the copy is not made at all:
  [a deep copy nothing changes is the original](../../docs/optimizations.md#a-deep-copy-nothing-changes-is-the-original),
  proven by [a copy nothing changes is the original](../../docs/proofs.md#a-copy-nothing-changes-is-the-original).

## The four forms

- [`naive/`](naive/): 5 000 orders, each with its own customer (a name and a level) and four lines (a quantity and
  a price), and 40 rounds that each deep copy the whole list, add up the copies' values, and let the copy go.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct and a `malloc` per
  object, a growable array of pointers per list, each name on the heap, and a deep copy function per struct written
  by hand, with no table, since its author can see that nothing leads back.
- [`expert.c`](expert.c): the same work tuned by hand: each order holds its customer and lines inline, so the
  orders are one array of plain structs and the deep copy is one allocation and one `memcpy`.
- [`highlights.c`](highlights.c): the round loop and the function its copy calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_copy_rounds___held_0` calls `spite_copy_site_1(orders_)` where it reads `orders.deep_copy()`, and
`spite_copy_site_1` is `return List_Order___retain(self);`: the round goes on to `sum_value()` its copy, which
reads every order, its customer's level and its lines' amounts and writes nothing, and nothing in the program
compares an order, a customer or a line by identity, so the copy and the original answer every question alike and
one list is both. No order is made per round and none let go: the per class copy functions are not in the C at
all, since nothing calls them. `naive.c` copies every round, as written.

Where a copy is changed while it lives, the site calls the deep copy instead, and the per class copy is what this
case is named after: one function per class, with no table unless a class can lead back to itself. A class that
can (a `Node` with a `parent: Node?`, `conformance/stage6/deep_copy_graphs`) gets `spite_copy_table_find(self)` at
the top of its copy and `spite_copy_table_add(self, copied)` after allocating, and only that class. A copy still
gets its object from the class's `___allocate`, which makes the attributes' defaults (an `Order`'s `Customer` and
`List_Line`) and then releases them for the copies, and grows each copied list from empty: a finding, not built.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 2 041 | 210 944 |
| naive C: `naive.c`, `clang -O2` | 76 166 | 143 872 |
| expert C: `expert.c`, `clang -O2` | 1 043 | 142 336 |

Spite takes 0.03 times naive C's time and 1.96 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=2041 naive=76166 expert=1043 -->
<!-- /timings -->
