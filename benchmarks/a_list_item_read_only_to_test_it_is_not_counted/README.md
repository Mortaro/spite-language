# A list item read only to test it is not counted

`if entries[at]` reads an item only to ask whether it is there, and read as a value it would be counted up for the
test and down again right after it. Nothing runs between the read and the test, so the compiler tests the slot
itself: the index inside the list and a pointer in the slot, without touching the item's object.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-list-item-read-only-to-test-it-is-not-counted).
- The proof: [A list item read only to test it](../../docs/proofs.md#a-list-item-read-only-to-test-it-is-not-counted).

## The four forms

- [`naive/`](naive/): 200 000 entries in a list and 20 000 000 probes at places that step by 7919 through twice
  the list's length, each `if entries[at]` counting a found or a missing entry, so half the tests find one.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per entry, `malloc`
  per entry, a growable array of pointers and a `list_get` that answers `NULL` outside the list, tested against
  `NULL`. C keeps no counts, so its test reads only the slot already.
- [`expert.c`](expert.c): the same work tuned by hand: the keys as a column, a byte per place saying whether an
  entry is there, the probe's remainder kept by a subtraction instead of a division, and each test one unsigned
  compare and one byte read.
- [`highlights.c`](highlights.c): what the compiler writes for `probe`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The `if` in `Naive_probe` is `(spite_temp_5 >= 0 && spite_temp_5 < (spite_temp_4)->item_count_) &&
((((Entry**)(intptr_t)(spite_temp_4)->items_)[spite_temp_5]) != 0)`: the index and the slot, and no call. Without the
optimisation the Spite would read the item through `List_Entry_get_at` (a count up on the entry's object), test it
and release it (a count down): two writes to an object at a place the probes jump to, a cache line of its own
each, for every probe that finds an entry. `naive.c` does what the Spite now does, a bounds check and a pointer
compare, since C has no count to keep; `expert.c` reads a byte array a quarter of the pointers' size and divides
nothing.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 58 596 | 180 736 |
| naive C: `naive.c`, `clang -O2` | 70 666 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 12 480 | 139 776 |

Spite takes 0.83 times naive C's time and 4.70 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=58596 naive=70666 expert=12480 -->
<!-- /timings -->
