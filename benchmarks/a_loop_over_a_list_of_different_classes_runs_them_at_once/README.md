# A loop over a list of different classes runs them at once

A loop that calls one function on every element of a list of a `type` runs the elements on the thread pool at
once when their classes share nothing one of them writes. The compiler works out which classes may run together
while compiling, as a table, and the loop reads its elements' classes when it starts and checks them against it.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-loop-over-a-list-of-different-classes-runs-them-at-once).
- The proof: [Classes in a list that share nothing written](../../docs/proofs.md#classes-in-a-list-that-share-nothing-written).

## The four forms

- [`naive/`](naive/): three voices of three classes, `Sine`, `Square` and `Saw`, each with a `render()` that steps
  its own phase 30 million times into its own `level`, kept in a `List<Voice>` and rendered with
  `voices.each_render()`, with no `Parallel` written.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one struct with a function
  pointer per voice, kept in an array, each rendered in turn on the program's one thread.
- [`expert.c`](expert.c): the same work tuned by hand: two voices on threads of their own and the third on the
  program's own thread, each keeping its phase and level in locals and storing them once, on a cache line of its
  own.
- [`highlights.c`](highlights.c): the loop, the run on the pool, its piece and one voice's `render`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_render_all` reads the class of every voice in the list (`.tag`, three cases) and checks every two against
`spite_row_table`, written while compiling: `1` where two classes' `render`s are independent, `0` on the diagonal,
so that a list holding one voice twice stays in order. `spite_row_heavy` says which of them reach a loop. When the
check passes it calls `List_Naive_Voice_spite_row_render`, which hands the voices to `ThreadPool_run_marked`: every
marked one but the last goes to the pool, the rest run on the program's thread, and all are joined; otherwise the
`else` branch is the plain `List_Naive_Voice_each_render`. `Sine_render` is the plain loop, writing only its own
object's `phase_` and `level_`. `naive.c` renders the three one after the other; `expert.c` runs them on three
threads as the Spite does, without its overflow checks.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 90 568 | 226 304 |
| naive C: `naive.c`, `clang -O2` | 284 023 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 96 497 | 139 776 |

Spite takes 0.32 times naive C's time and 0.94 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=90568 naive=284023 expert=96497 -->
<!-- /timings -->
