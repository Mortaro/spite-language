# A dictionary keyed by numbers hashes the numbers

A `Dictionary` the program gives whole-number keys is compiled as its own form of the library's dictionary: its
keys are a `List` of the numbers, a key is hashed by one multiply, and a slot's key is compared directly, with no
`String` made for any key. Which dictionaries are keyed by numbers is worked out while compiling, from the keys the
program's own code gives them.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers).
- The proof: [A dictionary's key kind](../../docs/proofs.md#a-dictionarys-key-kind-is-decided-while-compiling).

## The four forms

- [`naive/`](naive/): a `Dictionary<Integer>` of 100 000 entries keyed by spread-out numbers (`index * 7919 %
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
6364136223846793005, with no loop. `Dictionary_Integer_by_Integer_find_slot` compares `spite_temp.value == key_`,
a number with a number, and reads no hash bits from the slot (it still works out `fragment_`, which the C
compiler drops as unread, and `place` still writes those bits into each slot, where nothing reads them: a slot is
8 bytes for number keys too). In `Naive_look_up___held_0` the key goes in as an
`int32_t` and nothing is made for it: `--debug-memory` counts 20 allocations for the whole run, the table and the
two lists growing among them, where a key made with `index.to_string()` would be a text per lookup and a hash over
its characters.

Spite is slower than both C programs here: each probe reads the slot through `SpiteMemory_Address_read_integer`
at `slot * 8` with an overflow check, reads the key back out of `entry_keys_` with a range check, and steps with
`next_slot`, where `naive.c` follows a pointer to an entry that holds its key and `expert.c` reads the key beside
the slot. The hash is the cheap part; what remains is the dictionary keeping its entries in insertion order in two
lists, apart from its slots.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 38 957 | 186 368 |
| naive C: `naive.c`, `clang -O2` | 18 173 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 17 174 | 139 264 |

Spite takes 2.14 times naive C's time and 2.27 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=38957 naive=18173 expert=17174 -->
<!-- /timings -->
