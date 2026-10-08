# Singletons a `Parallel` reaches take a lock

In a program that makes a `Parallel`, a singleton of the program's own that can change after it is made, and that no
cheaper form makes safe, gets a lock of its own, taken around each of its functions that touches what can change.
You write no lock: the source is the same as in a program with one thread, and the compiler keeps the lock cheap
(a cache line of its own, calls to itself unlocked, functions that touch nothing that changes left unlocked).

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#singletons-a-parallel-reaches-take-a-lock).
- The proof: [A function that touches no changing state takes no lock](../../docs/proofs.md#a-function-that-touches-no-changing-state-takes-no-lock),
  with [Which singletons a `Parallel` reaches](../../docs/proofs.md#which-singletons-a-parallel-reaches) deciding
  which singletons need one at all.

## The four forms

- [`naive/`](naive/): four `Parallel` recorders, each recording a weight a quarter of a million times into one
  `Registry` singleton that appends it to a list and adds it to a total, then the program reads the count and the
  total back.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one registry behind a mutex,
  four threads, every call to `record` taking and letting go of the mutex.
- [`expert.c`](expert.c): the same work tuned by hand: the list made once at its final size, each thread writing its
  weights into a quarter of it and adding them up in a register, with no lock.
- [`highlights.c`](highlights.c): the lock's type, the recorder's loop, the locked `record` and its unlocked body, and
  how the lock is taken and let go.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Recorder_record_all` calls `Registry_record` once per weight. `Registry_record` first tests
`spite_tasks_in_flight`, which is not zero here while the recorders run, so it takes
`spite_guard_enter(&Registry___guard)`, calls `Registry_record___unguarded` and lets go with `spite_guard_leave`.
`SpiteGuard` is aligned to 64 bytes, so the lock has a cache line of its own. `Registry` holds a `List`, so it can
take none of the cheaper forms; its `total` is written once by `record`, so it is also atomic on its own
(`Registry___atomic` is 1 and the body adds it with `__atomic_fetch_add`), which lets another class read it without
the lock. The loop is not [one counted loop of calls](../a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/)
because it also calls `weight_of`, a function of the recorder's own, so each call takes the lock.

`spite_guard_enter` takes the lock with one compare-and-swap when it is free. When another thread holds it,
`spite_guard_wait` waits with `spite_spin_pause()` (the processor's `pause`), 1, 2, 4 and up to 1 024 of them
between looks, reads the lock before it tries the compare-and-swap again, and past that calls `SwitchToThread()`
(`sched_yield()` off Windows) between looks. The plain compare-and-swap loop it replaced made every waiting thread
pull the lock's line to its own core at every try, so the thread holding the lock lost it too: this program took
113 ms that way, against 7.7 ms backing off, and `naive.c`'s `CRITICAL_SECTION`, which spins briefly and then
waits in the system, takes 21.5 ms. `expert.c` takes no lock.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 7 700 | 232 960 |
| naive C: `naive.c`, `clang -O2` | 21 467 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 525 | 139 776 |

Spite takes 0.36 times naive C's time and 14.67 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=7700 naive=21467 expert=525 -->
<!-- /timings -->
