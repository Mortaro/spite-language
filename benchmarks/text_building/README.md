# Text building

A whole program, not one optimisation: one text grown by three million appends of `"item {index},"`, and a million
words joined with `", "`, against a growable buffer in C. It leans on
[appending to text in place](../../docs/optimizations.md#appending-to-text-in-place),
[a number joined into text written in place](../../docs/optimizations.md#a-number-joined-into-text-is-written-in-place)
and [short text living inside the `String`](../../docs/optimizations.md#short-text-lives-inside-the-string).

## The four forms

- [`naive/`](naive/): `built = "{built}item {index},"` three million times, then a `List<String>` of a million
  `"word{index}"` joined with `", "`, and the two lengths.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a growable byte buffer doubled as it fills,
  the digits of each number written by hand, each word `malloc`ed, and the join measured first and copied once.
- [`expert.c`](expert.c): the same work tuned by hand: each text written into one buffer reserved once, the digits
  written straight into place, and the words written where the join puts them, with no word made on its own.
- [`highlights.c`](highlights.c): the program and the list's `join`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`built = "{built}item {index},"` is three `SpiteString_append`s onto `built` itself, the number's digits written
into a buffer on the stack (`spite_long_digits`), so the text grows in place and no piece is allocated. Each word is
short enough to live inside its `String`, so the list holds the words themselves, and `List_String_join` measures
them and writes them into one text.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 148 063 | 201 728 |
| naive C: `naive.c`, `clang -O2` | 80 913 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 9 314 | 139 264 |

Spite takes 1.83 times naive C's time and 15.90 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=148063 naive=80913 expert=9314 -->
<!-- /timings -->
