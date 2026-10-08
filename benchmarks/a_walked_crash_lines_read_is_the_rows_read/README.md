# A walked `crash` line's read is the row's read

A walked row's template states each read with a `crash` line before the fill (`crash
Column<attribute.class>().values[stored_row]`, then `row.attributes[attribute] =
Column<attribute.class>().values[stored_row]`), since every `[]` answers a `T?`. Written out plainly that reads
each item twice, once to test it and once to fill the row, each time through the column singleton's guard; the
compiler reads it once, keeps the answer in a local, tests it, and fills the row from that local.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-walked-crash-lines-read-is-the-rows-read).
- The proof: none of its own; the row it fills stands on [Rows and walked rows](../../docs/proofs.md#rows-of-borrowed-items),
  and the `crash` line is what narrows the `T?` the read answers
  ([Narrowing a `T?`](../../docs/proofs.md#narrowing-a-t)).

## The four forms

- [`naive/`](naive/): 100 000 entities over two sparse columns (`Column<Position>` for every entity, and
  `Column<Velocity>` for two in three, inserted in reverse), a generic `Runner<Mover.Moving>` that walks the
  `Moving` row's attributes to find each one's place and then fill the row, and 50 ticks of it.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each column a sparse set
  beside an array of components, reached through a function that makes it on first use; the runner collects the
  places in a list, then for each component tests that the place and the item are there and reads both again to
  fill the row, as the template's lines read.
- [`expert.c`](expert.c): the same work tuned by hand: components as columns of numbers, each sparse set an array
  from entity to place, and one loop per tick that looks up both places and moves the entity, with no list and no
  row.
- [`highlights.c`](highlights.c): the runner's `run`, with the walk written out.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Runner__Mover_Moving_run`, the walk over `Moving`'s attributes is written out as the lines it amounts to, with
`attribute.index` a constant: `List_Integer_get_at(self->found_, 1)` is the `crash rows[attribute.index]` line,
kept in `spite_temp_1` and tested; `Vector__Position_get_at(...values_, spite_temp_1.value)` is the walked `crash`
line, kept in `spite_temp_2` and tested; and the row's `position_` is set to `spite_temp_2`, the same read, not a
second call to `Vector__Position_get_at` and a second pass through `spite_singleton_Column__Position()`. The
velocity is the same with places 3 and 4. The `Entity` is [made in the frame](../a_row_of_borrowed_items_lives_in_the_frame/)
beside the row, and the row goes to `Mover_update_each___lent_0`. `naive.c` reads each place and each item twice,
as the template's lines say.

Spite is several times slower than `naive.c` here, and not in the lines this optimisation touches: each entity
runs `find_attributes` as three calls that each reach the column singleton and append to the `found` list through
`List_Integer_append`, with its checks, and the walk's reads of `found` go through `List_Integer_get_at`, where
`naive.c` reads its arrays in place.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 83 413 | 198 144 |
| naive C: `naive.c`, `clang -O2` | 40 333 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 5 258 | 139 776 |

Spite takes 2.07 times naive C's time and 15.86 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=83413 naive=40333 expert=5258 -->
<!-- /timings -->
