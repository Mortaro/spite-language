# A function taking a `type` is compiled per class

`doubled(value: Measured)` takes anything with a `to_double(): Double`, so it reads as one function that must find
out at run time which class it was handed. The compiler writes a copy of it for each class that reaches it
(`Integer`, `Float`, `Crate`, `Ball`), and each call, whose argument's class is known while compiling, calls its
class's copy, so a number is passed as itself and `value.to_double()` is a direct call.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-function-taking-a-type-is-compiled-per-class).
- The proof: none of its own. It stands on the compiler knowing each argument's class while compiling, the same
  knowledge [Conditions decided while compiling](../../docs/proofs.md#conditions-decided-while-compiling) folds
  a class test with.

## The four forms

- [`naive/`](naive/): a `type` of the program's own, `Measured`, that `Integer`, `Float` and two classes of its own
  (`Crate`, `Ball`) fit, and 20 000 000 steps that each pass a whole number, a `Float`, a crate and a ball to
  `doubled` and add up the answers. Every value is a whole number, so the total is exact.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `doubled` takes a tagged value
  (the class it holds, and the number or a pointer to the object), and a `switch` on the tag finds that class's
  `to_double`; a struct and a `malloc` per object.
- [`expert.c`](expert.c): the same work tuned by hand: no tag and no `switch`, each class's measure written straight
  into the loop, the objects' attributes in locals.
- [`highlights.c`](highlights.c): what the compiler writes for the loop and the four copies of `doubled`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_measure_all` calls `Naive_doubled___for_0_Integer(self, count_)` with an `int32_t`,
`Naive_doubled___for_0_Float` with a `float`, and `Naive_doubled___for_0_Crate___held_0` and
`Naive_doubled___for_0_Ball___held_0` with the object's pointer: no box, no tag. Inside, the number copies read
`SpiteInteger_to_double(value_)` and `SpiteFloat_to_double(value_)` (each a C cast), and the object copies call
`Crate_to_double(value_)` and `Ball_to_double(value_)` directly. The function as written, with its `switch` over
the classes, is reached by no call and is shaken out. `naive.c` builds a tagged value per call and switches on it;
`expert.c` has neither the call nor the switch.

The crate and the ball are made on the heap (`Crate___make`, `Ball___make`) and released at the end, though only
their attributes are written and each is only passed to its copy of `doubled`, which keeps nothing: they are not
[frame objects](../objects_that_never_leave_their_function_live_in_the_frame/) here. That is one allocation each for
the whole run, so it costs nothing measurable.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 50 755 | 194 560 |
| naive C: `naive.c`, `clang -O2` | 51 280 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 50 791 | 139 264 |

Spite takes 0.99 times naive C's time and 1.00 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=50755 naive=51280 expert=50791 -->
<!-- /timings -->
