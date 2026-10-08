# An `Items`' storage is chosen while compiling

`Items<T>` is one class whose every body that touches an item folds on `$element_type.is_fixed_size`: for a `T`
that fits a `Vector`, only the inline branches are compiled, and the items are held in one block like a `Vector`'s;
for any other `T`, only the reference branches, and it holds pointers like a `List`. So one generic `Column` serves
every component, and each gets the storage that suits it, with no test of which while the program runs.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-items-storage-is-chosen-while-compiling).
- The proof: [Whether a class fits a `Vector`](../../docs/proofs.md#whether-a-class-fits-a-vector).

## The four forms

- [`naive/`](naive/): a generic `Column<$component_type>` holding an `Items<$component_type>`, made for `Velocity`
  (two numbers) and for `Trail` (two numbers and a `List` of marks, so it does not fit a `Vector`), 100 000 of
  each, and 100 ticks that each run `each_integrate()` and `sum_across()` over both columns.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the generic column written
  once, as a C programmer writes a generic container, a growable array of `void*` with each component `malloc`ed,
  whatever its class.
- [`expert.c`](expert.c): the same work tuned by hand: each component's numbers in columns, a trail's empty list
  inline beside it, and one vectorised pass per component per tick that integrates and adds up together.
- [`highlights.c`](highlights.c): `Items__Velocity`'s layout, both `append`s, both `each_integrate`s, and the read
  of an inline item.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`struct Items__Velocity` holds a block (`items_`) and a count, beside the two helper singletons `inline_` and
`references_`. `Items__Velocity_append` calls only `InlineMemory__Velocity_write_item`, which copies the velocity's
attributes into the block; `Items__Trail_append` calls only `TypedMemory__Trail_write_value`, which stores the
trail's pointer, retained. Neither has an `if` on which kind it is: that `if` was decided while compiling, and the
branch not taken is not in the C. Reading follows the same choice: `Items__Velocity_each_integrate` takes each item
with `InlineMemory__Velocity_item_at`, an address computed from the index (each item's attributes
`sizeof(Velocity) - sizeof(SpiteHeader)` bytes after the last, with no header of its own), while
`Items__Trail_each_integrate` reads `((Trail**)(intptr_t)self->items_)[index_]`, uncounted.

`naive.c` stores every component by pointer, so its velocities are 100 000 separate objects to follow; Spite's are
one block. Spite is close to `naive.c` overall: the trail column costs both the same, and the velocity column's
gain is spent in `sum_across` and `each_integrate` being two loops, each with its checked sums. `--debug-memory`
counts 300 010 allocations: the 100 000 trails and their 100 000 lists of marks, and 100 000 velocities each made
as an object by `Velocity(...)` in `fill` and then copied into the block and freed, a temporary the block could
have been written in place of.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 38 758 | 185 856 |
| naive C: `naive.c`, `clang -O2` | 65 374 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 007 | 139 776 |

Spite takes 0.59 times naive C's time and 12.89 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=38758 naive=65374 expert=3007 -->
<!-- /timings -->
