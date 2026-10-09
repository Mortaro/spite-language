# An image filter over planes

A data-oriented case (not an optimisation the compiler makes yet): an image of 4 194 304 pixels (2048 by 2048) as
a `List<Pixel>` of four `Integer` channels, brightened, given more contrast, then measured: a histogram of
luminance, a count of the bright opaque pixels and the sum of every pixel's weight. It is one of the measurements
behind [design/proposals/data_oriented_layout.md](../../design/proposals/data_oriented_layout.md), and the case
where the best layout is not obvious: interleaved channels (an array of structures of bytes) against one plane per
channel (a structure of arrays of bytes).

## The forms

- [`naive/`](naive/): a `Pixel` class of four `Integer`s with `brighten()`, `contrast()`, `luminance()`,
  `is_bright()` and `weight()`, held in a `List<Pixel>`; `pixels.each_brighten()`, `pixels.each_contrast()`, a
  `while` for the histogram, `count_is_bright()` and `sum_weight()`. `--pixels=N` sets the image's size.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct of four ints per pixel, one `malloc`
  each, an array of pointers, the same passes.
- [`expert_aos.c`](expert_aos.c): the layout an image library uses: red, green, blue and alpha interleaved, four
  bytes a pixel; each filter walks every byte and keeps alpha with a select, vectorised sixteen bytes at a time.
- [`expert_soa.c`](expert_soa.c): four planes of bytes, the same passes, each filter one vectorised loop over one
  plane, the alpha plane never read by the filters.
- [`expert.c`](expert.c): four planes, the two filters fused into one function of a channel applied per plane, a
  vectorised pass for luminance and the bright count, one for the weights, and the histogram into four histograms.
- [`highlights.c`](highlights.c): `struct Pixel`, the brighten template and functions, the histogram and the weight
  sum.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form prints the time of making the image, filtering it and measuring it after its `microseconds` line, and
takes `--pixels=N` too.

## What to look at in highlights.c

`struct Pixel` keeps each channel in 32 bits behind the 8-byte header: 24 bytes and a pointer for what fits in 4
bytes. `Pixel_brightened` checks its `+` for overflow and `Pixel_contrasted` its `-`, `*` and `+`, though every
channel is proven to stay in `0..255` by the `minimum` and `clamp` that end each function: the range proof that
would narrow the field to a byte would also drop every check and let the loop vectorise.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 115 503 | 242 176 |
| naive C: `naive.c`, `clang -O2` | 232 171 | 141 824 |
| expert C: `expert.c`, `clang -O2` | 20 762 | 143 872 |

Spite takes 0.50 times naive C's time and 5.56 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=115503 naive=232171 expert=20762 -->
<!-- /timings -->

At other sizes, and with each phase apart: [cases.md](../../design/proposals/data_oriented_layout/cases.md#image_filter_over_planes).
Fusing every step into one loop measured about four times slower than the separate passes of `expert_soa.c` (the fused
loop is no longer vectorised): fusion and layout are separate decisions, and a fusion that loses the vector loop
loses.
