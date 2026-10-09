# A deep copy is written per class, with a table only where a graph needs one

The compiler writes one deep copy function per class `deep_copy()` reaches. A class whose attributes can lead back
to itself, or to a `Weak`, gets a copy that looks each object up in a table of what this `deep_copy()` has already
copied, so a graph keeps its shape; every other class gets a plain copy: allocate, copy each attribute, return.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-deep-copy-is-written-per-class-with-a-table-only-where-a-graph-needs-one).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); whether a class can lead back to itself
  is read off the types of its attributes while compiling, and a class that might is given the table.

## The four forms

- [`naive/`](naive/): 5 000 orders, each with its own customer (a name and a level) and four lines (a quantity and
  a price), and 40 rounds that each deep copy the whole list, add up the copies' values, and let the copy go.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct and a `malloc` per
  object, a growable array of pointers per list, each name on the heap, and a deep copy function per struct written
  by hand, with no table, since its author can see that nothing leads back.
- [`expert.c`](expert.c): the same work tuned by hand: each order holds its customer and lines inline, so the
  orders are one array of plain structs and the deep copy is one allocation and one `memcpy`.
- [`highlights.c`](highlights.c): the round loop, the five copy functions, and what making an `Order` sets.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`List_Order___deep_copy` makes a list and appends `Order___deep_copy` of each element; `Order___deep_copy`
allocates, copies `id_`, and sets `customer_` and `lines_` to `Customer___deep_copy` and `List_Line___deep_copy`
of the originals; `Line___deep_copy` copies two numbers; `Customer___deep_copy` retains the name, which is short
text and so copied with the `String` itself. None of them looks anything up: no class here can reach itself, so
`spite_copy_table_find` and its table are not in the C at all. A class that can (a `Node` with a `parent: Node?`,
`conformance/stage6/deep_copy_graphs`) gets `spite_copy_table_find(self)` at the top of its copy and
`spite_copy_table_add(self, copied)` after allocating, and only that class. This is the code `naive.c` writes by
hand.

What the generated copy does that `naive.c` does not, a finding of this case: `Order___deep_copy` gets its object
from `Order___allocate`, which runs `Order___init`, and `Order___init` makes the attributes' defaults, a
`Customer___make(...)` and a `List_Line___make()`; the copy then releases both and puts the copies in their place.
So each copied order makes two objects it throws away: `--debug-memory` counts 2 040 088 allocations for the run
where 1 640 088 would do. A constructor's own call skips the defaults it replaces
([defaults the constructor replaces are never made](../../docs/optimizations.md#defaults-the-constructor-replaces-are-never-made));
a deep copy does not yet. Each copied list also grows from empty by `append` rather than being made at its size.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 46 039 | 209 920 |
| naive C: `naive.c`, `clang -O2` | 74 072 | 143 872 |
| expert C: `expert.c`, `clang -O2` | 998 | 142 336 |

Spite takes 0.62 times naive C's time and 46.13 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=46039 naive=74072 expert=998 -->
<!-- /timings -->
