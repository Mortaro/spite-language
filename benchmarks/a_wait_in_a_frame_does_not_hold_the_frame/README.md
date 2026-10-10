# A wait in a frame does not hold the frame

A frame loop that calls `saver.save_part()` on each of its first eight frames, where `save_part` sleeps twenty
milliseconds (standing for a slow disk or a server) and then writes a file. Written plainly, every one of those
frames would wait for its save. The compiler starts each call as a `Concurrent` instead, so the frames go on drawing
while the saves wait, and all eight are in flight at once.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-wait-in-a-frame-does-not-hold-the-frame).
- The proof: [A wait in a frame is started](../../docs/proofs.md#a-wait-in-a-frame-is-started), built on
  [Whether a function waits](../../docs/proofs.md#whether-a-function-waits).

## The four forms

- [`naive/`](naive/): sixty frames, each adding up 20 000 numbers and sleeping a millisecond, the first eight each
  saving a part through `saver.save_part()`; then it waits until all eight are saved.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `save_part` sleeps and writes
  before it returns, so each of the first eight frames waits for it.
- [`expert.c`](expert.c): the same work tuned by hand: each part is saved on a thread of its own started on its
  frame, and the eight threads are joined after the sixty frames.
- [`highlights.c`](highlights.c): the frame loop with the started call, `WaitsInFlight__Nothing__keep`, and the
  state machine `save_part` runs as.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_run_frames`, `saver.save_part()` is a block that holds the receiver, makes
`Concurrent__Nothing___make(spite_function_value_Saver_save_part(...))` and hands it to
`WaitsInFlight__Nothing__keep`, so the frame carries on to its `Program_sleep`. `Saver_save_part___step` is the
state machine: it takes the part's number, stops at the sleep (`SPITE_SUSPEND(1)`), and at the file write, and the
frame loop's sleeps run it on. `WaitsInFlight__Nothing__keep` lets go of the handles that have finished and keeps
the new one, waiting for the oldest only when a ninth from the same line would be in flight.

Every form here is held to the system's timer: a one-millisecond sleep takes about fifteen on Windows, so a frame
is about fifteen milliseconds whatever it draws. `naive.c` pays eight more frames' worth of saving on top;
`expert.c` and the Spite overlap the saves with the frames.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 933 233 | 274 944 |
| naive C: `naive.c`, `clang -O2` | 1 186 557 | 152 064 |
| expert C: `expert.c`, `clang -O2` | 928 354 | 152 064 |

Spite takes 0.79 times naive C's time and 1.01 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=933233 naive=1186557 expert=928354 -->
<!-- /timings -->
