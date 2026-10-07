# Proofs that survive a call

A proof such as `crash target` lets every read after it skip the null test, and a call between the proof and a
read keeps it unless the compiler, following the called function and everything it calls, finds that the call
may assign an attribute the proof reads through or shrink a list it reads. So no check is repeated after a call
that provably cannot change what was proven, and no `const` keyword is needed to say so.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#proofs-that-survive-a-call).
- The proof: [A call keeps a proof it cannot change](../../../docs/proofs.md#a-call-keeps-a-proof-it-cannot-change).

## The four forms

- [`naive/`](naive/): a `Hunter` whose `target` is a `Monster?`, given a thousand monsters in turn; each hunt
  proves the target once with `crash target`, then strikes it until it falls, calling `record_hit()` and
  `target.hurt(3)` in every pass.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: the pointer is checked once
  at the start of a hunt, because the programmer knows `record_hit` cannot change it.
- [`expert.c`](expert.c): each monster's health a local number, so there is no pointer to check.
- [`generated.c`](generated.c): `hunt` and the two functions it calls.

## What to look at in generated.c

`Hunter_hunt` tests `self->target_` once, before the loop, and the loop then reads `(self->target_)->health_` and
calls `Monster_hurt(self->target_, 3)` with no test, though `Hunter_record_hit(self)` runs between them on every
pass: `record_hit` assigns only `hits_`, and `Monster_hurt` only the monster's `health_`, so neither can make
`target` null. That is the C `naive.c` is written as, with its one check. When the called code may change what
was proven, the proof does not survive: with `record_hit` given the line `target = null` under an `if`, the same
`hunt` is refused,

```
hunter.spite:8: error: 'record_hit()' inside this loop may change 'target', which undoes what was proven about
it before the loop, and the next pass would read it unproven: prove it inside the loop instead (in Hunter.hunt)
```

and once the loop proves `target` after the call again (written as a `crash target` after `record_hit()` and a
function `still_alive()` that proves it before reading `target.health`), the C tests it twice in every pass.

## Why there is no time

The proof is made while compiling and emits nothing: what it saves is a test the programmer would otherwise have
to write and the program would run, here once per pass of a loop of 10 033 passes, a comparison a branch
predictor makes free. The three programs do the same work. What to compare is the tests in the loop, read from
the C written with `--check --c-source --optimized`:

| Program | Null tests per hunt |
|---|---|
| `naive/`, as written | 1, before the loop |
| `naive/` with `record_hit` able to clear `target`, proven again in the loop | 2 in every pass, and 1 before the loop |
| `naive.c` | 1, before the loop |
| `expert.c` | none |

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 170 496 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 240 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 776 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
