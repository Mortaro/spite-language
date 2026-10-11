# Singletons: made on first use, never counted

A singleton is made the first time something asks for it, and fetching it after that is one load from a static
slot. It is never reference counted, so an object that binds it, and two threads that fetch it at once, raise and
lower no count on it.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#singletons-made-on-first-use-never-counted).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); a singleton is never freed before the
  program's teardown, so a count on it could never reach zero and is left out for every singleton, with nothing to
  prove.

## The four forms

- [`naive/`](naive/): two `Parallel` fetchers, each making 10 million `Visit` objects that bind the `Rules`
  singleton (`var rules = Rules()`) and add up `number % 7 + rules.base`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `rules_get()` makes the one
  `Rules` under a mutex the first time and counts it up on every fetch with an atomic addition, and every `Visit`,
  made with `malloc`, counts it down with an atomic subtraction when it is freed, so the two threads hand the mutex's and
  the count's cache lines between them on every visit.
- [`expert.c`](expert.c): the same work tuned by hand: `Rules` is a static object set before the threads start, and
  each visit is two numbers in registers.
- [`highlights.c`](highlights.c): the fetch, the line of `Visit`'s defaults that binds the singleton, the fetchers'
  loop and `weight`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`spite_singleton_Rules` answers `spite_singleton_Rules_cache` at its first line once the singleton is made
(`SPITE_SINGLETON_FOUND`, one acquiring load in a program with threads); only the first fetch takes
`spite_singleton_Rules_lock` and makes it. `Visit___init` stores what it answers in `self->rules_` with no
`Rules___retain`, and nothing lets it go: `Rules` has no retain and no release at all. `Visit_weight` reads
`(self->rules_)->base_` plainly, since `Rules` never changes after its constructor and so takes no lock. The `Visit`
itself is made in the frame (`Visit___make_into(&spite_slot_1, index_)`,
[a separate optimisation](../../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)),
so part of the gap with `naive.c`, which `malloc`s and frees every visit, is that allocation and not the count.
`naive.c` takes the mutex and makes two locked additions on one shared line per visit; `expert.c` takes nothing.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 15 947 | 235 520 |
| naive C: `naive.c`, `clang -O2` | 212 008 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 5 547 | 139 776 |

Spite takes 0.08 times naive C's time and 2.87 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=15947 naive=212008 expert=5547 -->
<!-- /timings -->
