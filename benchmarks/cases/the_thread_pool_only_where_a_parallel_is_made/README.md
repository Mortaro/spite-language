# The thread pool only where a `Parallel` is made

`ThreadPool` is a singleton made the first time a `Parallel` (or a `parallel_each_` pass) needs it, and it starts its
worker threads then, once. A program that never makes one starts no thread and carries none of the pool's code.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#the-thread-pool-only-where-a-parallel-is-made).
- The proof: none of its own; it is a singleton
  [made on first use](../../../docs/optimizations.md#singletons-made-on-first-use-never-counted), and a program with
  no `Parallel` drops it under [Tree shaking](../../../docs/proofs.md#tree-shaking-what-main-can-reach).

## The four forms

- [`naive/`](naive/): one `Parallel` adds up `index % 7` over the even numbers below 20 million while the program's
  own thread adds up the odd ones, and the two sums are added.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a thread made for the one piece
  of work that runs beside the program's thread, and joined where its answer is read.
- [`expert.c`](expert.c): the same work tuned by hand: one thread for the even half, each half summed into a
  register and stored once on a cache line of its own.
- [`generated.c`](generated.c): the function that makes the `Parallel`, the pool's fetch, its `submit` and its
  `start`.

## What to look at in generated.c

`Naive_sum_on_two` makes the `Parallel` with `Parallel__Long___make`, whose defaults fetch the pool
(`self->_pool_ = spite_singleton_ThreadPool()`), and the constructor hands the work to `ThreadPool_submit`. The
fetch makes the pool on its first call, under its lock, and every later one is a load. `ThreadPool_submit` calls
`ThreadPool_start` first, which starts the workers only while `worker_count_` is 0: one fewer than the machine's
processors, each with `ThreadPool_start_thread`, so this program starts all of them for its one task, where
`naive.c` starts one thread and `expert.c` one. The pool is paid for once, by the first `Parallel`; a second one
finds it running.

The same program with `var even_sum = evens.total()` in place of the `Parallel` has none of it. Both written with
`--check --c-source --optimized` and counted with `grep -c` and `wc -c`, and their allocations counted by
`--debug-memory`:

| Program | Lines of C | Bytes of C | Lines naming `ThreadPool` | Allocations |
|---|---|---|---|---|
| this program | 4 144 | 228 958 | 343 | 31 |
| the same with no `Parallel` | 1 898 | 95 203 | 1 | 6 |

The one line left in the second is a `SPITE_ALLOCATOR_List_ThreadPoolJob` macro that nothing uses.

## Timings

<!-- timings -->
<!-- /timings -->
