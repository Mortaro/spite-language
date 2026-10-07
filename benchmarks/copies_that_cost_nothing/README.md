# Copies that cost nothing

`var moved = origin.copy()` reads as a new `Point` on the heap for every trial, since every class is a reference
and `copy()` is how a program asks for an independent one. The copy never leaves `all_trials`, so the compiler
makes it a slot in the function's frame filled by copying the two numbers, and the calls on it are passed the slot
with no count, so the loop allocates nothing.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#copies-that-cost-nothing).
- The proof: none of its own; the part shown here stands on
  [Frame objects](../../docs/proofs.md#objects-that-never-leave-their-function-live-in-the-frame) (a copy used as a
  value is one) and [Held arguments](../../docs/proofs.md#an-argument-its-caller-holds-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): a program's own class of two `Integer`s, `Point`, and 20 000 000 trials that each copy one
  origin, move the copy by an amount that depends on the trial, and add up its distance from the origin; the
  origin itself never changes.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct for `Point`, each
  copy a `malloc` and a `memcpy`, freed at the end of its trial.
- [`expert.c`](expert.c): the same work tuned by hand: the copy is two locals in registers.
- [`highlights.c`](highlights.c): what the compiler writes for `all_trials` and for a copy made into a slot.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_all_trials___held_0` the copy is `Point spite_slot_<n>;` and `Point___copy_into(origin_, &spite_slot_<n>)`,
which sets the slot up as a frame object and copies the attributes into it (`Point___copy_fields`): no heap
allocation, and `Point_move_by(moved_, ...)` and `Point_distance_from_origin(moved_)` are passed the slot with no
`Point___retain` or `Point___release` around them. Under `--debug-memory` the whole program makes 6 allocations.
`naive.c` mallocs each copy, but clang at `-O2` inlines the copy, the move and the measure and removes that
`malloc` and its `free` itself (`clang -O2 -S` of `naive.c` keeps only the origin's `malloc`), so its time is close
to `expert.c`'s; what is left between `naive/` and them is the overflow check on each `+` and `-`.

## What is not built yet

The section describes more than this program can show, and most of it is planned:

- a copy used only once, passed by value instead of being allocated;
- a copy that is never changed sharing the original, when that is cheaper;
- every object that never escapes laid out inline or in registers (built only for frame objects, as here);
- reference counting left out wherever ownership is provable (built only for frame objects passed to a function
  that never assigns the parameter, and for arguments a caller holds);
- a class holding text, lists or other objects as a result written into the caller's slot, a temporary or a copy
  used as a value (as a local in the frame it is built);
- an attribute object laid inline in a frame-held object where the attribute is never shared.

What is built is what this case shows (a copy of a class of numbers used as a value is a frame slot, and the calls
on it are not counted) and the allocator set right after construction, which has
[its own case](../an_allocator_set_after_construction_is_where_the_object_is_made/).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 36 787 | 173 568 |
| naive C: `naive.c`, `clang -O2` | 24 689 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 17 688 | 139 776 |

Spite takes 1.49 times naive C's time and 2.08 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=36787 naive=24689 expert=17688 -->
<!-- /timings -->
