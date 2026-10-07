# A decimal literal beside a `Float` is a `Float`

`(value + shift) * 1.5 + 0.25` with a `Float` `value` is written `* 1.5f` and `+ 0.25f` in the C, so the
arithmetic stays in `Float` instead of being widened to `Double` and narrowed back, as C does with a plain `1.5`.
A comparison then means what it reads as too: after `var tenth: Float = 0.1`, `tenth == 0.1` is `true`.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-decimal-literal-beside-a-float-is-a-float).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); it is a rule of the literal's type,
  decided by the other side of the operator, not a fact the compiler has to establish.

## The four forms

- [`naive/`](naive/): 50 000 `Float`s (quarters from 0 to 15.75), and 4 000 rounds that each add up
  `(value + shift) * 1.5 + 0.25` over all of them in a `Float`, the rounds' sums added up in a `Double`. Every value
  and every partial sum is exact in a `Float`, so the answer does not depend on the order of the additions.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of floats,
  the same two functions, and the same expression, in which C reads `1.5` and `0.25` as doubles.
- [`expert.c`](expert.c): the same work tuned by hand: one array made at its final size, `1.5f` and `0.25f`, and
  each round's sum kept in eight running totals that the C compiler holds in one vector register.
- [`generated.c`](generated.c): what the compiler writes for `rounds` and `scaled_sum`.

## What to look at in generated.c

`Naive_scaled_sum___held_0` is one line of arithmetic, `total_ = ((total_ + ((spite_temp[index_] + shift_) *
1.5f)) + 0.25f);`: both literals are `float`, and nothing is converted. In `naive.c` the same line converts the
value to `double`, multiplies and adds in `double`, adds the `float` total widened, and narrows the answer back
into `total`, every element, along one chain of dependent operations.

The loop also carries `#pragma clang fp contract(fast) reassociate(on)`, the other half of what makes it fast:
the last bits of a decimal result are not a promise in Spite (a loop that compares decimals with `==` or `!=`
excepted), so the C compiler may split the sum across vector lanes, as `expert.c` does by hand. The two are
separate rules, and both are needed here: `naive.c` with only its literals written `1.5f` and `0.25f` runs in
about half its time (265 ms against 536 ms, one run each, `clang -O2`, on a Windows machine on 2026-10-07),
still one addition after another; only the reassociation lets the additions run side by side.

## Timings

<!-- timings -->
<!-- /timings -->
