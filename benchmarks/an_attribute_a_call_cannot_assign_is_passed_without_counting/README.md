# An attribute a call cannot assign is passed without counting

Passing an attribute, or a path of attributes, to a function counted it once more for the callee and let that count
go when the callee returned. When nothing the call can run assigns any attribute along the path, the object holding
it holds the argument for the whole call, so the call goes to a copy that takes it as held, and nothing is counted.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-attribute-a-call-cannot-assign-is-passed-without-counting).
- The proof: [docs/proofs.md](../../docs/proofs.md#an-attribute-a-call-cannot-assign-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): a `Meter` holds a list of sixteen readings and its `Settings`, which hold its `Limits`; ten
  million times `measure(index)` hands `readings` to `reading_at` and `settings.limits` to `offset_of`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: structs and pointers, the same
  three small functions, nothing counted, since C has no counts.
- [`expert.c`](expert.c): the same work tuned by hand: the offset and the readings loaded once into locals, the
  remainder a mask, and the loop left to the C compiler to vectorise.
- [`highlights.c`](highlights.c): `measure` and the two held copies it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Meter_measure` calls `Meter_reading_at___held_0(self, self->readings_, ...)` and
`Meter_offset_of___held_0(self, (self->settings_)->limits_)`: the list and the limits read in place, where the
compiler before this optimisation wrote `List_Integer___retain(self->readings_)` and
`Limits___retain((self->settings_)->limits_)` and each callee released its parameter at its end. Neither callee
assigns `Meter.readings`, `Meter.settings` or `Settings.limits`, which is what lets the call take them held. The
program ran its ten million passes in 28.4 ms before and 5.4 ms after (one run each, the C compiled without
link-time optimisation).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 4 727 | 178 688 |
| naive C: `naive.c`, `clang -O2` | 4 673 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 791 | 139 264 |

Spite takes 1.01 times naive C's time and 1.69 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=4727 naive=4673 expert=2791 -->
<!-- /timings -->
