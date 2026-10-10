# What a `--hot-reload` build carries so its objects can move

A `--hot-reload` build lets a class gain or lose attributes while the program runs, so each object of a program
class carries two hidden words after its header, joins its class's list of live objects when it is made and leaves
it when it is freed, and every function that reads its attributes from outside the class is called through a slot.
This is a cost paid only in that build: a production build's object is its header and its attributes, and its
templates call and read directly.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#what-a---hot-reload-build-carries-so-its-objects-can-move).
- The proof: [A reload reaches every object whose attributes it changes](../../docs/proofs.md#a-reload-reaches-every-object-whose-attributes-it-changes),
  which is what the two words and the live lists exist for.

## The four forms

- [`naive/`](naive/): a thousand particles in a list, twenty rounds of stepping each one, and the sum of their
  positions.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per particle that
  holds its two numbers and nothing else, `malloc` per particle, a growable array of pointers.
- [`expert.c`](expert.c): the positions and speeds as two arrays of numbers on the stack.
- [`highlights.c`](highlights.c): `struct Particle`, its allocation and `List<Particle>.each_step`, as the
  `--optimized` build writes them.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`struct Particle` is `SpiteHeader header;` (a count and a class id, 8 bytes) and the two attributes: 16 bytes.
`Particle___allocate` takes the object from the class's pool and does nothing else, and `List_Particle_each_step`
reads each item as it lies in the list and calls `Particle_step` directly. The `--hot-reload` build of the same
program (written with `--check --c-source --hot-reload`, never run) writes instead:

```c
struct Particle {
SpiteHeader header;
void* spite_moved;
int64_t spite_live;
int32_t position_;
int32_t speed_;
};
```

32 bytes, with `self->spite_moved = 0; self->spite_live = 0;` and `spite_live_add(Particle___live(), self,
&self->spite_live);` in `Particle___allocate___hot`, `spite_live_remove` in its release, a `Particle___layout()`
table naming each attribute's offset and size, and `List_Particle_each_step` a forwarder,
`__atomic_load_n(&List_Particle_each_step___slot, __ATOMIC_ACQUIRE)(self);`, in front of a `___hot` body that reads
each item through `TypedMemory__Particle_read_value`, itself called through a slot, and counts and releases it.
`naive.c`'s struct is the production build's without the header, and `expert.c` has no objects at all.

## Why there is no time

What the build carries is in the `--hot-reload` build alone, which is never measured: the production build the
three programs are compared in has none of it. What to compare is the object and the code around it, between the
two builds' C, measured with `grep` on what `--check --c-source --optimized` and `--check --c-source --hot-reload`
wrote from `naive/`:

| Build | Bytes per `Particle` | Bytes for the thousand particles | Lines of C that call through a slot |
|---|---|---|---|
| `--optimized` (production) | 16 | 16 000 | 0 |
| `--hot-reload` | 32 | 32 000 | 2 564 |

The 2 564 lines that call through a slot are every function of the program, the library and the compiler's
helpers, since a reload may replace any of them ([REPL, live reload and debug machinery only in those
builds](../repl_live_reload_and_debug_machinery_only_in_those_builds/)); the two words and the live list are
the part this section adds, and `spite_live_add` appears in 18 lines of the `--hot-reload` C.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 173 056 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 138 752 |

Measured 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured untimed -->
<!-- /timings -->
