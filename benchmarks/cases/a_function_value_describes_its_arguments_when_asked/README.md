# A function value describes its arguments when asked

A function value is its own reflection object, with `.arguments`, a list of `Spite.Argument`s. Filling that list
when the value is made would cost objects per argument that almost no program reads, so the value carries a
pointer to a function the compiler wrote for it, and `.arguments` fills the list the first time it is read.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-function-value-describes-its-arguments-when-asked).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the list is filled under a lock the first
  time it is read, so every reader sees the same list, in the same order.

## The four forms

- [`naive/`](naive/): 2 000 000 calls of `apply(scorer.score, value)`, each making the function value
  `scorer.score` and calling through it, and at the end one read of `scorer.score`'s `.arguments.count()`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a function value is an object
  holding the function, its owner and its description, its list of arguments made with it, each argument with its
  name and class, and freed after the call.
- [`expert.c`](expert.c): the same work tuned by hand: the value is a function pointer and its owner on the stack,
  and the description a constant.
- [`generated.c`](generated.c): the maker of the value, the function that describes it, how the description is
  filled on first read, and the loop.

## What to look at in generated.c

`spite_function_value_Scorer_score` makes a `Spite_Function` with its name and return class, stores the owner and
`Scorer_score` as `spite_typed_call`, and sets `spite_add_arguments` to
`spite_function_value_Scorer_score___arguments` without calling it: the list of arguments is made empty and stays
empty. `Spite_Function_get_arguments`, what `.arguments` reads, first calls `spite_function_arguments_described`,
which, under the value's lock and only once, calls that function to append the one `Spite_Argument` and clears the
pointer. `Naive_apply` calls through `spite_typed_call` with the owner, and never looks at the description.

`--debug-memory` counts 4 000 014 allocations for the run: two per call (the value and its empty list), where a
value described when made would cost more per argument (the doc's `benchmarks/function_values` went from 10 a
value to 2). That is all this optimisation is about: the time is another matter. Spite makes those two objects on
every call, where `naive.c`'s four `malloc`s and their `free`s are removed by clang, which sees that the value
never leaves `apply_all` once `apply` is inlined, so `naive.c` runs as fast as `expert.c` and Spite far slower.
The value `scorer.score` is the same at every turn of the loop; making it once before the loop, or in the frame,
is work the compiler does not do yet. And `index = index + 1` keeps its overflow check here, though `index <
count` is in force, where the loops of the other cases drop it.

## Timings

<!-- timings -->
<!-- /timings -->
