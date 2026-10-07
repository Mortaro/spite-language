# An item written back to its own slot is not written

`var particle = particles[index]`, a change through `particle`, then `particles[index] = particle` puts back the
object the slot already holds. When nothing between the read and the write can change that slot, the compiler writes
no code for the write-back, and when nothing to the end of the block can let the item go, the read takes no count.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#an-item-written-back-to-its-own-slot-is-not-written).
- The proof: [An item written back to its own slot is the slot](../../../docs/proofs.md#an-item-written-back-to-its-own-slot-is-the-slot),
  with the uncounted read from [An item a name holds from its list](../../../docs/proofs.md#an-item-a-name-holds-from-its-list-is-not-counted).

## The four forms

- [`naive/`](naive/): the section's own loop scaled up: 200 000 particles, 200 passes of `step()`, each reading a
  particle into a name, adding its speed to its position and writing it back to its slot.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per particle, `malloc`
  per particle, a growable array of pointers, and the write-back as a plain store of the same pointer. C keeps no
  counts, so the store is all the write-back costs it.
- [`expert.c`](expert.c): the same work tuned by hand: the particles as two columns of numbers, a pass one loop
  that adds one column into the other, which the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `step`.

## What to look at in generated.c

`Naive_step` has no line for `particles[index] = particle`: after the change to `left_` comes `index_ = (index_ + 1)`
straight away. The read is `((Particle**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]` after a bounds check, with
no `Particle___retain` and no `Particle___release` at the end of the loop body. Without the optimisation the Spite
pays for a counted read (`List_Particle_get_at`, a count up), then `List_Particle_set_at` (two bounds checks, a count
up for the new value and a count down for the old one), then a count down at the end of the block: four writes to
each particle's count on every pass. `naive.c` stores the pointer it read back into its slot, which
costs one store; `expert.c` has no slots at all.

## Timings

<!-- timings -->
<!-- /timings -->
