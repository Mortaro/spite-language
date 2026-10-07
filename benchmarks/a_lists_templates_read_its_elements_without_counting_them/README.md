# A list's templates read its elements without counting them

A member template such as `bodies.each_advance()` or `bodies.sum_position()` would read each element with a count
up and let it go with a count down, two writes to every object it walks. When nothing the pass runs can let go of
anything, the element is read as it lies in the list, uncounted.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them).
- The proof: [List templates read uncounted](../../docs/proofs.md#a-lists-templates-read-their-elements-uncounted).

## The four forms

- [`naive/`](naive/): 200 000 bodies, each made beside a label of its own so the bodies lie spread through memory,
  and 100 rounds of `bodies.each_advance()` then `bodies.sum_position()`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per body and per label in the same order, a growable array of pointers per list, and each template a function
  that walks the list. C keeps no counts, so each element is the pointer from its slot.
- [`expert.c`](expert.c): the same work tuned by hand: positions, speeds and label numbers as columns, and each
  round's advance and sum in one loop the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `simulate` and the two templates.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`List_Body_each_advance` and `List_Body_sum_position` each read `Body* item_ = ((Body**)(intptr_t)self->items_)[index_]`
and use it: no `Body___retain` before the call to `Body_advance` or the read of `position_`, and no `Body___release`
after. `advance()` only assigns a number attribute, so nothing in the pass can let go of an object. Without the
optimisation the Spite would read each element with `values.read_value(items, index)` (a count up) and release it at
the end of its pass (a count down), two writes per body per template, on bodies spread through memory. `naive.c`
reads the pointer and nothing more, which is what the Spite now does; `expert.c` walks columns of numbers and fuses
the two passes of a round into one.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 29 290 | 182 784 |
| naive C: `naive.c`, `clang -O2` | 39 866 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 2 570 | 139 776 |

Spite takes 0.73 times naive C's time and 11.40 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=29290 naive=39866 expert=2570 -->
<!-- /timings -->
