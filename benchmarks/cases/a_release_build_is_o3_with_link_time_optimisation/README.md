# A release build is `-O3` with link-time optimisation

`--optimized` asks the C compiler for `-O3`, and a program split into several translation units is linked with
link-time optimisation, so functions are still inlined across units. The default build is `-O0`, quick to build
and slow to run, and every timing of these cases is of the release build.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-release-build-is--o3-with-link-time-optimisation).
- The proof: none; it is how the C is compiled, not a change to it.

## The four forms

- [`naive/`](naive/): a mixer object and 50 000 000 calls to its `mixed(value)`, adding up the answers.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct made with `malloc`, and a function
  call per value.
- [`expert.c`](expert.c): the factor in a register, unsigned values with a mask for the remainder by 65536, and a
  loop the C compiler vectorises.
- [`generated.c`](generated.c): what the compiler writes for `checksum` and `Mixer.mixed`.

## What to look at in generated.c

Nothing here is the optimisation: the C is the same in every build. `checksum` calls `Mixer_mixed(mixer_, index_)`
once per value, with every `+` and `*` checked for overflow, and it is the C compiler at `-O3` that copies
`Mixer_mixed` into the loop and keeps the factor in a register. Built by hand on the machine of the table below,
the same `naive/` ran its work in 220 759 µs in the default build (`-O0`) and in 31 479 µs with `--optimized`, one
run each, so the release build is seven times faster with no change to the source. What is left against
`expert.c` is the overflow checks, which every build keeps ([arithmetic is checked in every
build](../../../docs/optimizations.md#arithmetic-is-checked-in-every-build)), and the signed remainders the checks
leave in place.

## Timings

<!-- timings -->
<!-- /timings -->
