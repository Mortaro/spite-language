# A variadic list the callee only reads lives in the caller's frame

The `...values` of a call such as `largest(round % 10, round % 7, round % 13, round % 3)` arrive in a `List`, which
reads as a list made on the heap for every call. When the function called only reads its list and the call is a
statement of its own or the value of a `var`, the list and its items are put in the caller's frame instead and nothing is allocated.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame).
- The proof: [Local and variadic lists in the frame](../../docs/proofs.md#local-and-variadic-lists-in-the-frame).

## The four forms

- [`naive/`](naive/): 2 000 000 calls of a variadic function of the program's own, `largest(...values)`, each with
  four remainders, adding up the answers.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each call makes a list (a
  `malloc` for the struct, a growable array of numbers), appends the four values, and frees it after the call.
- [`expert.c`](expert.c): the same work tuned by hand: the four values in an array on the caller's stack, passed
  with their count.
- [`highlights.c`](highlights.c): what the compiler writes for `all_rounds` and `largest`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`largest` only reads its list, and its answer is the value of a `var`, `var biggest = largest(...)`, so the list
lives in the caller's frame: `Naive_all_rounds` declares `List_Integer spite_framed_<n>; int32_t
spite_framed_<n>_items[4];` inside the loop, writes the four remainders into the items, and passes
`List_Integer___framed(&spite_framed_<n>, ..., 4)`, a list over those items made with a count of 2, so
`Naive_largest`'s closing `List_Integer___release(values_)` never frees it. `Naive_largest` reads the items
straight from the list's buffer (`spite_temp_<n>[index_]`). Under `--debug-memory` the program makes 6
allocations in all.

Before a `var`'s value could frame its list, only a call that is a statement of its own did, and this program made
its list on the heap at every call: 4 000 006 allocations, and 31.6 times `naive.c`'s time. `naive.c` makes the
same list per call, but clang at `-O2` inlines `largest` and the list functions, sees each `malloc` freed in the
same function, and removes them, so `naive.c` runs as fast as `expert.c`, and now so does Spite.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 3 986 | 196 096 |
| naive C: `naive.c`, `clang -O2` | 3 828 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 3 869 | 139 264 |

Spite takes 1.04 times naive C's time and 1.03 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=3986 naive=3828 expert=3869 -->
<!-- /timings -->
