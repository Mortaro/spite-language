# Identical functions are folded into one

`Position` and `Velocity` are two classes with the same attributes, so `List<Position>` and `List<Velocity>`, their
member templates and the classes' own constructors are the same code over the same layout. The compiler keeps one
of each such group and sends every call of the others to it, so the program carries each function once.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#identical-functions-are-folded-into-one).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); two functions fold only when they are the
  same statements over types of the same layout, so the one kept does exactly what the other would.

## The four forms

- [`naive/`](naive/): a thousand positions and a thousand velocities in two lists, and the sums of both attributes
  of both.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: two structs, and each list,
  its `append` and its two sums written once per struct.
- [`expert.c`](expert.c): one struct for the shared layout, and one function of each kind for both arrays.
- [`generated.c`](generated.c): the pointers folding writes, `Naive`, and the one `sum_across` both lists use.

## What to look at in generated.c

Each `static __typeof__(&X) spite_folded_X = ((__typeof__(&X))&Y);` line says that `X` is not written: its calls go
to `Y`. `List_Velocity_sum_across` is `List_Position_sum_across`, `Velocity_Velocity` is `Position_Position`, and
both lists' `count` are the one `List_Console_Printable_count` the program already had for printing. `Naive` calls
`spite_folded_List_Velocity_sum_across(velocities_)`, a pointer nothing writes, which the C compiler turns back into
a direct call. This program folds 11 functions.

## Why there is no time

Folding changes the size of the program, not its speed: the C compiler makes the call through the folded pointer
direct again. So compare the executable sizes below and the functions: `naive.c` writes six functions twice over,
`expert.c` writes each once, and the compiler arrives at `expert.c`'s count from `naive/`'s source. At this size the
saving is a few hundred bytes of machine code; it grows with every generic class made for several classes of one
layout, and with every package loaded in two versions.

## Timings

<!-- timings -->
<!-- /timings -->
