# A function value describes its arguments when asked

A function value is its own reflection object, with `.arguments`, a list of `Spite.Argument`s. Filling that list
when the value is made would cost objects per argument that almost no program reads, so the value carries a
pointer to a function the compiler wrote for it, and `.arguments` fills the list the first time it is read.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-function-value-describes-its-arguments-when-asked).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the list is filled under a lock the first
  time it is read, so every reader sees the same list, in the same order.

## The four forms

- [`naive/`](naive/): 2 000 000 calls of `apply(scorer.score, value)`, each making the function value
  `scorer.score` and calling through it, and at the end one read of `scorer.score`'s `.arguments.count()`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a function value is an object
  holding the function, its owner and its description, its list of arguments made with it, each argument with its
  name and class, and freed after the call.
- [`expert.c`](expert.c): the same work tuned by hand: the value is a function pointer and its owner on the stack,
  and the description a constant.
- [`highlights.c`](highlights.c): the maker of the value, the function that describes it, how the description is
  filled on first read, and the loop.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`spite_function_value_Scorer_score` makes a `Spite_Function` with its name and return class, stores the owner and
`Scorer_score` as `spite_typed_call`, and sets `spite_add_arguments` to
`spite_function_value_Scorer_score___arguments` without calling it: the list of arguments is made empty and stays
empty. `Spite_Function_get_arguments`, what `.arguments` reads, first calls `spite_function_arguments_described`,
which, under the value's lock and only once, calls that function to append the one `Spite_Argument` and clears the
pointer. That maker is what the one read of `scorer.score`'s `.arguments` at the end uses.

The loop does not use it. `apply` does nothing with `change` but call it, so in `Naive_apply_all___held_0` the
value is `&(Spite_Function){ .spite_owner = (void*)(scorer_), .spite_typed_call = (void*)Scorer_score }`, a struct
in the loop's frame holding the owner and the function and nothing else, passed to `Naive_apply___held_0`, which
calls through `spite_typed_call` and lets nothing go
([the proof](../../docs/proofs.md#a-function-value-its-callee-only-calls-lives-in-the-frame)). Once clang
inlines `Naive_apply___held_0` it sees the function the struct holds and calls `Scorer_score` directly, inlined in
turn, as `expert.c` does by hand.

`--debug-memory` counts 14 allocations for the run, none of them in the loop; before the value lived in the frame
it counted 4 000 014, the value and its empty list at each of the 2 000 000 calls, and Spite took 66.8 times
`naive.c`'s time (`naive.c`'s four `malloc`s and their `free`s per call are removed by clang, which sees that the
value never leaves `apply_all` once `apply` is inlined). `index = index + 1` is the plain `index_ + 1`, since
`index < count` is in force and no call can change a local, even one through a function value.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 812 | 194 560 |
| naive C: `naive.c`, `clang -O2` | 1 258 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 1 255 | 139 264 |

Spite takes 0.65 times naive C's time and 0.65 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=812 naive=1258 expert=1255 -->
<!-- /timings -->
