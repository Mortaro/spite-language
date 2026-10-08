# A dictionary hashes a key once, cheaply

A dictionary keyed by text hashes the key with one exclusive or and one multiply per character (FNV-1a, on an
`UnsignedLong`), keeps some bits of that hash in the slot beside the entry's position, and compares the key texts
only when those bits match. A slower hash, or comparing the whole text at every slot probed, costs more per lookup.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-dictionary-hashes-a-key-once-cheaply).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); it is how `library/dictionary.spite` is
  written, and keys, values and their order are what they would be with any hash.

## The four forms

- [`naive/`](naive/): 5 000 names (`"entity_{index}"`), a `Dictionary<String, Integer>` from each name to a number, and 20
  rounds of 100 000 lookups of names spread over the list, written as Spite reads: `if by_name[name] { total =
  total + by_name[name] }`.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each name on the heap, and the
  hash table written from memory: `hash * 31 + character`, a bucket found with `%`, a chain of `malloc`ed entries
  each holding a copy of its key, `strcmp` on every entry of the chain, and the lookup made twice as the Spite
  writes it twice.
- [`expert.c`](expert.c): the same work tuned by hand: names in 16-byte slots, one open-addressed table of positions
  and hash bits, the keys and values in arrays beside it, FNV-1a, and one lookup per access.
- [`highlights.c`](highlights.c): the loop of lookups, and the dictionary's `find_slot`, `hash_of`, `home_slot` and
  `fragment_of` as the compiler writes them for text keys.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Dictionary_Integer_hash_of` is the loop of `SpiteUnsignedLong_bits_exclusive_or` and
`SpiteUnsignedLong_wrapping_multiply(mixed_, ... 1099511628211)`, one of each per character, the multiply a plain
C multiply since wrapping is what it asks for. `Dictionary_Integer_find_slot` hashes once, takes the home slot and
the fragment from that one hash, and at each slot it probes tests `stored_ > 0`, then the fragment read at `slot *
8 + 4`, and only then reads the key out of `entry_keys` and calls `SpiteString_equals`: a probe past an entry of
another key costs one compare of two numbers. The keys are short text, so each one is read where it lies in the
list, with no pointer to follow ([short text](../short_text_lives_inside_the_string/)).

Two things to know, both findings of this case. In `Naive_look_up___held_0_1`, `if by_name[name] { total = total +
by_name[name] }`, the way the language teaches narrowing a read, calls `Dictionary_Integer_get_at` twice and so
hashes the key twice: the path is narrowed, but the second read is not taken from the first. `naive.c` does the
same two lookups to match; `expert.c` does one, which is part of why it is three times faster (the rest is no
reference counting of the key, no overflow checks in the probing, and the table made once at its size). And the
fragment is `hash.shifted_right(33)`, 31 bits of the hash, where the optimisation's section says 32.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 69 590 | 195 072 |
| naive C: `naive.c`, `clang -O2` | 140 962 | 143 872 |
| expert C: `expert.c`, `clang -O2` | 41 508 | 143 872 |

Spite takes 0.49 times naive C's time and 1.68 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=69590 naive=140962 expert=41508 -->
<!-- /timings -->
