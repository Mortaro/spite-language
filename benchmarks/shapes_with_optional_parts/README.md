# Shapes with optional parts

An archetype case (not an optimisation the compiler makes yet): 200 000 shapes in a `List<Shape>`, each with a
position and, independently, a `texture: Texture?` and a `velocity: Velocity?` (half of the shapes have each, so a
quarter have both and a quarter neither). Twenty passes move every shape that has a velocity, add up what every
shape with both parts draws, and fade every texture. No part is ever added or removed after a shape is made. It is
one of the measurements behind
[design/proposals/compiler_archetypes.md](../../design/proposals/compiler_archetypes.md): when keeping the items of
one list in one table per set of present parts (archetypes) pays, and when a part is cheaper kept apart.

## The forms

- [`naive/`](naive/): a `Shape` class with two optional parts, made from a seed and appended to a `List<Shape>`;
  each pass is `shapes.each_move()`, `shapes.sum_drawn()` and `shapes.each_fade()`, each member testing the parts it
  needs (`assert velocity`, `if texture and velocity`). `--items=N`, `--density=P` (the percent of shapes with each
  part) and `--passes=N` are `Environment` settings.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per shape with a pointer to each
  part, one `malloc` per object, an array of pointers, the same loops and tests.
- [`expert_inline.c`](expert_inline.c): the smallest step: the parts inside the shape with a byte saying which are
  there, the shapes in a pool, the list of pointers and the tests kept.
- [`expert_dense.c`](expert_dense.c): columns over every shape, the parts included, with a byte of present parts
  (columns with holes); the tests become masks.
- [`expert_sparse.c`](expert_sparse.c): positions as columns over every shape and each part in a sparse set (a
  dense array of values, the owner of each row and a map from shape to row).
- [`expert_tables.c`](expert_tables.c): one table of columns per set of present parts; each loop walks only the
  tables that have its parts, with no test.
- [`expert.c`](expert.c): the fastest of those by hand at every setting measured, the tables.
- [`highlights.c`](highlights.c): `struct Shape`, `Shape_move`, `Shape_drawn` and the `sum_drawn` loop the compiler
  writes.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form takes the same settings and prints the time of making the shapes and of the passes after its
`microseconds` line.

## What to look at in highlights.c

`struct Shape` holds a pointer to each part, and each part is an object of its own with an 8-byte header. The
shapes come from their class's pool, side by side, but the textures and velocities do not: a pool is chosen for a
class a list holds, and no list holds a `Texture`, so each part is a `malloc` of its own wherever the C library puts
it. `Shape_move` tests `self->velocity_` and follows it to another cache line; `Shape_drawn` tests both pointers for
every shape, and a quarter of the shapes answer. The three passes read every shape whatever parts it has: the
tables read only the shapes each loop needs, in order, with no test and no pointer.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes and densities: [results.md](../../design/proposals/compiler_archetypes/results.md#shapes_with_optional_parts).
