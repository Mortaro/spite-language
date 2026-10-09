# A dictionary keyed by numbers hashes the numbers

A `Dictionary` keyed by a whole-number type is compiled as its own form of the library's dictionary: its keys are a
`List` of the numbers, a key is hashed by one multiply, and a slot's key is compared directly, with no `String`
made for any key.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers).

## The four forms

- [`naive/`](naive/): a `Dictionary<Integer, Integer>` of 100 000 entries keyed by spread-out numbers (`index * 7919 %
  1000003`), and 40 rounds of 100 000 lookups, a few of each round for keys that are not there.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a chained hash table, the key
  modulo the bucket count for a bucket, a `malloc`ed entry per key.
- [`expert.c`](expert.c): the same work tuned by hand: one open-addressed table made once at its size, key and
  value side by side in each slot, a multiplicative hash, linear probing.
- [`highlights.c`](highlights.c): the dictionary's struct, the loop of lookups, and its `find_slot`, `hash_of` and
  `home_slot` for number keys.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The dictionary is `Dictionary_Integer_by_Integer`, a class of its own beside the text-keyed `Dictionary_Integer`
(which this program does not use, so it is not in the C): its `entry_keys_` is a `List_Integer*`.
`Dictionary_Integer_by_Integer_hash_of` takes the key's bits as unsigned and multiplies once by
6364136223846793005, with no loop. `Dictionary_Integer_by_Integer_place` writes the key itself into the second
half of its slot, where a text key's slot holds 31 bits of its hash, and `Dictionary_Integer_by_Integer_find_slot`
compares that half with `key_`: a probe reads its slot and nothing else, and never reads `entry_keys_`. In
`Naive_look_up___held_0` the key goes in as an `int32_t` and nothing is made for it: `--debug-memory` counts 20
allocations for the whole run, the table and the two lists growing among them, where a key made with
`index.to_string()` would be a text per lookup and a hash over its characters.

Before the key was kept in its slot, each probe read the key back out of `entry_keys_` with a range check, and the
case took 2.14 times naive C's time. What still costs: each probe reads the slot through
`SpiteMemory_Address_read_integer` at `slot * 8` with an overflow check and steps with `next_slot`, and a hit
reads the value out of `entry_values_` with a range check, where `expert.c` keeps key and value side by side.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 26 296 | 207 872 |
| naive C: `naive.c`, `clang -O2` | 14 670 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 12 727 | 139 264 |

Spite takes 1.79 times naive C's time and 2.07 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=26296 naive=14670 expert=12727 -->
<!-- /timings -->
