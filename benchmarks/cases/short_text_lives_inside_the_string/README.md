# Short text lives inside the `String`

A `String` is sixteen bytes wherever it is kept, and text of up to 15 bytes is kept in those sixteen bytes
themselves: no allocation, no reference count and no pointer to follow. Longer text is one block on the heap that
the sixteen bytes point at, and written text points into the program.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#short-text-lives-inside-the-string).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the form is decided by the length when
  the text is made, and every reader asks the form.

## The four forms

- [`naive/`](naive/): 20 rounds that each make a list of 100 000 labels, `"r{round} n{index}"` (10 bytes at most),
  then add up each label's length and the code of its last character, and let the list go.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each label `malloc`ed and
  written with `snprintf`, a growable array of pointers, `strlen` for a length, and each label freed with its list.
- [`expert.c`](expert.c): the same work tuned by hand: one array of 16-byte slots made once, each label written
  into its slot digit by digit with its length beside it, nothing else allocated.
- [`generated.c`](generated.c): the `String` and its forms, the join, the release, and what the compiler writes
  for `make_labels` and `checksum`.

## What to look at in generated.c

`struct SpiteString` is two words, `_bytes_` and `_length_`, and the top byte of `_length_` is the form:
`SPITE_STRING_IS_INLINE` when it is 15 or less (the characters are the sixteen bytes themselves, and the last byte
is 15 minus the length, so a text of exactly 15 ends in the 0 C wants), `SPITE_STRING_IS_HEAP` for a block,
`SPITE_STRING_IS_CONSTANT` for written text. `spite_string_join` adds up the pieces' lengths first and, at 15 or
fewer, copies them into `made` on the stack and returns it by value: no block. In `Naive_make_labels` the two
numbers are written into `char spite_temp_digits[24]` on the stack (`SPITE_STATIC_STRING`), joined with the two
written pieces, and the sixteen bytes stored in the list; `SpiteString___release` returns at once for any text that
is not a heap block, so `Naive_checksum___held_0` releasing each label it reads costs one compare.

`--debug-memory` counts 46 allocations for the whole run (the lists' buffers and the program's own objects), where
`naive.c` makes 2 000 000 labels with a `malloc` and a `free` each. `expert.c` is still about three times faster:
its labels are written once into a buffer made once, where Spite grows each round's list from empty and reads
`labels[index]` through `List_String_get_at`, its count read on every turn (a list of text is not one of the
plain values whose loop reads its count once), and keeps every operation's overflow check.

## Timings

<!-- timings -->
<!-- /timings -->
