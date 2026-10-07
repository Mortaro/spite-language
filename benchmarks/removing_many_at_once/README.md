# Removing many at once

The library's `remove_where` against the same removal in C: 200 000 velocities in an `Items` and their entities in a
`List<Integer>`, half and then nine in ten of them removed, forty rounds
([collections.md](../../docs/collections.md#removing-many-at-once)).

## The four forms

- [`naive/`](naive/): a `Despawns` singleton that marks entities, and forty rounds that mark half or nine in ten of
  200 000, fill the two columns, remove the marked ones with `velocities.remove_where_despawned()` and
  `entities.remove_where(despawns.marked)`, and add up what is left.
- [`naive.c`](naive.c): the same program as a C programmer writes it: growable arrays of a `Velocity` struct, of
  entities and of marks, and each removal a function that walks its array once and moves each element that stays down
  to the next free place, the entities' one calling the test it is handed.
- [`expert.c`](expert.c): the same work tuned by hand: the velocities as columns in arrays made once at their full
  size, the marks as bytes, and each removal one branchless pass.
- [`highlights.c`](highlights.c): the program, the fill, both removals and the sum.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Both removals are one walk: `Items__Velocity_remove_where_despawned` tests each item in place and swaps the ones that
stay down to the first place not kept, then truncates, and `List_Integer_remove_where_marked_for_despawns` calls
`Despawns_marked` directly, the function value folded into the walk. Most of the time is not the removal but the
fill: `Naive_fill` makes each `Velocity` on the heap with `Velocity___make` and copies it into the `Items`, where
`naive.c` writes the struct in place (the planned fix is in
[other optimisations](../../docs/optimizations.md#other-optimisations)).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 90 684 | 189 440 |
| naive C: `naive.c`, `clang -O2` | 25 423 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 21 533 | 140 288 |

Spite takes 3.57 times naive C's time and 4.21 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=90684 naive=25423 expert=21533 -->
<!-- /timings -->
