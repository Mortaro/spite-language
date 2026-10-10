# A number joined into text is written in place

`"line {index} of {round};"` reads as turning each number into a text and joining the pieces. The compiler writes
each `Integer` or `Long` piece as digits into a buffer in the function's own frame and joins from there, so no text
is made for a number.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-number-joined-into-text-is-written-in-place).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the digits are the ones the library's
  `to_string()` writes, and a program that reopens `Integer` or `Long` with its own `to_string()` keeps calling it.

## The four forms

- [`naive/`](naive/): 20 rounds of 100 000 lines `"line {index} of {round};"`, adding up their lengths.
- [`naive.c`](naive.c): the same program as the Spite reads: each number made into a text of its own (`malloc`
  and `snprintf`), the five pieces joined into a new text, and all three freed.
- [`expert.c`](expert.c): each line written into one buffer on the stack, its digits straight into it, nothing
  allocated.
- [`highlights.c`](highlights.c): what the compiler writes for `lines`, and the digit writer it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The line is one statement: `char spite_temp_1_digits[24]` and `spite_long_digits(...)` write `index` into the frame,
`SPITE_STATIC_STRING` makes a text that points at those digits without copying them, the same for `round`, and
`spite_string_join(5, ...)` makes the line once at its final length (a line of 15 bytes or fewer is kept inside its
sixteen bytes and allocates nothing). `naive.c` makes three texts per line, two of them only to copy them into the
third; `expert.c` makes none and only writes bytes.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 70 870 | 197 120 |
| naive C: `naive.c`, `clang -O2` | 306 942 | 142 336 |
| expert C: `expert.c`, `clang -O2` | 15 066 | 139 776 |

Spite takes 0.23 times naive C's time and 4.70 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=70870 naive=306942 expert=15066 -->
<!-- /timings -->
