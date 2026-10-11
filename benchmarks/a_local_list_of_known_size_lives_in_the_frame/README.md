# A local list of known size lives in the frame

`var remainders = [round % 10, round % 7, round % 13, round % 3]` and `var weights = [4, 3, 2, 1]` read as two
lists made on the heap at every call. Both have a size known while compiling and are only read, so their headers
and items are in the function's own frame, and the weights, all constants, are read-only data of the program that
nothing writes while it runs.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-local-list-of-known-size-lives-in-the-frame).
- The proof: [Local and variadic lists in the frame](../../docs/proofs.md#local-and-variadic-lists-in-the-frame).

## The four forms

- [`naive/`](naive/): 10 000 000 calls of `largest_remainder(round)`, each making the two lists of four numbers and
  answering the largest remainder times its weight.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each list a `malloc` for the
  struct and one for its numbers, filled by appending, read and freed.
- [`expert.c`](expert.c): the same work tuned by hand: the remainders in an array on the stack, the weights a
  `static const` array, the largest taken with no branch.
- [`highlights.c`](highlights.c): what the compiler writes for `largest_remainder`, and the function that sets up a
  list header over items it is handed.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_largest_remainder` starts with `int32_t spite_framed_<n>_items[4];`, the four remainders written into it,
and `List_Integer spite_framed_<n>;`, the header, both locals; `List_Integer___framed` points the header at the
items. The weights are `static const int32_t spite_framed_<n>_items[4] = { 4, 3, 2, 1 };`, in the program's
constant data, with only their header in the frame. Nothing is allocated: under `--debug-memory` the whole program
makes 6 allocations, none of them in the loop.

## Why the times are alike

clang does the same for `naive.c` on its own: at `-O2` it inlines `largest_remainder` and the list functions, sees
each `malloc` freed in the same function with nothing escaping, and removes all four (`clang -O2 -S` of `naive.c`
has no call to `malloc` left). So the three programs take about the same time, the four divisions of each call
being most of it. The Spite program does not leave this to clang: without this optimisation each list would be
two allocations (the object and its buffer), made through `List_Integer_append` and the heap's `resize` and let go
through the list's reference count and `drop`, four allocations and four frees per call that the section counts
under `--debug-memory`, whatever the C compiler later makes of them. What is left in
`highlights.c` that `expert.c` does not do is the `crash weights[index]` line, read through `List_Integer_get_at`
twice (once to test it, once for the value) where the remainders are read straight from their items.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 23 071 | 198 144 |
| naive C: `naive.c`, `clang -O2` | 22 965 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 22 502 | 139 264 |

Spite takes 1.00 times naive C's time and 1.03 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=23071 naive=22965 expert=22502 -->
<!-- /timings -->
