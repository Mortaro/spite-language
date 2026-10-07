# Deciding conditions at compile time

`if $is_squared { ... } else { ... }` in `Meter` asks a codegen value, which is a fact of each instance, so the
compiler answers it while compiling and writes only the branch taken: `Meter<false>` has only the addition and
`Meter<true>` only the square. The branch not taken is not tested at run time; it is not in the program at all.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#deciding-conditions-at-compile-time).
- The proof: [Conditions decided while compiling](../../../docs/proofs.md#conditions-decided-while-compiling).

## The four forms

- [`naive/`](naive/): a million whole numbers, and 20 rounds passing each to two meters, one adding the values and
  one adding their squares.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one `Meter` struct for both
  kinds that keeps `is_squared` as a field, `malloc` per meter, and `meter_add` testing the field on every call.
- [`expert.c`](expert.c): both sums of a round in one pass over a plain array, the kind of each sum decided by
  where it is written, so no loop tests anything.
- [`generated.c`](generated.c): what the compiler writes for `Meter<true>`'s struct, the two instances' `add` and
  the loop that calls them.

## What to look at in generated.c

`Meter__false_add` is the addition alone and `Meter__true_add` the multiplication and the addition alone: neither
has an `if`, and `struct Meter__true` (like `Meter__false`) holds only its header and `total_`, with no field
saying which it is, since the codegen value is part of the class's name.
`Naive_measure___held_0_1_2` calls each directly. `naive.c` keeps the test, `if (meter->is_squared)`, in
`meter_add`; at `clang -O2` it does not cost much there, because `main` makes both meters with a literal flag in the
same file, so clang inlines `meter_add`, carries the flag and drops the test itself (the two C programs ran in 9 and
8 ms, the Spite one in 13, one run each). The Spite one keeps its checks (each `values[index]` read tested against
the list, each sum tested for overflow), which the C programs do not have, and the loop calls `add`, so
[the plain-values loop](../../../docs/optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked)
does not apply. What the fold guarantees is what clang only finds when it can see everything at once: the
untaken branch is absent whatever compiles the C, and it may even use what that instance does not have.

## Timings

<!-- timings -->
<!-- /timings -->
