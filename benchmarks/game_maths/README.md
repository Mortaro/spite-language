# Game maths

The library's game maths against the same passes in C: a million `position + velocity.scaled(delta)` steps on
`Vector3`, 200 000 `Matrix4` products of a turn made from a `Quaternion`, and a million `transform_point`s. It leans
on [objects that never leave their function living in the frame](../../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)
([game_maths.md](../../docs/game_maths.md)).

## The four forms

- [`naive/`](naive/): the three passes as the library's `Vector3<Float>`, `Quaternion<Float>` and `Matrix4<Float>`
  read, each answer a new object.
- [`naive.c`](naive.c): the same program as a C programmer writes it: `Vector3`, `Quaternion` and `Matrix4` as
  plain structs passed and returned by value, each operation a function.
- [`expert.c`](expert.c): the same work tuned by hand: every number a float local, each product only the eight
  multiplications that a turn about the y axis changes (the other terms are exact zeros and ones), and the
  transformed point, which never changes, worked out once.
- [`highlights.c`](highlights.c): the three passes, the matrix product and `transform_point`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The vector pass and the transforms keep every answer in a slot of the frame (`Matrix4__Float_transform_point___into`
fills the slot it is handed). The matrix pass does not: `accumulated * step_matrix` calls `Matrix4__Float_multiply`,
which makes its product on the heap, so each of the 200 000 steps allocates a matrix and releases the one before,
and `step_matrix` is counted up and down around every call. That is most of the gap to `naive.c`, whose product is a
struct the C compiler keeps in registers.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 7 196 | 185 856 |
| naive C: `naive.c`, `clang -O2` | 2 257 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 1 752 | 139 264 |

Spite takes 3.19 times naive C's time and 4.11 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=7196 naive=2257 expert=1752 -->
<!-- /timings -->
