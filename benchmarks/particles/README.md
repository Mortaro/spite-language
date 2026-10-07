# Particles

A whole program, not one optimisation: 100 000 particles in a `Vector<Particle>`, stepped in place 300 times by a
member template, against an array of structs in C. It leans on
[a `Vector`'s items held inline](../../docs/collections.md#vectort-items-inline),
[a list's templates reading their elements uncounted](../../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them)
and [decimal literals beside a `Float` being `Float`s](../../docs/optimizations.md#a-decimal-literal-beside-a-float-is-a-float).

## The four forms

- [`naive/`](naive/): a `Particle` of six `Float`s with a `step()` that applies gravity, moves it and bounces it off
  the floor, 100 000 of them appended to a `Vector<Particle>`, 300 ticks of `particles.each_step()`, then the sums
  of their heights and spreads.
- [`naive.c`](naive.c): the same program as a C programmer writes it: one `malloc`ed array of particle structs,
  stepped in place by a plain loop calling `step`.
- [`expert.c`](expert.c): the same work tuned by hand: the particles as six columns of floats, each tick one loop
  the C compiler vectorises, the bounce a select instead of a branch.
- [`highlights.c`](highlights.c): the particle's struct, the program, the template `each_step` and `step`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`struct Particle` is the six floats behind a header, and `Vector__Particle_each_step` is a plain `while` over the
vector's items, each read in place with `InlineMemory__Particle_item_at` and handed to `Particle_step` uncounted:
the same loop as `naive.c`. The difference left is the header each item keeps in the vector (so more memory is read
per tick than C's bare struct) and the checked additions of the program around the loop.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 41 101 | 178 176 |
| naive C: `naive.c`, `clang -O2` | 55 189 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 32 853 | 140 288 |

Spite takes 0.74 times naive C's time and 1.25 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=41101 naive=55189 expert=32853 -->
<!-- /timings -->
