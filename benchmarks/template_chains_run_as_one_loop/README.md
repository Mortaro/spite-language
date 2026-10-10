# Template chains run as one loop

`items.filter_is_active().map_owners().filter_is_adult().sum_age()` reads as four steps, each making a list for
the next, and that is what it means. The compiler writes the whole chain as one loop over `items` that tests, maps
and adds each element before it reads the next, so no list is made in between.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#template-chains-run-as-one-loop).
- The proof: none of its own; the elements the loop reads are not counted by
  [List templates read uncounted](../../docs/proofs.md#a-lists-templates-read-their-elements-uncounted).

## The four forms

- [`naive/`](naive/): 100 000 items, each with an owner, and 100 rounds of two chains.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per object, a growable array of pointers per list, and each step of a chain a function that makes the next list.
- [`expert.c`](expert.c): the same work tuned by hand: the items as columns of plain values, the owner's age beside
  the price, both sums of a round in one branchless pass.
- [`highlights.c`](highlights.c): what the compiler writes for `totals` and the two chains.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`totals` calls `List_Item_filter_is_active_then_sum_price` and
`List_Item_filter_is_active_then_map_owners_then_filter_is_adult_then_sum_age`: each chain is one function with one
`while` over `items`, an `if` per `filter_`, a local for the `map_`, and the sum's addition in the innermost `if`.
Nothing is allocated, and each element is read as it lies in the list, `((Item**)(intptr_t)self->items_)[index_]`,
without a count. `naive.c` makes three lists per round for the second chain and frees them; `expert.c` reads three
arrays of numbers and no pointers.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 11 029 | 203 776 |
| naive C: `naive.c`, `clang -O2` | 73 512 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 1 578 | 139 776 |

Spite takes 0.15 times naive C's time and 6.99 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=11029 naive=73512 expert=1578 -->
<!-- /timings -->
