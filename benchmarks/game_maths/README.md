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

Every pass keeps its answers in slots of the frame. `accumulated * step_matrix` calls
`Matrix4__Float_multiply___into`, which builds its product in the slot it is handed (its `var product =
Matrix4<$number_type>()` is the class's own instance, so it is fresh), into a slot of its own, and the product is
copied over `accumulated` with `Matrix4__Float___copy_fields`, so the matrix is never read while it is written.
`Matrix4__Float_transform_point___into` fills its slot the same way. Before the product was fresh, each of the
200 000 steps made a matrix on the heap and released the one before, and the program took 3.19 times naive C's
time. What is left: `step_matrix` (made by `turn.to_matrix()`, on the heap since `turn` is let go there) is
counted up and down around every product.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 2 677 | 208 896 |
| naive C: `naive.c`, `clang -O2` | 2 144 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 1 679 | 139 264 |

Spite takes 1.25 times naive C's time and 1.59 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=2677 naive=2144 expert=1679 -->
<!-- /timings -->
