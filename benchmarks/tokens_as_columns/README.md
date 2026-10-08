# Tokens as columns

A data-oriented case (not an optimisation the compiler makes yet): a text of a million and a half pieces (words,
numbers and symbols, about 8 MB) is tokenised into a `List<Token>` of a kind, a start, a length, a line and a value,
and then walked: a count of each kind, the total of the numbers, and a checksum over every token. It is one of the
measurements behind [design/proposals/data_oriented_layout.md](../../design/proposals/data_oriented_layout.md):
a parser's list of tokens as columns of kinds and offsets instead of objects.

## The forms

- [`naive/`](naive/): the text built with `"{source}{piece} "`, a tokeniser that reads it with `code_at` and appends
  a `Token(kind, start, length, line, value)` per token, then `count_identifier()`, `count_number()`,
  `count_symbol()`, a `while` for the numbers' total and `sum_checksum()`. `--pieces=N` sets the text's size.
- [`naive.c`](naive.c): the same program as a C programmer writes it: the text in a growing buffer, a struct per
  token, one `malloc` each, an array of pointers, the same passes.
- [`expert_aos.c`](expert_aos.c): the tokens inline in one growing array of structs, the same passes.
- [`expert_soa.c`](expert_soa.c): the tokens as five columns, the same passes, each a vectorised loop over the
  columns it reads.
- [`expert.c`](expert.c): five columns with the kind narrowed to a byte (an enum of three values), and the passes
  fused into one vectorised walk.
- [`highlights.c`](highlights.c): `struct Token`, the tokeniser, one count and the checksum sum.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form writes and scans the text the same way, so the difference between them is only where the tokens are
kept and how the passes read them. Each prints the time of writing the text, tokenising it and the passes after its
`microseconds` line, and takes `--pieces=N` too.

## What to look at in highlights.c

`Naive_tokenized` makes a `Token` from the class pool for every token and appends its pointer; `struct Token` is a
header and five fields (32 bytes with the header) reached through that pointer. `List_Token_count_identifier` reads
the whole object to test its kind, so each of the three counts walks every token's line of memory to read one byte
of it. In columns the three counts read 1.5 MB of kinds (a byte each) instead of 48 MB of tokens.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 124 524 | 220 160 |
| naive C: `naive.c`, `clang -O2` | 149 026 | 145 920 |
| expert C: `expert.c`, `clang -O2` | 80 375 | 145 408 |

Spite takes 0.84 times naive C's time and 1.55 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-08, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=124524 naive=149026 expert=80375 -->
<!-- /timings -->

At other sizes, and with each phase apart: [cases.md](../../design/proposals/data_oriented_layout/cases.md#tokens_as_columns).
Most of every form's time is writing and scanning the text: the layout of the tokens decides the passes and the
appends, not the scan, so this case also shows where a data-oriented layout cannot help.
