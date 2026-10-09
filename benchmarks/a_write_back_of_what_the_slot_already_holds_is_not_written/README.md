# A write-back of what the slot already holds is not written

`desk.take(index)` copies a page out of the desk's list into its `open` attribute, the caller changes the page, and
`desk.put_back()` stores `open` into the slot again. The compiler follows the value from the read in one function
through the attribute and the index to the write in another, proves the slot still holds that page, and leaves the
call to `put_back` out.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-write-back-of-what-the-slot-already-holds-is-not-written).
- The proof: [A value followed through calls is the slot it was read from](../../docs/proofs.md#a-value-followed-through-calls-is-the-slot-it-was-read-from).

## The four forms

- [`naive/`](naive/): a `Desk` singleton holding 100 000 pages; a hundred passes, each taking every page into
  `open`, adding one word to it and putting it back.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: an array of pointers, `take`
  and `put_back` as functions, the write-back one store of the same pointer after a bounds check.
- [`expert.c`](expert.c): the same work tuned by hand: only the word counts are read, so they are one array of
  numbers that each pass adds one to.
- [`highlights.c`](highlights.c): the loop of `edit_every_page`, and `take`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The inner loop of `Naive_edit_every_page` calls `Desk_take` and changes the words, and then goes straight on to
`index_ = (index_ + 1)`: there is no call for `desk.put_back()`, and no `Desk_put_back` in `generated.c` at all.
Without the optimisation each pass called `Desk_put_back`, which took the desk's guard, checked the index, loaded the
slot and compared it with `open` before finding nothing to store. What is left against `naive.c` is `take`'s:
the desk's guard on every outside access, and the counted copy into `open` (a count up for the new page and down
for the old one).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 25 999 | 203 264 |
| naive C: `naive.c`, `clang -O2` | 5 752 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 782 | 139 264 |

Spite takes 4.52 times naive C's time and 33.25 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=25999 naive=5752 expert=782 -->
<!-- /timings -->
