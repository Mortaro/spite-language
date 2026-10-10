# A table filled once is read as constants

`Checkout()` fills `steps` with `'discount'`, `'tax'`, `'discount'`, one `append` after the other, and nothing ever
puts another step in or takes one out. So `price` is written twice: as it reads, and with `steps` a constant list
of those three values, which the C compiler unrolls into the three steps. A test of the checkout's own list at the
start of `price` picks the copy; a checkout that held anything else would run `price` as written.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-table-filled-once-is-read-as-constants).
- The proof: [A table filled once](../../docs/proofs.md#a-table-filled-once-holds-what-its-setup-put-in).

## The four forms

- [`naive/`](naive/): a checkout whose constructor adds three steps (discount, tax, discount), then ten million
  prices put through it; `price` tests each step against all four kinds of step.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of step codes
  filled when the checkout is made and the same four tests per step.
- [`expert.c`](expert.c): the same work tuned by hand by someone who knows the checkout: the three steps written
  out, with no list and no test.
- [`highlights.c`](highlights.c): what the compiler writes for `price` and for its copy.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Checkout_price` starts with the test of the list's count and its three items, and when it holds calls
`Checkout_price___configured_0`, which reads `spite_configured_Checkout_steps_0`, a constant list of the three
steps, so its loop has a known count and known items. The tests of `'rounding'` and `'coupon'` are already
`((void)(...), 0)` in both, since no item is ever one of those. What is left against `expert.c` is the overflow check
of each step's arithmetic, which Spite makes in every build.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 26 933 | 200 192 |
| naive C: `naive.c`, `clang -O2` | 20 361 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 19 451 | 139 264 |

Spite takes 1.32 times naive C's time and 1.38 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=26933 naive=20361 expert=19451 -->
<!-- /timings -->
