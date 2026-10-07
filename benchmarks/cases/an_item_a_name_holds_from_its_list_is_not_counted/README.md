# An item a name holds from its list is not counted

`var column = columns[index]` would count the item up when it is read and down when the block ends. While nothing
in the block can write `columns`, the list holds the item the whole time, so the name is the item's address with no
count, and a call it is passed to takes it uncounted too.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#an-item-a-name-holds-from-its-list-is-not-counted).
- The proof: [An item a name holds from its list](../../../docs/proofs.md#an-item-a-name-holds-from-its-list-is-not-counted),
  and for the call, [Held arguments](../../../docs/proofs.md#an-argument-its-caller-holds-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): the section's program scaled up: 100 000 columns and a list of 100 000 weights, and 100
  rounds that `visit` every index: check the place, name the column, call `hit()` on it and store what
  `weight(column)` answers into the weights.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per column, `malloc`
  per column, a growable array of pointers for the columns and one of numbers for the weights, a function per Spite
  function called the same way. C keeps no counts, so the name is the pointer from the slot.
- [`expert.c`](expert.c): the same work tuned by hand: hits, steps and weights as three columns of numbers, a round
  one loop that adds each step and writes each weight, which the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `visit` and the `weight` it calls.

## What to look at in generated.c

In `Naive_visit`, `column_` is `((Column**)(intptr_t)(spite_temp_3)->items_)[spite_temp_4]` after its bounds check:
no `Column___retain` on the read and no `Column___release` at the end of the function. `weight` is compiled as
`Naive_weight___held_0`, the copy that takes its argument as held by the caller, so the call counts nothing either.
The `crash columns[index]` above it tests the slot in place ([A list item read only to test
it](../a_list_item_read_only_to_test_it_is_not_counted/)). Without the optimisation the Spite would count the column
up on the read and down when `visit` ends: two writes to each column's object per visit. `naive.c` does what the
Spite now does, since C has no counts; `expert.c` has no objects to name.

## Timings

<!-- timings -->
<!-- /timings -->
