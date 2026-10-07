# A release build is `-O3` with link-time optimisation

`--optimized` asks the C compiler for `-O3`, and a program split into several translation units is linked with
link-time optimisation, so functions are still inlined across units. The default build is `-O0`, quick to build
and slow to run, and every timing of these cases is of the release build.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-release-build-is--o3-with-link-time-optimisation).
- The proof: none; it is how the C is compiled, not a change to it.

## The four forms

- [`naive/`](naive/): a mixer object and 50 000 000 calls to its `mixed(value)`, adding up the answers.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct made with `malloc`, and a function
  call per value.
- [`expert.c`](expert.c): the factor in a register, unsigned values with a mask for the remainder by 65536, and a
  loop the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `checksum` and `Mixer.mixed`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

Nothing here is the optimisation: the C is the same in every build. `checksum` calls `Mixer_mixed(mixer_, index_)`
once per value, with every `+` and `*` checked for overflow, and it is the C compiler at `-O3` that copies
`Mixer_mixed` into the loop and keeps the factor in a register. Built by hand on the machine of the table below,
the same `naive/` ran its work in 220 759 µs in the default build (`-O0`) and in 31 479 µs with `--optimized`, one
run each, so the release build is seven times faster with no change to the source. What is left against
`expert.c` is the overflow checks, which every build keeps ([arithmetic is checked in every
build](../../docs/optimizations.md#arithmetic-is-checked-in-every-build)), and the signed remainders the checks
leave in place.

## Each optimisation level

The C the compiler writes (`--c-source`) for the whole programs [vector_maths](../vector_maths/),
[particles](../particles/), [number_dictionary](../number_dictionary/), [text_building](../text_building/) and
[sorting](../sorting/), built by hand at each optimisation level, best of five, microseconds, measured when `-O2` was
the release build:

| program | `-O0` (default build) | `-O2` | `-O3` (`--optimized`) | `-O3 -march=native` |
|---|---|---|---|---|
| `vector_maths` | 1 081 065 | 312 885 | 323 404 | 298 152 |
| `particles` | 199 393 | 60 972 | 61 007 | 51 148 |
| `number_dictionary` | 725 624 | 109 304 | 90 113 | 92 474 |
| `text_building` | 496 976 | 178 327 | 155 558 | 160 818 |
| `sorting` | 496 780 | 164 525 | 167 291 | 165 973 |

The compiler compiling itself (`spite bootstrap --check`), best of five, in CPU milliseconds (the process's own
time, since starting a process took up to two seconds on the loaded machine):

| the compiler built | CPU ms |
|---|---|
| one file, `-O0` (the default build) | 5 875 |
| one file, `-O1` | 1 938 |
| one file, `-O2` | 1 656 |
| one file, `-O3` | 1 766 |
| eight translation units, `-O3 -flto=thin` (`--optimized`) | 1 766 |

The default build is three to seven times slower than `-O2` on every program here; `-O3` against `-O2` is a wash
(the dictionary and text 10-18% faster, the compiler 6% slower, the rest equal); and the split `--optimized` build
runs exactly as fast as one file at `-O3`, so ThinLTO gets back the inlining across units. The programs have changed
since, and so has the compiler (`vector_maths` was measured before objects that never leave their function went
into the frame), so read the table for the ratios between levels, not for today's times.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 31 988 | 172 544 |
| naive C: `naive.c`, `clang -O2` | 44 651 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 44 501 | 139 264 |

Spite takes 0.72 times naive C's time and 0.72 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=31988 naive=44651 expert=44501 -->
<!-- /timings -->
