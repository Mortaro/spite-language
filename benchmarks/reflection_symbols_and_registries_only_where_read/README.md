# Reflection, symbols and registries only where read

Every class could answer `.instances`, but the compiler knows which classes a program asks, and only those keep a
registry of their live objects. Here the program asks `Lamp.instances` and never `Door.instances`, so making a lamp
records it and making a door records nothing; the same goes for every other piece of reflection, which is emitted
only where something reads it.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#reflection-symbols-and-registries-only-where-read).
- The proof: none of its own; what is left out is what [tree shaking](../../docs/proofs.md#tree-shaking-what-main-can-reach)
  finds nothing reaching, and which classes are asked for their instances is read off the program while compiling.

## The four forms

- [`naive/`](naive/): a thousand lamps and a thousand doors, how many of each are lit or open, and how many lamps
  are alive, asked of `Lamp.instances`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per object, and a registry of the live lamps that making a lamp adds to and freeing one removes from. The doors
  have none, since nothing asks.
- [`expert.c`](expert.c): lamps and doors as arrays of flags, and the live lamps kept as a count, the only thing
  asked of them.
- [`highlights.c`](highlights.c): the `#define SPITE_TRACKS_` lines of the whole program, Lamp's registry and the
  function that records into it, both classes' `___allocate`, and `Lamp___instances`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

There is one `#define SPITE_TRACKS_` in the whole program, `SPITE_TRACKS_Lamp`. Every class's `___allocate` and
`___free` are written with the same `#ifdef SPITE_TRACKS_<class>` around the call that records it, so
`Lamp___allocate` calls `spite_track_Lamp` while the `#ifdef` in `Door___allocate` leaves nothing, and no
`spite_track_Door` or `spite_instances_Door` exists. `naive.c` does the same by hand, and `expert.c` keeps a count.
`Lamp___instances` answers a new list of the live lamps, each one counted, so `Lamp.instances.count()` builds a list
of a thousand to read its count.

There is no time: what the optimisation saves is the registry of every class nothing asks about, and a person
writing C does not build those to begin with, so the three programs do the same work. What to compare is the C the
compiler writes for this program and for two variants of it, each built with `--check --c-source --optimized` and
the executable with `clang -O2` (Windows, `wc -l -c` and `ls -l`):

| Program | Lines of C | Bytes of C | Executable bytes |
|---|---|---|---|
| no class asked for its instances (`lamps.count()` instead) | 2 191 | 110 789 | 173 568 |
| this program: `Lamp.instances` | 2 224 | 112 872 | 175 616 |
| `Lamp.instances` and `Door.instances` | 2 260 | 114 811 | 177 152 |

Each class asked about adds its registry, its two recording functions and its `___instances`, about 35 lines of C
and 2 kB of executable; a class nobody asks about adds none.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 179 200 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 776 |

Measured 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured untimed -->
<!-- /timings -->
