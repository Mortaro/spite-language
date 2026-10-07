# A variadic list the callee only reads lives in the caller's frame

The `...values` of a call such as `largest(round % 10, round % 7, round % 13, round % 3)` arrive in a `List`, which
reads as a list made on the heap for every call. When the function called only reads its list and the call is a
statement of its own, the list and its items are put in the caller's frame instead and nothing is allocated.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame).
- The proof: [Local and variadic lists in the frame](../../../docs/proofs.md#local-and-variadic-lists-in-the-frame).

## The four forms

- [`naive/`](naive/): 2 000 000 calls of a variadic function of the program's own, `largest(...values)`, each with
  four remainders, adding up the answers.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each call makes a list (a
  `malloc` for the struct, a growable array of numbers), appends the four values, and frees it after the call.
- [`expert.c`](expert.c): the same work tuned by hand: the four values in an array on the caller's stack, passed
  with their count.
- [`generated.c`](generated.c): what the compiler writes for `all_rounds` and `largest`.

## What to look at in generated.c

**This program is written the obvious way, and the optimisation does not apply to it.** `largest` only reads its
list, but its answer is used, `var biggest = largest(...)`, and a call whose answer is assigned is not a statement
of its own in the compiler's sense, so the list is made on the heap at every call. The case is kept to show it.

In `Naive_all_rounds` the argument of `Naive_largest` is
`({ List_Integer* spite_temp_<n> = List_Integer___make(); List_Integer_append(spite_temp_<n>, (round_ % 10)); ... })`:
a list on the heap, grown by its first `append`, and `Naive_largest` ends with `List_Integer___release(values_)`.
Under `--debug-memory` the program makes 4 000 006 allocations, two per call. `Naive_largest` itself reads the
items straight from the list's buffer (`spite_temp_<n>[index_]`), so what it costs is the list, not the reads.

`naive.c` makes the same list per call, but clang at `-O2` inlines `largest` and the list functions, sees each
`malloc` freed in the same function, and removes them (`clang -O2 -S` of `naive.c` has no call to `malloc` left),
so `naive.c` runs as fast as `expert.c`. The Spite program's list goes through `List_Integer___make`, the heap's
`resize` and a reference count, which clang does not remove.

The same program written with the call as a statement of its own, `add_largest(round % 10, round % 7, round % 13,
round % 3)` adding the answer into an attribute instead of returning it, does get the frame list:
`List_Integer spite_framed_<n>; int32_t spite_framed_<n>_items[4];` in the caller, the four values written into
the items, and no allocation (6 in the whole program under `--debug-memory`). Built `--optimized` and run once on
this machine, it took 6 ms where `naive/` took 205 to 267 ms. The rule the section states, a call that is a
statement of its own, keeps `var biggest = largest(...)`, the way most programs call a function that answers
something, on the heap.

## Timings

<!-- timings -->
<!-- /timings -->
