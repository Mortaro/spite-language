# An item passed to a call that cannot change its list is not counted

A list item used at once, without a name, is read from its slot without counting it: passed to a call that cannot
change the list, read for one of its attributes, or asked one of a list's reading functions. The list holds the
item the whole time, so the count up and down around the use bought nothing.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#an-item-passed-to-a-call-that-cannot-change-its-list-is-not-counted).
- The proof: [docs/proofs.md](../../docs/proofs.md#an-item-used-at-once-is-not-counted).

## The four forms

- [`naive/`](naive/): a `Playlist` holds sixteen tracks; ten million times `listen` hands `tracks[at]` to
  `length_of` and reads `tracks[at].seconds`, adding a remainder of each.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: an array of pointers to tracks
  allocated one by one, a bounds-checked `track_at`, the same small function; nothing counted.
- [`expert.c`](expert.c): the same work tuned by hand: what a pass adds depends only on its track, so the sixteen
  sums are worked out once and the loop adds table entries.
- [`highlights.c`](highlights.c): `listen` and the held copy of `length_of` it calls.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Both uses of `tracks[at]` in `Playlist_listen` read `((Track**)(intptr_t)(list)->items_)[at]` after the index is
checked, with no count: one is passed to `Playlist_length_of___held_0`, which `length_of`'s call effects allow (it
writes into no list), and one has its `seconds_` read. The compiler before this optimisation read both with
`List_Track_get_at`, counted the track, and let it go after each use: 23.9 ms for the loop against 20.7 after (one
run each, the C compiled without link-time optimisation). The rest of the time is the two remainders and the
checked additions, which `naive.c` makes too; `expert.c` works them out once per track.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 22 970 | 179 712 |
| naive C: `naive.c`, `clang -O2` | 21 764 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 562 | 139 264 |

Spite takes 1.06 times naive C's time and 8.97 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking at the same time.
<!-- measured spite=22970 naive=21764 expert=2562 -->
<!-- /timings -->
