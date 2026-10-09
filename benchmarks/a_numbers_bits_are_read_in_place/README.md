# A number's bits are read in place

`value.bits()` on a `Float`, `bits_as_float()` on an `UnsignedInteger`, and the other reinterpretations of a
number's bits are C macros over a union of the two types: the value's bits are read as the other type where the
call is written, with no memory written and read back and nothing allocated. The library's
`to_half_precision()` and `half_precision_to_float()` are written with them, so a conversion is a few operations
on registers.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-numbers-bits-are-read-in-place).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); a union of two types of one width holds
  exactly the bits of either, so nothing has to be proven.

## The four forms

- [`naive/`](naive/): 30 000 000 values from -10 000 to 70 000 in halves, each turned into a 16-bit float and back
  (the ones above 65 504 become infinity), adding up the half's bits and the bits of the float it comes back as.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the library's two
  conversions as two C functions, step for step, with a float's bits read by `memcpy` into a variable and back,
  the way C is taught to do it without breaking its aliasing rules.
- [`expert.c`](expert.c): the same work tuned by hand: the processor's half-precision instructions (F16C), eight
  values at a time, which round, overflow and keep the sign as the library's functions do.
- [`highlights.c`](highlights.c): the two macros, the loop, and the library's two conversions as the compiler
  writes them.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`SpiteFloat_bits(self)` is `((union { float spite_value; uint32_t spite_bits; }) { .spite_value = (self)
}).spite_bits`: a compound literal of a union, written in place of the call, and
`SpiteUnsignedInteger_bits_as_float` is the same the other way round. `SpiteFloat_to_half_precision` and
`SpiteUnsignedShort_half_precision_to_float` use them on every path (`SpiteFloat_bits(self)` first,
`SpiteUnsignedInteger_bits_as_float(magnitude_)` for the small values, `SpiteUnsignedInteger_bits_as_float(bit_pattern_)`
to answer), and `Naive_round_trips` calls `SpiteFloat_bits(back_)` for the sum: nothing is stored to memory to be
read back. Around them stand Spite's other rules: `shifted.bits() - magic` and the additions are checked, and
`half_` is checked to fit an `UnsignedShort`, where `naive.c` adds and narrows unchecked.

`naive.c`'s `memcpy` costs the same at `-O2`: clang sees through a 4-byte copy just as it sees through the union.
The difference the union makes is in an unoptimised build, where the copy is a call and a store and a load, and
in what the C compiler is handed: one value in a register rather than a block it has to see through.
`expert.c` is faster because it does not run the library's algorithm
at all: the processor converts eight values in one instruction.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 64 690 | 194 560 |
| naive C: `naive.c`, `clang -O2` | 87 330 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 22 694 | 139 264 |

Spite takes 0.74 times naive C's time and 2.85 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=64690 naive=87330 expert=22694 -->
<!-- /timings -->
