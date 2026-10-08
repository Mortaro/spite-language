# Storing an object into a list counts it only when it changes the slot

`list[index] = name`, where the function holds `name` for the call, is written in place: the slot's pointer is
compared with the new one, and only when they differ is the new object counted, stored and the old one let go. Writing
back the object a slot already holds changes nothing, so it costs a load and a compare instead of four counts.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#storing-an-object-into-a-list-counts-it-only-when-it-changes-the-slot).
- The proof: none; whether the slot already holds the object is known only when the program runs. That the caller
  holds the object for the call is [a held argument](../../docs/proofs.md#an-argument-its-caller-holds-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): a `Rack` of sixteen slots, all holding its `light` crate; ten million times `place(at, crate)`
  reads the weight in the slot and stores a crate there, the `heavy` one once in 64 passes and `light` otherwise.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: an array of pointers, the same
  `place` reading the weight and storing the pointer; nothing counted.
- [`expert.c`](expert.c): the same work tuned by hand: only weights are ever read, so the slots are a local array of
  weights.
- [`highlights.c`](highlights.c): `place`, with the store written in place.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`crates[at] = crate` in `Rack_place___held_1` is one block: the index checked, the old pointer loaded, and
`Crate___retain`, the store and `Crate___release` of the old one only behind `if (old != new)`. The compiler before
this optimisation called `List_Crate_set_at(self->crates_, at_, Crate___retain(crate_))`, which released the old
crate, retained the new one again and released its own parameter: four counts per store, here all on the same
object. 39.6 ms for the loop against 9.8 after (one run each, the C compiled without link-time optimisation).

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 9 302 | 180 736 |
| naive C: `naive.c`, `clang -O2` | 6 926 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 4 578 | 139 264 |

Spite takes 1.34 times naive C's time and 2.03 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=9302 naive=6926 expert=4578 -->
<!-- /timings -->
