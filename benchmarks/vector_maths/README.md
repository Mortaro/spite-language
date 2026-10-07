# Vector maths

A whole program, not one optimisation: five million steps of `Vector3` maths, each answer a new vector as Spite
reads it, against the same steps in C on a struct held by value. It leans on
[objects that never leave their function living in the frame](../../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)
and on [arithmetic checked in every build](../../docs/optimizations.md#arithmetic-is-checked-in-every-build).

## The four forms

- [`naive/`](naive/): a position, a velocity and an axis, and 5 000 000 steps of `velocity.scaled(0.001)` added to
  the position, the velocity nudged by its `cross` with the axis and `normalized()`, and the `dot` of the two
  added up.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a `Vector3` struct passed and returned by
  value, each operation a function.
- [`expert.c`](expert.c): the same work tuned by hand: six float locals, and the cross product with the constant axis
  `(0, 1, 0)` worked out by hand, so a step has six multiplications fewer; the arithmetic left is in the same order,
  so the answer is the same.
- [`highlights.c`](highlights.c): the loop and the five `Vector3` functions it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Every answer that stays in the loop is a slot in the frame: `scaled`, `+` and `cross` are `___into` functions that
fill the slot `Naive_Naive` hands them (`Vector3__Float_scaled___into(velocity_, 0.001, &spite_slot_3)`), and the new
position is copied back into the old one with `Vector3__Float___copy_fields`. `normalized()` is not: it is called as
`Vector3__Float_normalized`, which makes its answer with `Vector3__Float___make`, so each step allocates the new
velocity and releases the old one (the frame claim for a member callee's answer fails without a report; it is
listed as still open in design/status.md). The argument of each operator is counted up before the call and down
inside it, and the conversions to `Long` at the end are checked.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 161 486 | 175 616 |
| naive C: `naive.c`, `clang -O2` | 67 412 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 50 017 | 139 264 |

Spite takes 2.40 times naive C's time and 3.23 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=161486 naive=67412 expert=50017 -->
<!-- /timings -->
