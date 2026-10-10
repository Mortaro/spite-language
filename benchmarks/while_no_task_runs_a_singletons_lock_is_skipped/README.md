# While no task runs, a singleton's lock is skipped

Every function a singleton's lock wraps first asks whether any work is on the thread pool, and while none is, only
the program's own thread runs the program, so the function runs without the lock. It notes that it skipped the
lock, and a task it starts before it returns takes the lock first, so nothing a task can see differs.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#while-no-task-runs-a-singletons-lock-is-skipped).
- The proof: none; it is a run-time test of a counter, one load per call, not a proof
  ([docs/proofs.md](../../docs/proofs.md#a-counted-loop-takes-a-singletons-lock-once) says so below the counted
  loop's entry).

## The four forms

- [`naive/`](naive/): one `Parallel` calls `tally.add(1)` and is read, so the `Tally` singleton is one a `Parallel`
  reaches and is locked; then the program's own thread calls `tally.add` ten million times, with no task on the
  pool, in a loop whose index each call answers (so it is not a counted loop that would take the lock once).
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the tally is shared with a
  thread, so it sits behind a mutex that every call takes, the ten million calls after the thread was joined too.
- [`expert.c`](expert.c): the same work tuned by hand: once the thread is joined only the program's thread touches
  the tally, so the calls are two additions in registers each, stored once at the end.
- [`highlights.c`](highlights.c): the loop, the locked `add` with its skip, its body, and how the pool counts tasks.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_add_many`'s loop is not counted (its index is what `tally.add` answers), but it calls only `Tally` and can
start no task, so it [tests for tasks once](../../docs/optimizations.md#a-loop-of-calls-to-one-singleton-tests-for-tasks-once):
`spite_coarse_1_skip_enter()` finds `spite_tasks_in_flight` zero, pushes `&Tally___guard` on `spite_skipped` and
answers 1, and every call is then `Tally_add___unguarded`, which the C compiler inlines into the loop it keeps for
that case; `spite_coarse_1_skip_leave` pops it. With a task in flight the loop would call `Tally_add`, whose own
test takes `spite_guard_enter`. `ThreadPool__task_begun` adds one to `spite_tasks_in_flight` for every task handed
out, after taking every lock its thread skipped (`spite_enter_skipped`), and `ThreadPool__task_ended` takes it away.
The body adds to `total_` and `calls_` plainly: each is written once by `add`, so each is atomic on its own, but no
other class reads them past the lock, so `Tally___atomic` is 0 (in `generated.c`) and its `if (Tally___atomic)`
tests fold away ([its section](../../docs/optimizations.md#an-attribute-read-only-under-its-singletons-lock-is-a-plain-number)).
What is left between the Spite and `expert.c` is that `total_` and `calls_` are loaded and stored in memory on every
call, with an overflow test each, where `expert.c` keeps them in registers. `naive.c` takes and lets go of its mutex
on every call.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 5 270 | 239 616 |
| naive C: `naive.c`, `clang -O2` | 52 234 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 219 | 139 264 |

Spite takes 0.10 times naive C's time and 2.37 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=5270 naive=52234 expert=2219 -->
<!-- /timings -->
