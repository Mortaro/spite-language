# Arithmetic is checked in every build

Every `+`, `-` and `*` on whole numbers is the C compiler's overflow builtin, and an answer that does not fit halts
naming its line, in the release build as in every other: there is no unchecked mode. The check is left out only
where the compiler proves it cannot fire (a counter stepped by one under its loop's `<`, arithmetic on constants),
so this is the one case where Spite is meant to be slower than plain C, and the README says what that buys.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#arithmetic-is-checked-in-every-build).
- The proof: [Arithmetic that does not fit halts](../../../docs/proofs.md#arithmetic-that-does-not-fit-halts),
  which is also what leaves the counter's check out.

## The four forms

- [`naive/`](naive/): 100 000 numbers, and 400 rounds that each add up `value * 3 + round` over all of them in an
  `Integer`, the rounds' sums added up in a `Long`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of numbers,
  the same two functions, and C's own arithmetic, which checks nothing.
- [`expert.c`](expert.c): the same work tuned by hand: one array made at its final size and the same unchecked
  loop, which the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `rounds` and `checksum`.

## What to look at in generated.c

In `Naive_checksum___held_0`, `total + values[index] * 3 + round` is three operations, and each is a
`__builtin_mul_overflow` or `__builtin_add_overflow` followed by `if (__builtin_expect(..., 0))
spite_overflowed("values[index] * 3", "an Integer", "*", ...)`: a halt that names the expression, its type, the
operator and both operands. The counter's `index_ = (index_ + 1)` has no check, since `index < values.count()`
is in force and the next number fits; neither has `round_ = (round_ + 1)` in `Naive_rounds___held_0`, under
`round < 400`. The read `values[index]` is a plain load, `spite_temp[index_]`, its count read once before the
loop.

## Why Spite is slower here, and what it buys

Each check is a compare and a branch that is never taken, cheap on its own. What it costs is the loop around it:
with three exits that can stop the program part way, the C compiler may no longer add eight numbers at once, so
`naive.c` and `expert.c` run a vectorised loop and Spite a scalar one, several times slower on this loop of
nothing but additions. Loops that do more per element (calls, loads through pointers, divisions) pay a few percent
or nothing, as the measured costs on the optimisation's section show.

What it buys: an `Integer` sum that does not fit stops the program at that line, naming the values, where `naive.c`
gives a wrong total without a word (and, being signed C arithmetic, is not even promised to give that). Here every
sum fits, so the answers are the same; make the list thirteen times as long and a round's sum passes 2^31, and
Spite halts while both C programs print a number. A program that wants wrapping says so with `wrapping_sum`, which is one plain C
addition, and a sum that cannot overflow can be held in a type it fits (a `Long` here would still be checked, but
never fire).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 21 653 | 175 104 |
| naive C: `naive.c`, `clang -O2` | 3 272 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 3 336 | 139 264 |

Spite takes 6.62 times naive C's time and 6.49 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=21653 naive=3272 expert=3336 -->
<!-- /timings -->
