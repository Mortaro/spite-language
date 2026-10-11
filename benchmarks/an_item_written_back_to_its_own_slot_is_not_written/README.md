# An item written back to its own slot is not written

`var particle = particles[index]`, a change through `particle`, then `particles[index] = particle` puts back the
object the slot already holds. When nothing between the read and the write can change that slot, the compiler writes
no code for the write-back, and when nothing to the end of the block can let the item go, the read takes no count.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-item-written-back-to-its-own-slot-is-not-written).
- The proof: [An item written back to its own slot is the slot](../../docs/proofs.md#an-item-written-back-to-its-own-slot-is-the-slot),
  with the uncounted read from [An item a name holds from its list](../../docs/proofs.md#an-item-a-name-holds-from-its-list-is-not-counted).

## The four forms

- [`naive/`](naive/): the section's own loop scaled up: 200 000 particles, 200 passes of `step()`, each reading a
  particle into a name, adding its speed to its position and writing it back to its slot.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per particle, `malloc`
  per particle, a growable array of pointers, and the write-back as a plain store of the same pointer. C keeps no
  counts, so the store is all the write-back costs it.
- [`expert.c`](expert.c): the same work tuned by hand: the particles as two columns of numbers, a pass one loop
  that adds one column into the other, which the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `step`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_step` has no line for `particles[index] = particle`: after the change to `left_` comes `index_ = (index_ + 1)`
straight away. The read is `((Particle**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]` after a bounds check, with
no `Particle___retain` and no `Particle___release` at the end of the loop body. Without the optimisation the Spite
pays for a counted read (`List_Particle_get_at`, a count up), then `List_Particle_set_at` (two bounds checks, a count
up for the new value and a count down for the old one), then a count down at the end of the block: four writes to
each particle's count on every pass. `naive.c` stores the pointer it read back into its slot, which
costs one store; `expert.c` has no slots at all.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 37 078 | 201 728 |
| naive C: `naive.c`, `clang -O2` | 43 310 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 3 465 | 139 776 |

Spite takes 0.86 times naive C's time and 10.70 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=37078 naive=43310 expert=3465 -->
<!-- /timings -->
