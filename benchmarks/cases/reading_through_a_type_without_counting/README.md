# Reading through a `type` without counting

`moving.position.left = moving.position.left + moving.velocity.across * steps` reads `position` and `velocity`
through the `type` `Moving`, which would answer each component retained and release it once its number is read.
Where only a plain attribute of the component is read or written, the compiler asks the `type` for the component
as it lies in the value, uncounted, with the attribute's default standing in for a class that has no such
attribute.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#reading-through-a-type-without-counting).
- The proof: none of its own; a write borrows only under the same proof as
  [List templates read uncounted](../../../docs/proofs.md#a-lists-templates-read-their-elements-uncounted), and the
  value read through stands on [Rows and walked rows](../../../docs/proofs.md#rows-of-borrowed-items).

## The four forms

- [`naive/`](naive/): 100 000 entities as a `Vector<Position>` and a `Vector<Velocity>`, and 200 ticks that each
  hand every entity's row, `{position: positions[index], velocity: velocities[index]}`, to `mover.advance(moving,
  steps)`, which takes it as the `type` `Moving`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: arrays of structs for the
  vectors, the row a struct of two pointers, and `Moving` an interface: the object and its class, with a getter
  per attribute that switches on the class, called for every read.
- [`expert.c`](expert.c): the same work tuned by hand: four columns of numbers and one vectorised loop per tick.
- [`generated.c`](generated.c): the two `peek` functions of `Moving` and the copy of `advance` the row is lent to.

## What to look at in generated.c

`Mover_Moving___peek_position` answers `((Object_position_Position_velocity_Velocity*)self)->position_` when the
value's class is the row's, and `0` otherwise: a pointer to the component where it lies, with no retain. In
`Mover_advance___lent_0` each read is `({ Position* p = Mover_Moving___peek_position(moving_); p != 0 ? p->left_ :
(0); })`, the default `0` standing in for a class without the attribute, and each write lands in
`*Mover_Moving___peek_position(moving_)` or, for such a class, in `spite_temp_scratch`, a `Position` in the frame
that is thrown away, as a write to a fresh default component would be. Nothing is retained or released in the
function, and `--debug-memory` counts 200 011 allocations for the run: the 200 000 components `fill` makes (each
made as an object, then copied into its vector), and none for the 20 000 000 rows or their reads. `naive.c` reads through its getters the
same way and never counts; the class test folds away in both once the C compiler inlines the getter.

When does a read go through the `type` at all? Here, because the value is a row of borrowed items: a row is
passed only to its function's lent copy, and inside it the row is a `Moving`. Hand `advance` an object of a class
of the program's own instead (a `Body` with a `position` and a `velocity`) and the compiler writes
`Mover_advance___for_0_Body`, in which `moving.position.left` is `moving_->position_->left_`: a function taking a
`type` is [compiled per class](../a_function_taking_a_type_is_compiled_per_class/), and no `type` is read.

Spite is about three times slower than `naive.c` on this loop, for reasons outside this optimisation: the vector's
`count()` and the `crash velocities[index]` read are calls each turn, and each `+` and `*` is
[checked](../arithmetic_is_checked_in_every_build/).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 35 456 | 182 272 |
| naive C: `naive.c`, `clang -O2` | 10 962 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 6 221 | 139 776 |

Spite takes 3.23 times naive C's time and 5.70 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=35456 naive=10962 expert=6221 -->
<!-- /timings -->
