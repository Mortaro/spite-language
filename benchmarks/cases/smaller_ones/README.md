# Smaller ones

The section lists several small optimisations; this case shows the one with a time: a function passed to a
template by name, `values.each(tally.count_value)`, is not made into a function value. The template is written
once for that function and its owner, so the call allocates nothing and calls the function directly, where a
function held in a variable would be called through its `Spite.Function`.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#smaller-ones).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the function named is the one called,
  so nothing is left to decide while the program runs.

## The four forms

- [`naive/`](naive/): a million numbers, and 50 rounds of `values.each(tally.count_value)`, which adds each number
  to a total and counts the large ones.
- [`naive.c`](naive.c): the same program as a C programmer writes it: `tally.count_value` is a function value, so a
  struct of a function pointer and its owner is made with `malloc` per round, and `each` calls through it.
- [`expert.c`](expert.c): the counting written into the loop, in locals, branchless, so the C compiler vectorises it.
- [`generated.c`](generated.c): the `each` the compiler writes for this function and owner, and the function.

## What to look at in generated.c

`List_Integer_each_count_value_for_tally` is `each` written for `count_value` and its owner: it takes the `Tally`
itself, not a function value, and calls `Tally_count_value(owner_, item_)` directly, so the C compiler can copy the
function into the loop. Nothing is allocated for the call. `naive.c` calls through a pointer it allocated (clang may
see through it in so small a program); `expert.c` keeps the two counts in registers, where the Spite writes them
to the tally's attributes on every number and checks each addition for overflow.

The other items of the section change no time a program could measure: a `T?` that is the reference itself, a
generic singleton's static slot, a library `assert` that writes nothing into the crash trace, a program with no
`crash` carrying no trace at all, an attribute written through a local, and a foreign library closed at exit only
when something opens it. Each shows in the C of any program that uses it.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 30 578 | 176 640 |
| naive C: `naive.c`, `clang -O2` | 11 259 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 10 043 | 139 264 |

Spite takes 2.72 times naive C's time and 3.04 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=30578 naive=11259 expert=10043 -->
<!-- /timings -->
