# A proven divisor is not checked

A whole-number `/` or `%` halts naming its line when the divisor is zero, so it carries a compare and a branch
before it. Where the divisor is a constant other than zero, or a proof in scope says it is not zero (here
`crash parts > 0` at the top of `shares`), the compiler writes the division with no test at all.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-proven-divisor-is-not-checked).
- The proof: [A proven divisor is not checked](../../../docs/proofs.md#a-proven-divisor-is-not-checked).

## The four forms

- [`naive/`](naive/): a million amounts, and 20 rounds that each divide every amount by that round's divisor (1 to
  7) and add up the quotients and the remainders. `shares` states `crash parts > 0` before its loop.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of numbers,
  the same two functions, and no zero test, since the caller never passes zero.
- [`expert.c`](expert.c): the same work tuned by hand: the amounts as unsigned numbers in one array made at its
  final size, divided unsigned, with one division giving both the quotient and the remainder.
- [`generated.c`](generated.c): what the compiler writes for `rounds` and `shares`.

## What to look at in generated.c

In `Naive_shares___held_0`, `amount / parts` is `spite_temp / spite_temp` behind only the test for `-1` (the
smallest `Integer` divided by `-1` does not fit, and halts as any overflow does), and `amount % parts` the same:
there is no `if (... == 0) spite_divided_by_zero(...)` before either, because `crash parts > 0` at the top of the
function proves the divisor and nothing in the loop assigns `parts`. In `Naive_rounds___held_0`, `round % 7` has
no test either: its divisor is a constant. A divisor with no proof gets the test; the library's own `Clock` shows
it in the same program, `if (spite_temp == 0) spite_divided_by_zero("ticks / _ticks_per_second", ...)`.

`naive.c` has no test because its author knows the divisor is never zero, and `expert.c` none for the same
reason; Spite reaches the same C because the program says so in a line the compiler reads, and a program that
breaks it halts at that line instead of dividing by zero. The C compiler then folds the `-1` test away too, since
`parts > 0` holds after the `crash`, so the three loops are the same division.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 31 204 | 176 128 |
| naive C: `naive.c`, `clang -O2` | 32 479 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 28 691 | 139 264 |

Spite takes 0.96 times naive C's time and 1.09 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=31204 naive=32479 expert=28691 -->
<!-- /timings -->
