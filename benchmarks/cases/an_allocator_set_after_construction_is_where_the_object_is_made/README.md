# An allocator set after construction is where the object is made

`var particle = Particle(index + round, index % 5)` followed by `particle.memory.allocator = arena` reads as a
particle made on the heap and then moved into the arena. The compiler joins the two lines into one construction
that asks the arena for the memory, so the particle is made in the arena from the start and the heap is never
asked for it.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#an-allocator-set-after-construction-is-where-the-object-is-made).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); setting an allocator anywhere but the
  line right after the construction is a compile error, so no object the line names can have been used before it.

## The four forms

- [`naive/`](naive/): 200 rounds, each making a `Memory.Arena`, a `List<Particle>` in it and 10 000 particles in
  it, adding up the particles' two attributes, and letting the arena go at the end of the round.
- [`naive.c`](naive.c): the same program as a C programmer writes it without an arena: a struct per class, `malloc`
  per particle, a growable array of pointers for the list, and each particle freed at the end of its round.
- [`expert.c`](expert.c): the same work with a bump allocator by hand: one block per round for the particles and
  their array of pointers, each particle a bump of the pointer, and the block given back at once.
- [`generated.c`](generated.c): the `Particle` struct, `one_round`, and the constructor that makes a particle at an
  address it is handed.

## What to look at in generated.c

In `Naive_one_round` the two lines are one call,
`Particle___make_in(spite_temp_<n>, spite_give_back_Memory_Arena, Memory_Arena_allocate(spite_temp_<n>, (int64_t)sizeof(Particle)), ...)`:
the arena's `allocate` gives the memory, and `Particle___make_in` builds the object there, with no heap call and
nothing copied. `struct Particle` carries the two hidden pointers the section names, `spite_allocator` and
`spite_give_back`, sixteen bytes on every particle. Under `--debug-memory` the program makes 2 206 allocations in
all (the arenas, their 64 KB blocks and the list's first buffers), where `naive.c` calls `malloc` more than
2 000 000 times.

What `expert.c` still does better: each particle in `naive/` takes a count on the arena
(`Memory_Arena___retain(arena_)` before every `Particle___make_in`), and at the end of the round the list lets
each particle go one by one, each calling its `spite_give_back` (an arena's `free` does nothing, then the arena's
count is given back). The list's buffer, in the arena too, grows by allocating the larger size and copying, so the
smaller buffers stay until the arena goes. `expert.c` frees the round's block in one call and walks nothing.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 64 161 | 178 176 |
| naive C: `naive.c`, `clang -O2` | 82 922 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 059 | 139 776 |

Spite takes 0.77 times naive C's time and 31.16 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=64161 naive=82922 expert=2059 -->
<!-- /timings -->
