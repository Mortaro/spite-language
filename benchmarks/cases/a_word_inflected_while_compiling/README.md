# A word inflected while compiling

`"cactus".pluralize()` called on a text written out in lower-case letters has one answer, so the compiler works it
out through the same rules and the same table of irregular words the call would read, and the program holds
`"cacti"` as a constant. Nothing is called, looked up or allocated, and a program whose only inflections are
literals carries none of the inflection code or its table.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-word-inflected-while-compiling).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the answer is the library's own, worked
  out by the compiler running the library's rules, so it is the one the call would give.

## The four forms

- [`naive/`](naive/): a million labels of a count and `"cactus"` in the plural, `"7 cacti"`, adding up their
  lengths.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `pluralize("cactus")` on
  every label, lower-casing the word into a new text, looking it up among the uncountable and irregular words and
  answering a new text, then the label made as a new text with `snprintf`, and both freed.
- [`expert.c`](expert.c): `"cacti"` written as the constant it is, and each label written once into a buffer on the
  stack, its digits by hand.
- [`generated.c`](generated.c): what the compiler writes for `label` and the loop that calls it.

## What to look at in generated.c

`Naive_label` sets `plural_` to a static text literal, `SPITE_STATIC_STRING("cacti", 5)` in the full C, where the
Spite calls `pluralize()`: there is no call, and no `String.Inflection` in the whole program. The label is one
`spite_string_join(3, ...)` of the count's digits (written into the frame by `spite_long_digits`), `" "` and the
plural, and at 8 or 9 bytes it fits inside the `String`, so the million labels allocate nothing:
`--debug-memory` counts 6 allocations for the whole run. `naive.c` makes three texts per label and searches 22
uncountable and 11 irregular words before it finds `cactus`; `expert.c` copies the five letters.

## Timings

<!-- timings -->
<!-- /timings -->
