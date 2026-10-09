# Reflection on constants folds and unrolls

`stats.attributes.each(add_attribute)` walks the attributes of a class the compiler knows, so the list is a constant:
the walk is written as one call per attribute, each to a copy of `add_attribute` made for that attribute, where
`attribute.class == Integer` is answered and `attribute.value` is a plain read of the field. No list, no
`Spite.Attribute` object and no boxed value is made.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#reflection-on-constants-folds-and-unrolls).
- The proof: none of its own; the class test in each copy is answered by
  [Conditions decided while compiling](../../docs/proofs.md#conditions-decided-while-compiling).

## The four forms

- [`naive/`](naive/): 200 000 `Stats` of four whole numbers, and 40 rounds adding up every attribute of every one
  through an attribute walk.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a table describing `Stats`
  (each attribute's name, class and offset), `attributes_of` making a list of attribute objects from it on every
  walk, `add_attribute` testing each one's class and reading its value through a pointer, and the list freed.
- [`expert.c`](expert.c): no walk at all: the party as four columns, the four values added where they are read,
  each round one loop the C compiler vectorises.
- [`highlights.c`](highlights.c): what the compiler writes for `total_of` and two of its four copies of
  `add_attribute`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_total_of___held_0` is the walk unrolled: four calls in a row, `Naive_add_attribute_for_strength___held_0` to
`..._for_stamina___held_0`, each handed the `Stats` itself. In each copy the `if attribute.class == Integer` is
gone (it was true while compiling) and `attribute.value` is `(attribute_instance_)->strength_`. Under
`--debug-memory` the whole run makes 200 008 allocations, the `Stats` and the party's list, and none for the 8 000 000
walks. `naive.c` asks for six blocks per walk (the list, its items and four attribute objects); `clang -O2` sees that
none of them leaves `total_of` and removes them all, so `naive.c` ran in 14 ms against 11 for the Spite program, but
built with `-fno-builtin`, where clang may not assume what `malloc` and `free` do, it took 1.37 s (one run each). The
Spite program's C holds no walk whatever compiles it. `expert.c` has no walk to begin with.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 12 046 | 200 192 |
| naive C: `naive.c`, `clang -O2` | 11 837 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 538 | 139 776 |

Spite takes 1.02 times naive C's time and 4.75 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=12046 naive=11837 expert=2538 -->
<!-- /timings -->
