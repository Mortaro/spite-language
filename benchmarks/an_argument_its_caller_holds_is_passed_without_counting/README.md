# An argument its caller holds is passed without counting

Passing an object to a function counts it once more for the callee's own name and lets that count go when the
callee returns: two atomic operations in a program with threads. When the argument is a name the caller holds for
the whole call (its own parameter, or a local it owns), the call goes to a copy of the function,
`<name>___held_<positions>`, that does not let those parameters go, and the caller passes them as they are.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-argument-its-caller-holds-is-passed-without-counting).
- The proof: [An argument its caller holds is passed uncounted](../../docs/proofs.md#an-argument-its-caller-holds-is-passed-uncounted).

## The four forms

- [`naive/`](naive/): one `Parallel` stream walks 3 million entities, and for each hands its list of rows to
  `matcher.match_into(entity, rows)`, which hands it on to `find_row(index, entity, found)` for each of three
  columns, which writes the entity's place into it.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a reference-counted program
  with threads, where each function handed the list counts it up with an atomic operation and down when it
  returns, four times up and four down per entity.
- [`expert.c`](expert.c): the same work tuned by hand: the headers and rows as arrays on the thread's stack, the
  match inline in the entity loop, and nothing counted.
- [`highlights.c`](highlights.c): the stream's run and the three functions the list is passed through.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Stream_run` calls `Stream_run_positions___held_0_1`, which calls `Matcher_match_into___held_1(matcher_, entity_,
rows_)`, which calls `Matcher_find_row___held_2(self, index_, entity_, found_)`: each passes its own parameter on
as it is, and none of the three has a `List_Integer___retain` or `___release` of it. `Stream_run` releases `found_`
once, after the whole walk. The ordinary versions of these functions are not in the C: nothing else calls them, so
they are shaken out. `naive.c` makes eight atomic operations on the list per entity; `expert.c` makes none and
reads the rows without a `get_at` or its check.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 11 975 | 234 496 |
| naive C: `naive.c`, `clang -O2` | 34 130 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 665 | 139 264 |

Spite takes 0.35 times naive C's time and 18.01 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=11975 naive=34130 expert=665 -->
<!-- /timings -->
