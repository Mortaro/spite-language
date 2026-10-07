# A list's templates read its elements without counting them

A member template such as `bodies.each_advance()` or `bodies.sum_position()` would read each element with a count
up and let it go with a count down, two writes to every object it walks. When nothing the pass runs can let go of
anything, the element is read as it lies in the list, uncounted.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them).
- The proof: [List templates read uncounted](../../../docs/proofs.md#a-lists-templates-read-their-elements-uncounted).

## The four forms

- [`naive/`](naive/): 200 000 bodies, each made beside a label of its own so the bodies lie spread through memory,
  and 100 rounds of `bodies.each_advance()` then `bodies.sum_position()`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per body and per label in the same order, a growable array of pointers per list, and each template a function
  that walks the list. C keeps no counts, so each element is the pointer from its slot.
- [`expert.c`](expert.c): the same work tuned by hand: positions, speeds and label numbers as columns, and each
  round's advance and sum in one loop the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `simulate` and the two templates.

## What to look at in generated.c

`List_Body_each_advance` and `List_Body_sum_position` each read `Body* item_ = ((Body**)(intptr_t)self->items_)[index_]`
and use it: no `Body___retain` before the call to `Body_advance` or the read of `position_`, and no `Body___release`
after. `advance()` only assigns a number attribute, so nothing in the pass can let go of an object. Without the
optimisation the Spite would read each element with `values.read_value(items, index)` (a count up) and release it at
the end of its pass (a count down), two writes per body per template, on bodies spread through memory. `naive.c`
reads the pointer and nothing more, which is what the Spite now does; `expert.c` walks columns of numbers and fuses
the two passes of a round into one.

## Timings

<!-- timings -->
<!-- /timings -->
