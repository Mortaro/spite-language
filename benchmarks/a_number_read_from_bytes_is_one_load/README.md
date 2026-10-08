# A number read from bytes is one load

A file format or a network message is numbers laid out in bytes, and in Spite those bytes are a `List<Byte>` that
reads the numbers by position: `records.read_integer_big_endian(position)`. Each read is a library function that
answers a `T?`; where a proof covers it, the compiler writes the read itself instead: one unaligned load from the
list's block, and one byte swap for a big-endian number.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-number-read-from-bytes-is-one-load).
- The proof: [A proven count or bound proves a read of a width](../../docs/proofs.md#a-proven-count-or-bound-proves-a-read-of-a-width),
  and the counted loop it runs in, [A counted loop reads its items unchecked](../../docs/proofs.md#a-counted-loop-reads-its-items-unchecked).

## The four forms

- [`naive/`](naive/): 100 000 records of 16 bytes, each three big-endian `Integer`s and two big-endian `Short`s,
  built with `append_integer_big_endian` and `append_short_big_endian`, then decoded 300 times by a loop that walks
  them, `while position + 15 < records.count()`, reads the five numbers at `position` to `position + 14` and steps
  `position = position + 16`. Nothing in it is narrowed by hand.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of bytes, each
  big-endian number put together from its bytes with shifts, the loop reading through the list's pointer and count.
- [`expert.c`](expert.c): the same work tuned by hand: a plain `restrict` block, each number one `memcpy` load and one
  `__builtin_bswap`, the loop over whole records.
- [`highlights.c`](highlights.c): what the compiler writes for the decode loop.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized` build
  for Windows writes it.

## What to look at in highlights.c

`Naive_decode___held_0` reads `records.count()` and the list's block once, before the loop, and runs while
`position_ < count - 15`: the window `position + 15` is the loop's bound, so the five reads inside it are proven at
both ends. Each is `SpiteMemory_Address_read_integer_big_endian(block, position_ + 4)` and its kin, the macro that is a
`memcpy` of four bytes and a `__builtin_bswap32`, with no compare, no call into `List_Byte_read_integer_big_endian` and
no `T?`. What the loop still pays for is the checked arithmetic of its sum and of `position + 16`, which C at the same
effort does not check.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 41 143 | 176 128 |
| naive C: `naive.c`, `clang -O2` | 54 481 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 33 020 | 139 264 |

Spite takes 0.76 times naive C's time and 1.25 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=41143 naive=54481 expert=33020 -->
<!-- /timings -->
