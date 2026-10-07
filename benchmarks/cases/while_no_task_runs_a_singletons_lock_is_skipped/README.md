# While no task runs, a singleton's lock is skipped

Every function a singleton's lock wraps first asks whether any work is on the thread pool, and while none is, only
the program's own thread runs the program, so the function runs without the lock. It notes that it skipped the
lock, and a task it starts before it returns takes the lock first, so nothing a task can see differs.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#while-no-task-runs-a-singletons-lock-is-skipped).
- The proof: none; it is a run-time test of a counter, one load per call, not a proof
  ([docs/proofs.md](../../../docs/proofs.md#a-counted-loop-takes-a-singletons-lock-once) says so below the counted
  loop's entry).

## The four forms

- [`naive/`](naive/): one `Parallel` calls `tally.add(1)` and is read, so the `Tally` singleton is one a `Parallel`
  reaches and is locked; then the program's own thread calls `tally.add` ten million times, with no task on the
  pool, in a loop whose index each call answers (so it is not a counted loop that would take the lock once).
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the tally is shared with a
  thread, so it sits behind a mutex that every call takes, the ten million calls after the thread was joined too.
- [`expert.c`](expert.c): the same work tuned by hand: once the thread is joined only the program's thread touches
  the tally, so the calls are two additions in registers each, stored once at the end.
- [`generated.c`](generated.c): the loop, the locked `add` with its skip, its body, and how the pool counts tasks.

## What to look at in generated.c

`Tally_add` opens with `__atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0`: with no task in flight it
pushes `&Tally___guard` on `spite_skipped`, calls `Tally_add___unguarded` and pops it, and only otherwise takes
`spite_guard_enter`. `ThreadPool__task_begun` adds one to `spite_tasks_in_flight` for every task handed out, after
taking every lock its thread skipped (`spite_enter_skipped`), and `ThreadPool__task_ended` takes it away. So the ten
million calls each make one load of a counter no thread writes, and no compare-and-swap. The body still adds to
`total_` and `calls_` with `__atomic_fetch_add`, because `Tally___atomic` is 1 (each is written once by `add`, so
each is atomic on its own): two locked additions per call are what is left between the Spite and `expert.c`.
`naive.c` takes and lets go of its mutex on every call.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 52 634 | 229 376 |
| naive C: `naive.c`, `clang -O2` | 76 369 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 806 | 139 264 |

Spite takes 0.69 times naive C's time and 18.76 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=52634 naive=76369 expert=2806 -->
<!-- /timings -->
