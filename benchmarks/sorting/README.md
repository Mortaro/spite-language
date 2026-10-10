# Sorting

A whole program, not one optimisation: the same quicksort of two million numbers in a `List<Integer>` as in an
`int32` array in C, then a checksum and a test that the list is in order. It leans on
[an argument its caller holds being passed uncounted](../../docs/optimizations.md#an-argument-its-caller-holds-is-passed-without-counting)
and on [arithmetic checked in every build](../../docs/optimizations.md#arithmetic-is-checked-in-every-build).

## The four forms

- [`naive/`](naive/): two million pseudo-random numbers below a million appended to a `List<Integer>`, a recursive
  quicksort on the list (middle pivot, recursing into the smaller side), with `crash numbers[...]` before each read
  that needs a number, then the checksum and the order test.
- [`naive.c`](naive.c): the same program as a C programmer writes it: the same quicksort on a `malloc`ed `int32`
  array, every read unchecked.
- [`expert.c`](expert.c): the same work tuned by hand: the numbers are below a million, so two passes of a radix sort
  on ten bits each instead of a quicksort.
- [`highlights.c`](highlights.c): the program and the sort.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_sort___held_0` takes the list uncounted, since its caller holds it. The loops are not counted loops, so every
read of the list is `List_Integer_get_at`, which tests the index, and each `crash numbers[...]` reads the item a
second time to test it before the line reads it again; every `+ 1`, `- 1` and `/ 2` is checked. `naive.c` reads its
array directly.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 141 324 | 210 432 |
| naive C: `naive.c`, `clang -O2` | 118 351 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 18 817 | 140 288 |

Spite takes 1.19 times naive C's time and 7.51 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=141324 naive=118351 expert=18817 -->
<!-- /timings -->
