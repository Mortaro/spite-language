# A loop whose passes write only their own item runs in bands

A loop that calls one function on every element of a list of a class runs on the thread pool in bands, one for
each thread, when every pass writes only its own element and reads nothing another pass writes. The compiler
proves it while compiling, weighs the function and writes the smallest count at which bands pay; the loop compares
the list's count with it, and checks that no element is held twice, when it starts.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-loop-whose-passes-write-only-their-own-item-runs-in-bands).
- The proof: [Passes that write only their own item](../../docs/proofs.md#passes-that-write-only-their-own-item).

## The four forms

- [`naive/`](naive/): 100 000 orbits in a `List<Orbit>`, each with an `advance()` that steps its own angle 1000
  times and counts its own turns, advanced ten times with `orbits.each_advance()`, with no `Parallel` written.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct allocated per orbit,
  an array of pointers, and the orbits advanced one after the other on the program's one thread.
- [`expert.c`](expert.c): the same work tuned by hand: the orbits side by side in one array, one band per
  processor on a thread of its own, each band running all ten ticks of its orbits back to back with the angle and
  the count in registers.
- [`highlights.c`](highlights.c): the loop, the bands, their piece and `advance`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_advance_ticks` compares the list's count with the smallest count at which bands pay, a constant the compiler worked
out from what `advance` weighs (its loop of 1000 steps), and checks that every orbit is held by the list alone
(`ref_count == 1`). Then `List_Orbit_spite_band_advance` hands its piece to `ThreadPool_run_bands`, which cuts the
list into a band for each thread, runs the first on the program's thread and waits for the rest; otherwise the
`else` branch is the plain `List_Orbit_each_advance`. `Orbit_advance` is the plain loop, writing only its own
object's `angle_` and `turns_`. `naive.c` advances the orbits one after the other; `expert.c` runs them in bands on
every processor as the Spite does, without its overflow checks and with the ten ticks fused.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 32 213 | 228 352 |
| naive C: `naive.c`, `clang -O2` | 762 239 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 30 372 | 140 800 |

Spite takes 0.04 times naive C's time and 1.06 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=32213 naive=762239 expert=30372 -->
<!-- /timings -->
