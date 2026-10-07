# Reading an address is one machine operation

`source.read_byte(index)` and `address.write_byte(at, digit)` in `library/base64.spite` read as calls on a
`Memory.Address`, and only `library/` may write them. The compiler writes each one where it is called as the single
load or store it is, with no call and no check, so a library class such as `Base64` reads its bytes the way C does.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#reading-an-address-is-one-machine-operation).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the reads and writes are primitives of
  the language, like `+`, so there is nothing to prove before writing them as one instruction.

## The four forms

- [`naive/`](naive/): a `List<Byte>` of 100 000 bytes, and 100 rounds that each change one byte and encode the
  whole list with `base64.encode`, adding up the text's length and one of its characters. The reads are the
  library's: the program itself never touches an address.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of bytes, and
  the encoder written as the library writes it, with a function for each byte read, each byte written and each
  digit, and the text a new allocation per call.
- [`expert.c`](expert.c): the same work tuned by hand: one output buffer kept across the rounds, and every whole
  group of three bytes encoded as four table lookups with no test.
- [`generated.c`](generated.c): the two macros the reads and writes become, and what the compiler writes for
  `Base64._encoded` and `Base64._write_digit`.

## What to look at in generated.c

`SpiteMemory_Address_read_byte(self, offset)` is a `memcpy` of one byte from `self + offset`, and
`SpiteMemory_Address_write_byte` one byte to it: the C compiler makes each a single `mov`. In `Base64__encoded` the
three reads of a group are `SpiteMemory_Address_read_byte(source_, ...)` straight on the list's own buffer
(`source_ = (bytes_)->items_`), and in `Base64__write_digit` the digit is one `SpiteMemory_Address_write_byte`.
`naive.c` calls `read_byte` and `write_byte`, which clang inlines to the same single instructions, so on the reads
themselves the two are equal; `expert.c` reads the same bytes with no function in between.

What still makes `naive/` slower than `naive.c` is not the reads: every `+` on `index` and `written` is checked
for overflow, `_write_digit` takes and gives back a count on the alphabet text for every digit
(`SpiteString___retain(alphabet_)` at each call, `SpiteString___release(alphabet_)` inside), `code_at` clamps its
index, and `address.text(written)` copies the digits into a new `String` where `naive.c` keeps its buffer as the
text. Those are other sections' work (the counts on a parameter, arithmetic checked in every build); the reads
are already as cheap as they can be.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 22 760 | 183 296 |
| naive C: `naive.c`, `clang -O2` | 6 253 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 4 320 | 139 264 |

Spite takes 3.64 times naive C's time and 5.27 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=22760 naive=6253 expert=4320 -->
<!-- /timings -->
