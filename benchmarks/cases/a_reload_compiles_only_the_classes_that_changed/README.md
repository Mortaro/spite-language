# A reload compiles only the classes that changed

When a file of a running `--hot-reload` program is saved, the reload compiles the functions of the classes that
file declares and nothing else of the program: every class is still read and checked, but every function the
running program already has with the same prototype is reached there, through the facts the build wrote into its
manifest. A change that alters one of those facts (a class's attributes, a parameter list) compiles the whole
program instead, and says why.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-reload-compiles-only-the-classes-that-changed).
- The proof: [A reload compiles only the changed classes](../../../docs/proofs.md#a-reload-compiles-only-the-changed-classes).

## The four forms

- [`naive/`](naive/): a hundred monsters and a hundred heroes, and an `Arena` where every hero strikes its monster
  each round until none is alive, in four classes of four files.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per object, a growable array of pointers per list. A change to it is a rebuild of the whole program.
- [`expert.c`](expert.c): the health and the damage as two arrays on the stack, each round one pass that strikes
  and notes whether any monster is alive.
- [`generated.c`](generated.c): `Hero.damage`, the function the change below edits, and `Arena.strike_all`, which
  calls it and is not compiled again by the reload.

## What to look at in generated.c

`Arena_strike_all___held_0_1` calls `Hero_damage(hero_)` by name. In the `--hot-reload` build that name is a
forwarder to a slot, so when a reload changes `damage` (here `strength + 1` became `strength + 2` in `hero.spite`)
the new `Hero_damage___hot` is installed in the slot and `strike_all`, `Monster`, `Naive` and the library are
left as the running program has them. The reload's C is only `Hero`'s functions (its allocation, release, copy,
constructor and `damage`), the helpers they call and `spite_reload_bind`, which installs them. `naive.c` and
`expert.c` have no reload: their change is a whole rebuild.

## Why there is no time

The reload changes how long a save takes to reach a running program, not how fast the program runs, so the three
programs run alike and there is nothing to time in them. What to compare is the reload against the whole build.
These were measured with a copy of `naive/` in a scratch folder: built with `--hot-reload --build
--executable-path=...` (never run), then `hero.spite` changed and `spite reload <folder> --hot-reload
--executable-path=...` run the way the running program runs it, timed by the shell around each command (one run
each, on Windows with clang, on a machine other sessions were building on):

| Step | Wall time | C written |
|---|---|---|
| the whole `--hot-reload` build | 5.2 s | 60 834 lines, 4 737 343 bytes |
| a reload of the unchanged files | 0.3 s | none: it answers `unchanged` |
| a reload after `damage` changed | 1.7 s | 461 lines, 24 795 bytes, 16 functions: it answers `rebuilt Hero` |
| a reload after `Hero` gained an attribute | 4.8 s | 667 lines, 37 299 bytes, 29 functions, after compiling the whole program |

The last row is the fallback: a new attribute changes the layout other classes were compiled against, so the
compiler says `compiling the whole program, since the attributes of Hero changed, and their live objects move to
the new layout`, compiles everything, and still writes only the functions whose C changed. With
`SPITE_RELOAD_CHECK` set to `hero.spite`, the body change was compiled both ways and the compiler answered `the
fast reload matches a whole compile, rebuilt Hero`. At this size the 1.7 s is still reading and checking every class
(the library's included) and one run of the C compiler; on a large game's server the same reload takes about 6 seconds where a whole compile takes 20 to
30.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 177 664 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 140 288 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
