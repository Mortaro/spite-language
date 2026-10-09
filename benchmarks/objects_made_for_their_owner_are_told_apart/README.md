# Objects made for their owner are told apart

Two calls that each keep an object of one class of their own, made where it is given, run at once: what one does
to its meter, and to the list inside it, is told apart from what the other does to its own.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#objects-made-for-their-owner-are-told-apart).
- The proof: [An object made for its owner is told apart](../../docs/proofs.md#an-object-made-for-its-owner-is-told-apart).

## The four forms

- [`naive/`](naive/): two tracks of two classes, `Drum` and `Bass`, each with a `var meter = Meter()` of its own
  that `render()` records 20 million samples into (a total, a peak and a list of the loud ones), kept in a
  `List<Track>` and rendered with `tracks.each_render()`, with no `Parallel` written.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one struct with a function
  pointer per track, each pointing at a meter of its own, rendered in turn on the program's one thread.
- [`expert.c`](expert.c): the same work tuned by hand: the drum on a thread of its own and the bass on the
  program's own thread, each meter on a cache line of its own with its total and peak kept in locals.
- [`highlights.c`](highlights.c): the loop's table, the run on the pool, its piece, `Drum_render` and
  `Meter_record`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_render_all` reads the class of each track and checks every two against `spite_row_table`: the `Drum` and the
`Bass` are together (`1`). Before this optimisation both calls "wrote `Meter`", since a meter was told apart only by
its class, and the table had no pair, so the loop was the plain `List_Naive_Track_each_render`. `Drum_render` passes
`self->meter_` to `Meter_record`, and the compiler follows the meter into it: `Meter`'s `total_`, `peak_` and the
items of its `loud_` list are counted as the drum's meter's, which no other attribute holds, because `meter` is
only ever given a `Meter()` made where it is given.

## Timings

Measured while the machine was in other use: provisional, to be timed again on a quiet machine.

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 68 950 | 235 008 |
| naive C: `naive.c`, `clang -O2` | 124 895 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 65 094 | 139 776 |

Spite takes 0.55 times naive C's time and 1.06 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; measured while the machine was in other use, provisional.
<!-- measured spite=68950 naive=124895 expert=65094 -->
<!-- /timings -->
