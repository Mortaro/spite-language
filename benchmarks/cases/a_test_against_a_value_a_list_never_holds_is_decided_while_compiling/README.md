# A test against a value a list never holds is decided while compiling

`steps` is filled only by `add`, and every call of `add` in the program passes `'discount'` or `'tax'`, so no item
is ever `'rounding'` or `'coupon'`. The compiler lists the values a list only its class fills can hold, and decides
every test of an item against any other value, so those two tests are `false` before the program runs.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-test-against-a-value-a-list-never-holds-is-decided-while-compiling).
- The proof: [A list only its class fills](../../../docs/proofs.md#a-list-only-its-class-fills-holds-only-what-it-fills).

## The four forms

- [`naive/`](naive/): a pipeline of three steps (discount, tax, discount) added once, then ten million prices put
  through it; `price` tests each step against all four kinds of step.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of step codes
  and the same four tests per step. C knows nothing of what the array can hold, so it tests all four.
- [`expert.c`](expert.c): the same work tuned by hand by someone who knows the pipeline: the three steps written
  out, with no list and no test.
- [`generated.c`](generated.c): what the compiler writes for `price`.

## What to look at in generated.c

The third and fourth `if` are `((void)(...), 0)`: the step is still read (so an index outside the list would still
halt), but the answer is `false`, and the C compiler drops the read and the branch. The first two tests stay, since
the list holds both values. Only the configuration's values are known, not its order or its length, so the loop
over `steps` stays; folding those too is step 2 of pair S1.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 28 970 | 176 128 |
| naive C: `naive.c`, `clang -O2` | 41 692 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 21 221 | 139 264 |

Spite takes 0.69 times naive C's time and 1.37 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=28970 naive=41692 expert=21221 -->
<!-- /timings -->
