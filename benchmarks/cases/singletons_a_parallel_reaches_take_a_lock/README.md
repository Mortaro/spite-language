# Singletons a `Parallel` reaches take a lock

In a program that makes a `Parallel`, a singleton of the program's own that can change after it is made, and that no
cheaper form makes safe, gets a lock of its own, taken around each of its functions that touches what can change.
You write no lock: the source is the same as in a program with one thread, and the compiler keeps the lock cheap
(a cache line of its own, calls to itself unlocked, functions that touch nothing that changes left unlocked).

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#singletons-a-parallel-reaches-take-a-lock).
- The proof: [A function that touches no changing state takes no lock](../../../docs/proofs.md#a-function-that-touches-no-changing-state-takes-no-lock),
  with [Which singletons a `Parallel` reaches](../../../docs/proofs.md#which-singletons-a-parallel-reaches) deciding
  which singletons need one at all.

## The four forms

- [`naive/`](naive/): four `Parallel` recorders, each recording a weight a quarter of a million times into one
  `Registry` singleton that appends it to a list and adds it to a total, then the program reads the count and the
  total back.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one registry behind a mutex,
  four threads, every call to `record` taking and letting go of the mutex.
- [`expert.c`](expert.c): the same work tuned by hand: the list made once at its final size, each thread writing its
  weights into a quarter of it and adding them up in a register, with no lock.
- [`generated.c`](generated.c): the lock's type, the recorder's loop, the locked `record` and its unlocked body, and
  how the lock is taken and let go.

## What to look at in generated.c

`Recorder_record_all` calls `Registry_record` once per weight. `Registry_record` first tests
`spite_tasks_in_flight`, which is not zero here while the recorders run, so it takes
`spite_guard_enter(&Registry___guard)`, calls `Registry_record___unguarded` and lets go with `spite_guard_leave`.
`SpiteGuard` is aligned to 64 bytes, so the lock has a cache line of its own. `Registry` holds a `List`, so it can
take none of the cheaper forms; its `total` is written once by `record`, so it is also atomic on its own
(`Registry___atomic` is 1 and the body adds it with `__atomic_fetch_add`), which lets another class read it without
the lock. The loop is not [one counted loop of calls](../a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/)
because it also calls `weight_of`, a function of the recorder's own, so each call takes the lock.

`spite_guard_enter` is a spin lock: a compare-and-swap in a loop with no pause and no fall back to waiting in the
system. With four threads calling it all the time, the lock's line goes from core to core and the threads spin
while they wait; one run of the `-O2` builds `scripts/cases/check.sh` makes took 134 ms for the Spite against
43 ms for `naive.c`, whose `CRITICAL_SECTION` spins briefly and then waits in the system. `expert.c` takes no lock.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 113 095 | 231 424 |
| naive C: `naive.c`, `clang -O2` | 23 851 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 578 | 139 776 |

Spite takes 4.74 times naive C's time and 195.67 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=113095 naive=23851 expert=578 -->
<!-- /timings -->
