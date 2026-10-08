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
`Despawns_marked` directly, the function value folded into the walk. `Naive_fill` makes each `Velocity` in its frame with `Velocity___make_into(&spite_slot_<n>, ...)` and
`Items__Velocity_append` copies it into the block ([other optimisations](../../docs/optimizations.md#other-optimisations)),
where it made each one on the heap and freed it when the case took 3.57 times naive C's time. What is left is the
`Items`' own append (its room check, the copy and the count kept on the item), the checked steps of the walks, and
the swap the removal does for each item it keeps where `naive.c` copies it down.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 138 269 | 187 904 |
| naive C: `naive.c`, `clang -O2` | 47 681 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 30 247 | 140 288 |

Spite takes 2.90 times naive C's time and 4.57 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=138269 naive=47681 expert=30247 -->
<!-- /timings -->
