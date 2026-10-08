# Other optimisations

Two entries of the page: an appended item made in the frame, where `var made = Velocity(...)` followed by
`velocities.append(made)` makes `made` in the frame and `append` copies its attributes into the vector's block, so
filling a vector allocates only when the block grows; and the build report of what could not be optimised,
written only when asked with `--optimization-report=file`, which lists each place that fell back and the line that
lets it go.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#other-optimisations), and the report in
  [docs/compiler.md](../../docs/compiler.md#read-what-was-not-optimised).
- The proof: [Objects that never leave their function live in the
  frame](../../docs/proofs.md#objects-that-never-leave-their-function-live-in-the-frame): a `Vector`'s `append`
  keeps nothing of the object it is given, since writing an item copies its attributes, so a local used for
  nothing but the append stays in the frame, and the report's "Objects not in the frame" lists where it fails.

## The four forms

- [`naive/`](naive/): a thousand velocities, each made into a local and appended to a `Vector<Velocity>`, and the
  sums of both attributes.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each velocity made on the
  heap, copied into a growable array of velocities held inline, and freed.
- [`expert.c`](expert.c): each velocity's two numbers written straight into an array sized once.
- [`highlights.c`](highlights.c): `Naive` and the vector's `append`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_Naive` makes each velocity with `Velocity___make_into(&spite_slot_<n>, (index_ % 7), (index_ % 5))`, a
struct in its frame, and passes it to `Vector__Velocity_append`, which copies its attributes into the block with
`InlineMemory__Velocity_write_item`; nothing is allocated for it. `naive.c` makes each with `malloc`, copies it
and frees it; `expert.c` writes the numbers into the array and makes no object. Before `append` was known to keep
nothing, each of the thousand items was an object on the heap for a moment.

The report says what is left, at the line that makes it. `spite benchmarks/other_optimisations/naive --check
--optimization-report=report.md` writes (the same with `--optimized`):

```
## Lists that hold references (1)

- library/console.spite:18: `List<Printable>` keeps a reference to each item: an item is a Printable, which a
  list holds by reference

## Lists not in the frame (0)

## Objects not in the frame (0)

## Copies not elided (0)
```

The list is linked at the first line that needs it in a function this build keeps. It was linked at the first
line that needed it at all, a line of the standard library's allocation table, which this build does not contain.

## Why there is no time

The report is written while compiling and changes nothing in the program built, and a thousand items are too few
to time. What to compare is the allocations, which `--debug-memory` counts, and the report itself above:

| Program | Allocations |
|---|---|
| `naive/` under `--debug-memory` | 4, none for the velocities (1 004 before) |
| `naive.c` | 1 000 velocities, the vector and its block |
| `expert.c` | 1: the array |

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 169 984 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 138 752 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
