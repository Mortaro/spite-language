# Other optimisations

Two entries of the page: an appended item made in place, where `var made = Velocity(...)` followed by
`velocities.append(made)` writes the constructor's attributes straight into the vector's block when the object is
used for nothing else, so filling a vector allocates only when the block grows; and the build report of what
could not be optimised, written only when asked with `--optimization-report=file`, which lists each place that
fell back and the line that lets it go.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#other-optimisations), and the report in
  [docs/compiler.md](../../docs/compiler.md#read-what-was-not-optimised).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the item made in place would stand on
  the object being used for nothing but the append, the fact [Objects that never leave their function live in the
  frame](../../docs/proofs.md#objects-that-never-leave-their-function-live-in-the-frame) establishes and the
  report's "Objects not in the frame" lists where it fails.

## The four forms

- [`naive/`](naive/): a thousand velocities, each made into a local and appended to a `Vector<Velocity>`, and the
  sums of both attributes.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite, doing what the compiler does
  today: each velocity made on the heap, copied into a growable array of velocities held inline, and freed.
- [`expert.c`](expert.c): each velocity's two numbers written straight into an array sized once, which is what
  the item made in place would do.
- [`highlights.c`](highlights.c): `Naive` and the vector's `append`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The item made in place is not built, and this case shows it: `Naive_Naive` makes each velocity with
`Velocity___make((index_ % 7), (index_ % 5))`, passes it to `Vector__Velocity_append`, which copies it into the
block with `InlineMemory__Velocity_write_item`, and releases it after the append, so each of the thousand items is
an object for a moment. `naive.c` does the same with `malloc`, a copy and `free`; `expert.c` writes the numbers
into the array and makes no object.

The report says why, at the line that makes it. `spite benchmarks/other_optimisations/naive --check
--optimization-report=report.md` writes (the same with `--optimized`):

```
## Lists that hold references (1)

- library/allocation_table.spite:83: `List<Printable>` keeps a reference to each item: an item is a Printable,
  which a list holds by reference

## Lists not in the frame (0)

## Objects not in the frame (1)

- benchmarks/other_optimisations/naive/naive.spite:7: `made` (`Velocity`) is made on the heap: line 8 lets
  it go: `velocities.append(made)`

## Copies not elided (0)
```

The second entry is the item that would be made in place. The first is not this program's: it is a line of the
standard library's allocation table, which this build does not contain (its C names no `AllocationTable`), so it
is a line no one reading the report can act on.

## Why there is no time

The report is written while compiling and changes nothing in the program built, and the item made in place is
not built yet, so there is nothing of either to time: the Spite program runs as `naive.c` does. What to compare is
the allocations, which `--debug-memory` counts, and the report itself above:

| Program | Allocations |
|---|---|
| `naive/` under `--debug-memory` | 1 004, of which 1 000 are the velocities |
| `naive.c` | 1 000 velocities, the vector and its block |
| `expert.c` | 1: the array |

Once the item made in place is built, `naive/` should count 4 and nothing per velocity, and the
report's "Objects not in the frame" should be empty.

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
