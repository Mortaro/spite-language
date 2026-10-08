# Number dictionary

A whole program, not one optimisation: a `Dictionary` of 500 000 whole-number keys and five million lookups, half of
them for keys that are not there, against an open-addressing table in C. It leans on
[a dictionary keyed by numbers hashing the numbers](../../docs/optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers).

## The four forms

- [`naive/`](naive/): `scores[index * 7] = index` for 500 000 indices, then ten rounds of looking up `index * 3`
  for 500 000 indices, written as Spite reads: `if scores[key] { total = total + scores[key] }`.
- [`naive.c`](naive.c): the same program as a C programmer writes it: an open-addressing table of `int32` keys with a
  Fibonacci hash, grown by doubling, and one lookup per key.
- [`expert.c`](expert.c): the same work tuned by hand: the table made at its final size so it never grows, the keys
  and values in two arrays with `-1` marking an empty slot, so a probe reads four bytes.
- [`highlights.c`](highlights.c): the program and the dictionary's `set_at`, `get_at` and the functions they
  call to find a slot.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The keys are numbers, so `Dictionary_Integer_by_Integer_get_at` hashes the number itself, with no text made, and
a probe compares the key kept in its slot. The lookup is made once: `if scores[key]` keeps what it found in
`spite_temp_<n>`, and `scores[key]` in the sum is `spite_temp_<n>.value`, as `naive.c` keeps its pointer. Before
both, the case took 3.79 times naive C's time. Every `index * 7`, `index * 3` and `+` is checked.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 78 997 | 185 344 |
| naive C: `naive.c`, `clang -O2` | 26 543 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 14 231 | 139 264 |

Spite takes 2.98 times naive C's time and 5.55 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=78997 naive=26543 expert=14231 -->
<!-- /timings -->
