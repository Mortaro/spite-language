# Benchmarks

Small, ordinary Spite programs that each lean on one thing the compiler emits, so a change to the generated C can
be measured instead of guessed. `check.sh` only compiles them; timing them is done by hand:

```
bash benchmarks/run.sh [compiler] [benchmark ...]
```

`run.sh` has the compiler write each program's C, builds it with `clang -O2`, prints the best of seven runs, and
builds the program once more with `--debug-memory` to print how many allocations it makes. The compiler
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

### All four steps: the compiler before this work against the compiler after it

Each side compiled in its own tree, so each reads its own `library/`; best of nine interleaved runs.

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| fused_chain | 216 | 143 | 300 009 | 200 009 |
| dictionary_keys | 391 | 197 | 2 208 012 | 1 104 014 |
| text_building | 276 | 155 | 5 500 265 | 800 227 |
| reflection_walks | 602 | 260 | 16 356 022 | 7 596 024 |
| function_values | 119 | 67 | 2 000 019 | 400 019 |
| small_allocations | 146 | 82 | 9 004 013 | 9 004 013 |
| parallel_calls | 163 | 138 | 1 040 077 | 440 074 |
| stress | 106 | 106 | 150 049 | 150 049 |

Compiling the compiler (the compiler built with `clang -O1`, best of seven): 1 831 ms before, 1 318 ms after.

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

### Step 3: numbers written into text in place, freed small objects kept for reuse, a cheaper dictionary hash, and the replaced defaults of `Spite.Function` and `Spite.Attribute`

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| fused_chain | 139 | 149 | 200 009 | 200 009 |
| dictionary_keys | 350 | 191 | 1 108 012 | 1 104 014 |
| text_building | 270 | 156 | 5 500 225 | 800 227 |
| reflection_walks | 435 | 261 | 11 756 022 | 7 596 024 |
| function_values | 87 | 64 | 1 000 019 | 400 019 |
| small_allocations | 143 | 78 | 9 004 013 | 9 004 013 |
| parallel_calls | 150 | 126 | 680 074 | 440 074 |
| stress | 103 | 105 | 150 049 | 150 049 |

The machine was quieter for this step, so its `before` column is lower than step 2's `after`. `dictionary_keys`
is timed against the step-2 compiler with the step-2 `library/dictionary.spite`; the other rows share the
library, which a compiler reads from beside itself. Compiling the compiler: 1 905 ms before this work, 1 484 ms
after step 2, 1 339 ms after step 3 (same machine state, best of seven).

`stress` has not moved: its cost is reading `moving.position.left` through a `type`, where each read of the shape
raises and lowers the component's count, which none of these steps removes.

### Step 4: plain attributes read and written through a `type` borrow the component

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| stress | 110 | 114 | 150 049 | 150 049 |
| stress, counts atomic (`SPITE_THREADS`, as in a program that makes a `Parallel`) | 162 | 129 | 150 049 | 150 049 |

In a program without threads a retain is one plain add and `stress` spends its time filling rows and spawning, so
nothing moves; with atomic counts, as SlopEngine has whenever it runs systems in parallel, the ticks are a fifth
faster. The second row is the same two C files built with `-DSPITE_THREADS` prepended.

### Taken out again: freed small blocks kept for reuse (step 3)

Mortaro's rule for step 3's small-block reuse was to keep it only if it is a measured gain for SlopEngine. It was
measured on a copy of SlopEngine: each program's C written once by the compiler, then built with `clang -O2`
twice -- as written (reuse) and with `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` defined as the C library's
`malloc`, `realloc` and `free` (plain, what every build now does) -- and run interleaved, nine times each. SlopEngine
runs systems in parallel, so these programs count references atomically. Best of nine; the medians agree, and the
ordinary machine noise is 2-5% (the UI tests and `flex_layout` also have occasional runs 250 ms slower on both
sides).

| SlopEngine program | reuse | plain |
|---|---|---|
| `stress`, average tick, parallel | 42.9 ms | 41.9 ms |
| `stress`, average tick, single-threaded (`--parallel=false`) | 49.0 ms | 50.9 ms |
| `stress`, spawning 200 000 bodies | 449 ms | 504 ms |
| `stress`, 60 ticks after despawning them | 87.7 ms | 87.2 ms |
| `flex_layout`, whole run | 68.9 ms | 72.4 ms |
| `click_counter_test`, whole run (Vulkan, validation on) | 1 008 ms | 1 020 ms |
| `text_field_test`, whole run | 827 ms | 820 ms |

Three rounds agreed: the parallel tick, which is what SlopEngine runs, was 1-3% slower with reuse every time; the
single-threaded tick 0-4% faster; only spawning (11%) and `flex_layout` (about 3 ms) were clearly faster, both
one-off work. Not a clear gain on the engine, so it is gone, with up to 2 MB it kept per thread. The benchmarks
here, the same way (best of nine, same C, only the allocator differs), are where it had helped:

| benchmark | reuse ms | plain ms |
|---|---|---|
| fused_chain | 139 | 138 |
| dictionary_keys | 185 | 185 |
| text_building | 159 | 151 |
| reflection_walks | 264 | 349 |
| function_values | 67 | 72 |
| small_allocations | 84 | 140 |
| parallel_calls | 162 | 141 |
| stress | 108 | 101 |

After the change `run.sh` reports the same allocation counts as the "after" column at the top, every one: taking
the reuse out, and leaving the counter out of builds that do not read it, change no allocation.
