# An item a name holds from its list is not counted

`var column = columns[index]` would count the item up when it is read and down when the block ends. While nothing
in the block can write `columns`, the list holds the item the whole time, so the name is the item's address with no
count, and a call it is passed to takes it uncounted too.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-item-a-name-holds-from-its-list-is-not-counted).
- The proof: [An item a name holds from its list](../../docs/proofs.md#an-item-a-name-holds-from-its-list-is-not-counted),
  and for the call, [Held arguments](../../docs/proofs.md#an-argument-its-caller-holds-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): the section's program scaled up: 100 000 columns and a list of 100 000 weights, and 100
  rounds that `visit` every index: check the place, name the column, call `hit()` on it and store what
  `weight(column)` answers into the weights.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per column, `malloc`
  per column, a growable array of pointers for the columns and one of numbers for the weights, a function per Spite
  function called the same way. C keeps no counts, so the name is the pointer from the slot.
- [`expert.c`](expert.c): the same work tuned by hand: hits, steps and weights as three columns of numbers, a round
  one loop that adds each step and writes each weight, which the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `visit` and the `weight` it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_visit`, `column_` is `((Column**)(intptr_t)(spite_temp_3)->items_)[spite_temp_4]` after its bounds check:
no `Column___retain` on the read and no `Column___release` at the end of the function. `weight` is compiled as
`Naive_weight___held_0`, the copy that takes its argument as held by the caller, so the call counts nothing either.
The `crash columns[index]` above it tests the slot in place ([A list item read only to test
it](../a_list_item_read_only_to_test_it_is_not_counted/)). Without the optimisation the Spite would count the column
up on the read and down when `visit` ends: two writes to each column's object per visit. `naive.c` does what the
Spite now does, since C has no counts; `expert.c` has no objects to name.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 14 709 | 206 336 |
| naive C: `naive.c`, `clang -O2` | 14 639 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 1 686 | 139 776 |

Spite takes 1.00 times naive C's time and 8.72 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=14709 naive=14639 expert=1686 -->
<!-- /timings -->
