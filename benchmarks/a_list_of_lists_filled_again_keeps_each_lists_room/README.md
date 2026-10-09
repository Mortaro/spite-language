# A list of lists filled again keeps each list's room

A list of lists whose lists live in its slots keeps each slot's block of items when it is cleared, and the next
empty list put in that slot takes the block instead of growing from nothing.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-list-of-lists-filled-again-keeps-each-lists-room).
- The proof it rests on: [A list held only in a slot of another list](../../docs/proofs.md#a-list-held-only-in-a-slot-of-another-list).

## The four forms

- [`naive/`](naive/): 200 000 orders, each with a customer among 32 768; forty rounds that group the orders into one
  list per customer (shifted by the round), made afresh each round, and add up the size of the largest group.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each bucket list `malloc`ed and
  freed every round, each growing its array from nothing.
- [`expert.c`](expert.c): the same answer tuned by hand: only the groups' sizes are asked, so each round counts the
  orders per customer into one array cleared in place and takes the largest; no group is built.
- [`highlights.c`](highlights.c): the list of lists' `clear`, `drop` and growth, the slot's write, and the grouping.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`List_List_Order_clear` lets go of each list's orders with `List_Order_clear` and marks the slot (`ref_count` -7),
keeping its block; `TypedMemory__List_Order_write_value` gives a marked slot's block to the empty list put there,
and frees it when the list put there has items of its own; `List_List_Order__grow` zeroes the room it adds, so a
slot past the count is either marked or empty; `List_List_Order_drop` frees every marked slot's block. In
`Naive_group` the fresh `List<Order>()` of each slot finds its block from the round before, so the orders are
appended without a single allocation after the first round.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 121 142 | 207 872 |
| naive C: `naive.c`, `clang -O2` | 342 006 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 6 829 | 139 264 |

Spite takes 0.35 times naive C's time and 17.74 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=121142 naive=342006 expert=6829 -->
<!-- /timings -->
