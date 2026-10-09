# Entities with components added and removed

An archetype case (not an optimisation the compiler makes yet), the one written like an entity system: 200 000
entities in a `List<Entity>`, each with a position and three optional components: a `velocity` and a `health`,
given when the entity is made and never changed, and `burning`, added to 2 000 entities chosen at random each pass
and removed by the entity itself when its ticks run out. Each of twenty passes sets the fires, moves the entities
with a velocity, burns the ones with health that are on fire, cools every fire and heals every entity with health.
It is one of the measurements behind
[design/proposals/compiler_archetypes.md](../../design/proposals/compiler_archetypes.md): components that stay and a
component that comes and goes, with no word in the program saying which is which.

## The forms

- [`naive/`](naive/): an `Entity` class with three optional components; each pass calls `entity.catch_fire(seed)`
  on entities picked by index, then `each_move()`, `sum_burn()`, `each_cool()` and `each_heal()`, each member
  testing the components it needs, and `cool()` setting `burning = null` when the fire is out. `--items=N`,
  `--density=P` (the percent of entities with a velocity, and with health), `--passes=N` and `--churn=N` (entities
  set on fire a pass) are `Environment` settings.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per entity with a pointer to each
  component, one `malloc` per object, a fire freed when it goes out, the same systems and tests.
- [`expert_inline.c`](expert_inline.c): the components inside the entity with a byte saying which are there.
- [`expert_dense.c`](expert_dense.c): columns over every entity, the components included (columns with holes).
- [`expert_sparse.c`](expert_sparse.c): positions as columns, each component in a sparse set: the layout of the
  entity systems that keep every component apart.
- [`expert_tables.c`](expert_tables.c): one table per set of components (eight); catching fire or going out moves
  the entity between tables: the layout of the entity systems that group by archetype.
- [`expert_hybrid.c`](expert_hybrid.c): the velocity and the health, which never change, decide the table (four
  tables, and an entity never moves); burning, which comes and goes, is a sparse set beside them.
- [`expert.c`](expert.c): the fastest by hand at the default settings, the hybrid. Which form wins elsewhere is in
  the proposal.
- [`highlights.c`](highlights.c): `struct Entity`, `Entity_burn`, `Entity_cool` and the `sum_burn` loop the
  compiler writes.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

## What to look at in highlights.c

`struct Entity` holds a pointer per component, each component an object of its own made by `malloc` (no list holds
them, so none has a pool). `Entity_cool` lets a fire go when its ticks reach zero, and `Entity_catch_fire` makes a
new one: every change of an entity's shape is an allocation or a free. Every system reads every entity to answer for
the ones that have its components. The hybrid's systems over velocity and health walk their tables with no test,
and the ones over burning walk the fires; catching fire and going out touch only the fires.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes, densities and churn: [results.md](../../design/proposals/compiler_archetypes/results.md#entities_with_components_added_and_removed).
