# Short symbols are inline text

A symbol whose text is 15 bytes or fewer, a symbol literal or a reflection name such as an attribute's, is
written into the program as an inline `String`: its bytes inside the 16-byte value, the form a short text built
at run time takes. Reading its bytes follows no pointer and the executable holds no separate copy of the text;
a longer symbol stays a pointer to constant text, and neither form is ever counted.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#short-symbols-are-inline-text).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); a symbol's text is known while
  compiling, so which form it takes is decided by its length alone.

## The four forms

- [`naive/`](naive/): three hundred monsters, and for each one a walk over its attributes that adds up the
  lengths of their names (`health` and `experience_points_gained`), then the sum of their power.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per monster, the
  attribute names as pointers to constant text, each measured with `strlen` for every monster.
- [`expert.c`](expert.c): the short name's bytes and length held together in 16 bytes, the long one as a pointer
  and a length, so adding up the lengths reads no text.
- [`generated.c`](generated.c): the program's symbol table and the two functions the walk was unrolled into, one
  per attribute.

## What to look at in generated.c

`spite_symbol_1` is `'health'`: `0x000068746c616568` is the six bytes `h e a l t h` read as one number, and the
top byte of the second word, `0x09`, says it is inline and 15 minus 9, six, bytes long. `spite_symbol_3` is
`'identity'`, the symbol the standard library passes when it opens the C library for the console, inline too.
`spite_symbol_2`, `'experience_points_gained'`, is 24 bytes, so it is `SPITE_STATIC_STRING`, a pointer to the
text and its length. The two `add_name_length` functions read their names the same way whatever the form, and
`SpiteString___retain` and `SpiteString___release` do nothing on either. `naive.c` keeps both names as pointers
and measures them with `strlen` each time; `expert.c` keeps the short one inline as the compiler does.

Each function still calls `SpiteString_length` on a name known while compiling: the length of a reflection name
is not folded to its number, which [Maths on constants is worked out while
compiling](../../../docs/optimizations.md#maths-on-constants-is-worked-out-while-compiling) could do.

## Why there is no time

The form of a symbol changes where its bytes live, not how much work the program does: the length is read from
the value in both forms, and this program never reads a name's bytes. What to compare is where the text is.
Counted with `grep` in the C that `--check --c-source --optimized` writes from `naive/`, it holds three symbols:
two inline (`health`, 6 bytes, and `identity`, 8 bytes), for which no string constant is written, and
one constant (`experience_points_gained`, 24 bytes), whose text is a string constant the value points at. An
inline symbol saves its text's bytes and the pointer's read; at this size that is 14 bytes, too few to see in the
executable's size, which Windows rounds to 512 bytes.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 172 032 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 240 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 264 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
