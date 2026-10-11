# Appending to text in place

`text = text + piece` means a new text holding both, which copied on every append would make the commonest loop
there is quadratic. When nothing but that variable holds the text, the compiler grows its buffer in place instead,
doubling its room, and copies only when something else holds it too.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#appending-to-text-in-place).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); whether another name holds the text is
  asked while the program runs, of the text's own count (`block->header.ref_count == 1`), so the program means
  the same either way.

## The four forms

- [`naive/`](naive/): eight texts, each made of 10 000 appends of `"word "`, and the total of their lengths.
- [`naive.c`](naive.c): the same program written as the Spite reads, with text that never changes: each append
  makes a new text of the old one and the piece, and frees the old one.
- [`expert.c`](expert.c): one buffer per text, grown by doubling, each piece copied once onto its end.
- [`highlights.c`](highlights.c): what the compiler writes for `words` and the library's `SpiteString_append`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`words` calls `SpiteString_append(text_, ...)` and assigns the answer back to `text_`. In `SpiteString_append`, a
text of up to 15 bytes grows inside its own sixteen bytes; past that, a block whose count is 1 is grown with
`SPITE_REALLOC` to twice its capacity and the piece copied onto its end, so each byte is copied a constant number of
times on average. Only a block something else holds is copied, once, with room to grow. `naive.c` copies the whole
text on every append, so its time grows with the square of the text's length; `expert.c` is the same doubling buffer
the compiler arrives at, without the checks for the inline and shared forms.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 194 | 196 608 |
| naive C: `naive.c`, `clang -O2` | 87 742 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 106 | 139 264 |

Spite takes 0.00 times naive C's time and 1.83 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-10, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=194 naive=87742 expert=106 -->
<!-- /timings -->
