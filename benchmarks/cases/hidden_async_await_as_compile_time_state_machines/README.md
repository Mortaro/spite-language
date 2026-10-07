# Hidden async/await as compile-time state machines

A wait is written as an ordinary call, and the compiler compiles every function a `Concurrent` can reach that waits
a second time as a state machine: a frame holding its parameters, locals and temporaries, and a step function that
runs until it finishes or reaches a wait that is not over, and starts with a jump to where it stopped. There are no
stacks to switch and nothing to ship, and a program with no `Concurrent` has none of it.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#hidden-asyncawait-as-compile-time-state-machines).
- The proof: [Which calls suspend a `Concurrent`](../../../docs/proofs.md#which-calls-suspend-a-concurrent).

## The four forms

- [`naive/`](naive/): eight `Napper`s started as `Concurrent`s, each sleeping 2 ms, doubling its number and sleeping
  2 ms again, and the eight answers added up once all are back.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: work that runs at once is a
  thread each, whose stack keeps the napper's locals while it sleeps, joined in order.
- [`expert.c`](expert.c): the state machines written by hand: a frame per napper with its place, number, local and
  deadline, stepped on one thread by a loop that sleeps until the nearest deadline.
- [`generated.c`](generated.c): `nap`'s frame, its start and its step, the sleep's step, and the loop that starts
  the `Concurrent`s and reads them.

## What to look at in generated.c

`Napper_nap___frame` is `nap`'s locals and temporaries as a struct: `doubled_`, the `Program` it calls, and
`spite_wait_1` and `spite_wait_2`, one slot per sleep for the frame of the sleep it waits on.
`Napper_nap___begin` allocates it, and `Napper_nap___step` opens with `switch (spite_frame->spite_head.state)` and a
`goto` to `spite_resume_1` or `spite_resume_2`, so each call carries on from the sleep it stopped at; while
`Program_sleep___step` answers `false` it returns with `SPITE_SUSPEND`. `doubled_` lives in the frame, so it
survives the second sleep with nothing copied. `Program_sleep___step` is the small state machine at the bottom: a
timer the scheduler keeps. `Naive_nap_all` is the plain function: it makes each `Concurrent` and reads each
handle's result, which is where it waits. `naive.c` keeps the same locals on eight thread stacks; `expert.c` writes
the frames and the loop that `generated.c` and the library's scheduler do.

One thing in the step is more than the page says: each sleep counts the napper's `Program` twice, into
`spite_temp_1` and again into `spite_temp_2`, and lets both go after the wait, where the page says a reference kept
across a wait is one count each way.

## Why there is no time

All three programs take as long as their sleeps: about 4 ms of waiting each, longer by the system's timer
granularity, and the work around them is a few microseconds. So compare what each holds while it waits. Measured with
`--debug-memory` on this program and on the same program with 16 nappers:

| Program | Allocations |
|---|---|
| this program, 8 nappers | 100 |
| the same with 16 nappers | 172 |

Nine allocations per napper, of which three are frames: `nap`'s and one per sleep, each the size of its struct. `naive.c`
holds a thread per napper instead, each with a stack the system reserves (1 MB by default on Windows), and
`expert.c` holds eight frames of 24 bytes on its own stack.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 233 472 |
| naive C: `naive.c`, `clang -O2` | not timed | 139 264 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 264 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
