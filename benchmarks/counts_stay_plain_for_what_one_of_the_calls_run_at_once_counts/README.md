# Counts stay plain for what one of the calls run at once counts

A loop over different classes that runs them at once counts with plain arithmetic every class that only one of
the calls counts. Before, every class those calls counted was counted atomically, in the whole program.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#counts-stay-plain-for-what-one-of-the-calls-run-at-once-counts).
- The proof: [What one of the calls run at once counts](../../docs/proofs.md#what-one-of-the-calls-run-at-once-counts).

## The four forms

- [`naive/`](naive/): an `Archive` of 2 000 `Letter`s and a `Gallery` of 2 000 `Painting`s, kept by a
  `Library` in a `List<Collection>` and sorted by a `Sorter` with `library.collections.each_sort()`: 2 000 rounds
  each, every round keeping the objects that match in a fresh list (a retain each) and letting it go (a release
  each). The collections are held by a singleton so that the function value `Benchmark` is handed, whose owner is
  the `Sorter`, reaches none of them when it is let go: a function value's owner is still counted as let go on
  another thread, which would make every class it reaches atomic.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: an object per letter and
  painting with a count in it, a fresh array per round, one collection after the other on the program's one
  thread.
- [`expert.c`](expert.c): the same work tuned by hand: the values in plain arrays, each round only counting the
  matches (the program only ever reads how many there were), the archive on a thread of its own.
- [`highlights.c`](highlights.c): the run on the pool, its piece, `Archive_sort` and the counts of `Letter` and
  `List_Letter`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Letter___retain` and `Letter___release` count with `SPITE_PLAIN_COUNT_UP` and `SPITE_PLAIN_COUNT_DOWN`: only the
archive's `sort` counts a `Letter`, only the gallery's a `Painting`, and the piece that runs them on the pool counts
only the collections. Before this optimisation they used `SPITE_COUNT_UP` and `SPITE_COUNT_DOWN`, a locked
instruction for each of the 2.1 million letters and paintings kept, and again when each is let go. A class both calls counted would be counted
with `SPITE_ROW_COUNT_UP`: atomically only while the loop runs them at once.

## Timings

Measured while the machine was in other use: provisional, to be timed again on a quiet machine.

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 7 267 | 247 296 |
| naive C: `naive.c`, `clang -O2` | 10 400 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 5 226 | 139 264 |

Spite takes 0.70 times naive C's time and 1.39 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=7267 naive=10400 expert=5226 -->
<!-- /timings -->
