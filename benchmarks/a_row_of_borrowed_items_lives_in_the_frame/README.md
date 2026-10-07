# A row of borrowed items lives in the frame

An object literal of borrowed `Vector` items, `{position: positions[entity], velocity: velocities[entity]}`, reads
as an object made for the call. It is not allocated: it is a struct in the frame of the function that makes it,
its attributes the items' addresses, uncounted, and the function it is passed to is compiled a second time for it,
as `<name>___lent_<positions>`, which neither retains nor releases it.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-row-of-borrowed-items-lives-in-the-frame).
- The proof: [Rows and walked rows](../../docs/proofs.md#rows-of-borrowed-items).

## The four forms

- [`naive/`](naive/): 100 000 entities in four `Vector` columns (position, velocity, health, regeneration), and 100
  ticks that each make two rows per entity and hand them to two systems, `mover.update_each(moving)` and
  `healer.update_each(mending)`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the vectors as arrays of
  structs, and each row an object made for its call: a `malloc`ed struct of two pointers, freed after the call.
- [`expert.c`](expert.c): the same work tuned by hand: no rows, six columns of numbers, one vectorised loop per
  tick.
- [`highlights.c`](highlights.c): the row's struct, the tick that makes the rows, and the mover's lent copy.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_tick_once`, `Object_position_Position_velocity_Velocity spite_temp = { { 1, 179 } };` is the row: a
local struct whose header (a count of 1 and its class) is set once, then its two attributes set to the items'
addresses from `Vector__Position_get_at` and `Vector__Velocity_get_at`, and `moving_` is its address. It is passed
to `Mover_update_each___lent_0`, not to `Mover_update_each`, and nothing retains it, releases it or frees it: it
ends with the loop's turn. The lent copy reads the components through the `type` uncounted, as
[Reading through a `type` without counting](../reading_through_a_type_without_counting/) shows. `--debug-memory`
counts 400 016 allocations for the run: the 400 000 components `spawn_all` makes, and none for the 20 000 000 rows.

`naive.c` asks for a `malloc` and a `free` per row, but clang removes the pair once it has inlined the system into
the loop, since the row never leaves it, so at `-O2` it ends with a row on the stack too and is several times
faster than Spite here: the cost Spite still pays is around the row, not in it. Each turn calls
`Vector__Position_count` for the bound, and the line `crash velocities[entity] and healths[entity] and
regenerations[entity]` reads three items to test them before the rows read them again; each `+` is checked.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 46 634 | 190 976 |
| naive C: `naive.c`, `clang -O2` | 8 363 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 3 298 | 139 776 |

Spite takes 5.58 times naive C's time and 14.14 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=46634 naive=8363 expert=3298 -->
<!-- /timings -->
