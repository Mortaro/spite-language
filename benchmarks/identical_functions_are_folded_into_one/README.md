# Identical functions are folded into one

`Position` and `Velocity` are two classes with the same attributes, so `List<Position>` and `List<Velocity>`, their
member templates and the classes' own constructors are the same code over the same layout. The compiler keeps one
of each such group and sends every call of the others to it, so the program carries each function once.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#identical-functions-are-folded-into-one).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); two functions fold only when they are the
  same statements over types of the same layout, so the one kept does exactly what the other would.

## The four forms

- [`naive/`](naive/): a thousand positions and a thousand velocities in two lists, and the sums of both attributes
  of both.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: two structs, and each list,
  its `append` and its two sums written once per struct.
- [`expert.c`](expert.c): one struct for the shared layout, and one function of each kind for both arrays.
- [`highlights.c`](highlights.c): the pointers folding writes, `Naive`, and the one `sum_across` both lists use.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Each `static __typeof__(&X) spite_folded_X = ((__typeof__(&X))&Y);` line says that `X` is not written: its calls go
to `Y`. `List_Velocity_sum_across` is `List_Position_sum_across`, `Velocity_Velocity` is `Position_Position`, and
both lists' `count` are the one `List_Console_Printable_count` the program already had for printing. `Naive` calls
`spite_folded_List_Velocity_sum_across(velocities_)`, a pointer nothing writes, which the C compiler turns back into
a direct call. This program folds 11 functions.

## Why there is no time

Folding changes the size of the program, not its speed: the C compiler makes the call through the folded pointer
direct again. So compare the executable sizes below and the functions: `naive.c` writes six functions twice over,
`expert.c` writes each once, and the compiler arrives at `expert.c`'s count from `naive/`'s source. At this size the
saving is a few hundred bytes of machine code; it grows with every generic class made for several classes of one
layout, and with every package loaded in two versions.

## Executable size

`bash scripts/executable_size.sh [compiler] [program ...]` builds each program with `--optimized` and prints the
executable's size and how many functions its C defines, so two compilers can be compared. When folding was built,
on Windows with clang, before and after:

| program | bytes before | bytes after | functions before | functions after |
|---|---|---|---|---|
| the compiler (`bootstrap`) | 9 211 392 | 9 197 056 | 4 894 | 4 614 |
| a program of sparse columns over generic classes | 246 784 | 244 224 | 437 | 404 |
| a program of walked rows over generic classes | 211 968 | 210 944 | 277 | 259 |

About 6% of the functions fold away, but an `-O3` build had already inlined most of the small ones they are, so the
executable shrinks by 0.2% to 1%. The folding pays most where whole functions are duplicated: two versions of one
dependency, and generic classes over classes of the same layout.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 175 616 |
| naive C: `naive.c`, `clang -O2` | not timed | 139 264 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 264 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
