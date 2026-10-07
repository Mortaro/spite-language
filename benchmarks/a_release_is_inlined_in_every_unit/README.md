# A release is inlined in every unit

Letting go of a reference is a count-down and, rarely, the freeing of the object. The compiler writes each class's
retain and release as a small `static inline` function in the header every translation unit includes, with the
freeing out of line, so the count-down is copied into every caller in every unit and only the freeing is a call.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-release-is-inlined-in-every-unit).
- The proof: none of its own; it is a choice of where the C is written, not a fact about the program.

## The four forms

- [`naive/`](naive/): 1 000 movers in a list, every other one kept in a second list, then the first list cleared,
  counting each mover down and freeing the ones not kept.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct and a count per mover,
  one release function that counts down and frees, and a clear that releases every mover. One file, as a program
  this size is written.
- [`expert.c`](expert.c): the same work tuned by hand: the speeds as one array of numbers, the kept ones copied
  into a second, and no counts, so letting go of a mover costs nothing.
- [`highlights.c`](highlights.c): a mover's release and freeing, the list's release of one value, and the loop that
  keeps every other mover.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Mover___release` is `static inline` and holds only the test and the count-down; `Mover___free`, out of line, gives
the mover back to its class's pool. `TypedMemory__Mover_release_value`, which `movers.clear()` calls for each
mover, and every other caller therefore get the count-down copied in. `naive.c` writes the same two paths in one
`static` function and `expert.c` has no release at all.

There is no time here, and none is printed. The optimisation changes something only in a build of several
translation units, which the compiler makes only of a program whose C is bigger than 1.5 MB: this program's C is
98 321 bytes, so it is one unit, and inside one unit the C compiler inlines a release whichever way it is written.
Built from the C `check.sh` writes with `clang -O2 -S`, its assembly calls neither `Mover___release` nor
`Mover___free` anywhere: both are inlined, and the only call left of the class is to `Mover___pool_grow`, from 4
places. What to compare is a build of several units (a game engine, or the compiler compiling itself): there, count
the calls to `<Class>___release` in the objects of one unit, which this form turns into an inline count-down and a
call to `<Class>___free` only when the count reaches zero.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 170 496 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 264 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
