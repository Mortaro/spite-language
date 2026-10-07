# Boxing only where a value travels as a shape

A `type` that requires no attributes (`Anything`, `Printable`, a shape of functions only) holds its value in
sixteen bytes: the class's id as a tag, and either the object or, for a number, `Boolean` or enum value, the value
itself, so a number put in a `List<Printable>` is never boxed. Only text and a `Symbol` are put in a box where
they travel as a shape, since a `String`'s own sixteen bytes do not fit beside a tag.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#boxing-only-where-a-value-travels-as-a-shape).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the class of each value is known where
  it is put into the shape, and the tag carries it from there.

## The four forms

- [`naive/`](naive/): a `List<Printable>` of 90 000 values, a third each `Integer`, `Boolean` and text made while
  the program runs (`"item {index}"`), and 20 rounds that turn every value into text with `to_string()` and add up
  the lengths.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: "any printable value" as an
  object, a `malloc`ed box of the class and the value for every value, numbers included, a growable array of
  pointers, and a `to_string` that switches on the class and `malloc`s its text.
- [`expert.c`](expert.c): the same work tuned by hand: one array of tagged values with the text inline, and each
  round's texts written into a buffer on the stack.
- [`highlights.c`](highlights.c): the tagged value's macros, how an `Integer` and a text enter the shape, the
  dispatch of `to_string`, and the two loops.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

In `Naive_add_value___held_0`, `values.append(index)` is `List_Console_Printable_append(values_,
spite_tagged_SpiteInteger(index_))`: `spite_tagged_SpiteInteger` sets the tag to the class's id, `plain` to 1, and
copies the number into the value's bits, a struct returned by value with nothing allocated; a `Boolean` is the
same. The text goes in as `spite_tagged_object(0, spite_box_SpiteString(...))`, and `spite_box_SpiteString` is
one `SPITE_MALLOC`: the one box per text. `Console_Printable___call_to_string` tests the tag and calls the class's
own `to_string` with the plain value, `SPITE_TAGGED_VALUE(self, int32_t)`, or with the boxed text. So
`--debug-memory` counts 30 008 allocations for the run: the 30 000 text boxes and the list. `naive.c` makes 90 000
boxes, and a text per value per round besides, where Spite's `to_string` of a number or a `Boolean` is short text
held in the `String` itself ([short text](../short_text_lives_inside_the_string/)), so measuring allocates nothing.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 20 380 | 176 640 |
| naive C: `naive.c`, `clang -O2` | 150 885 | 142 848 |
| expert C: `expert.c`, `clang -O2` | 12 572 | 140 288 |

Spite takes 0.14 times naive C's time and 1.62 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=20380 naive=150885 expert=12572 -->
<!-- /timings -->
