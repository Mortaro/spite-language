# Benchmarks

Small, ordinary Spite programs that each lean on one thing the compiler emits, so a change to the generated C can
be measured instead of guessed. `check.sh` only compiles them; timing them is done by hand:

```
bash benchmarks/run.sh [compiler] [benchmark ...]
```

`run.sh` has the compiler write each program's C, builds it with `clang -O2`, prints the best of seven runs, and
builds the same C once more with `--debug-memory`'s table to print how many allocations it makes. The compiler
defaults to `.spite-cache/spite_development.exe`, the one `check.sh` last built. Times are wall-clock milliseconds
on Mortaro's Windows machine and move by 10-20% from run to run; the allocation counts are exact.

| Benchmark | What it leans on |
|---|---|
| `fused_chain` | 100 000 objects walked 300 times by fused `filter_`/`map_`/`sum_`/`count_` chains |
| `dictionary_keys` | a `Dictionary` keyed by numbers turned into text (`"{index}"`) and by 2 000 names, set and read |
| `text_building` | appending to text in a loop, `"word{index}"` pieces and `join` |
| `reflection_walks` | a Symbol walk (`show_attributes`), a `.attributes` walk reading `.value`, and `Json` |
| `function_values` | `each(f)`, `filter(f)`, `sum(f)`, `count(f)` and a function value passed 200 000 times |
| `small_allocations` | three small objects made and dropped per pass, three million passes, and `copy()` |
| `parallel_calls` | 20 000 rounds of two `Parallel`s |
| `stress` | SlopEngine's `examples/stress` shape: component columns as generic singletons, `system/` classes with `update_each` over `type` rows filled by a Symbol walk, 50 000 entities, 20 ticks |

## Results

Each step is one commit; `before` is the compiler before it. Best of nine interleaved runs.

### Step 1: text joined in one piece, replaced defaults never made, function arguments described on demand

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| fused_chain | 279 | 289 | 300 009 | 200 009 |
| dictionary_keys | 446 | 416 | 2 208 012 | 1 108 012 |
| text_building | 329 | 326 | 5 500 265 | 5 500 225 |
| reflection_walks | 659 | 505 | 16 356 022 | 11 756 022 |
| function_values | 140 | 107 | 2 000 019 | 1 000 019 |
| small_allocations | 167 | 166 | 9 004 013 | 9 004 013 |
| parallel_calls | 197 | 179 | 1 040 077 | 680 074 |
| stress | 142 | 147 | 150 049 | 150 049 |

### Step 2: list templates borrow the elements they only read; the compiler finds classes by name in one lookup

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| fused_chain | 284 | 168 | 200 009 | 200 009 |
| dictionary_keys | 400 | 415 | 1 108 012 | 1 108 012 |
| text_building | 349 | 352 | 5 500 225 | 5 500 225 |
| reflection_walks | 547 | 587 | 11 756 022 | 11 756 022 |
| function_values | 110 | 110 | 1 000 019 | 1 000 019 |
| small_allocations | 184 | 172 | 9 004 013 | 9 004 013 |
| parallel_calls | 219 | 201 | 680 074 | 680 074 |
| stress | 190 | 200 | 150 049 | 150 049 |

Only `fused_chain` walks a list of objects through a template, and it is the one that moved; the rest is noise.
Compiling the compiler (`spite bootstrap --run=false --c-source`, the compiler built with `clang -O1`, best of
seven): 2 352 ms before this work, 2 198 ms after step 1, 1 953 ms after step 2 -- finding a class by its name
or its C name was a walk over every class, and is now one dictionary lookup.
