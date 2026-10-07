# Text joined in one piece

`"{first} meets {second} at the gate."` is one join, not a chain of pairs: every piece is worked out in order and
the text is made once, at its final length. A chain of pairs would make a whole new text at every piece and throw
all but the last away.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#text-joined-in-one-piece).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the pieces are computed left to right
  as written, so the order of anything they call is the order the source reads in.

## The four forms

- [`naive/`](naive/): a thousand names, and a thousand rounds of a line for each name and the next, adding up the
  lines' lengths.
- [`naive.c`](naive.c): the same program joined a pair at a time, as `a + b + c + d` reads: three texts per line,
  two of them freed as soon as the next is made.
- [`expert.c`](expert.c): each name's length kept beside it, and each line copied once into a buffer on the stack.
- [`generated.c`](generated.c): what the compiler writes for `meetings`, and the library's `spite_string_join`.

## What to look at in generated.c

The line is one `spite_string_join(4, ...)` over the two names and the two written pieces (`spite_lit_1`,
`spite_lit_2`), which adds up the four lengths, allocates once (or keeps a text of 15 bytes or fewer inside its
sixteen bytes) and copies each piece once. `naive.c` copies the first name three times and the second twice. The
reads `names[index]` and `names[index + 1]` are proven by the loop's `index + 1 < names.count()` and test only their
bounds; since that bound is not `index < ...`, the counter's `index + 1` keeps its overflow check, which a loop over
`index < names.count()` would not have.

## Timings

<!-- timings -->
<!-- /timings -->
