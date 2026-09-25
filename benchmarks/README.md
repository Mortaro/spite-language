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

### Short text inside the `String` (D203)

A `String` is a sixteen-byte value and keeps text of up to 15 bytes in itself
([optimizations.md](../docs/optimizations.md#short-text-lives-inside-the-string)). `before` is the compiler at
`1e1162d`, each side in its own tree; best of three interleaved rounds of seven runs, on a machine other sessions
were loading (the same binary moved by up to 80% between rounds, so a row within 10% has not moved). The
allocation counts are exact.

| benchmark | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| fused_chain | 154 | 155 | 200 009 | 200 007 |
| dictionary_keys | 207 | 177 | 1 104 014 | 1 032 |
| text_building | 173 | 143 | 800 227 | 165 |
| reflection_walks | 298 | 219 | 7 596 024 | 3 999 918 |
| function_values | 85 | 85 | 400 019 | 400 013 |
| small_allocations | 100 | 99 | 9 004 013 | 9 004 009 |
| parallel_calls | 176 | 164 | 440 074 | 440 041 |
| stress | 127 | 128 | 150 049 | 150 043 |

Compiling the compiler: 1 372 ms before, 1 295 ms after (best of five, interleaved). Of the 4.7 million texts it
makes compiling itself, 68% are 15 bytes or fewer.

SlopEngine, from a copy, each example built `--optimized` with `clang -O2` (best of five whole runs) and once more
with `--debug-memory` for the count:

| example | before ms | after ms | before allocations | after allocations |
|---|---|---|---|---|
| `stress` (200 000 entities) | 1 518 | 1 485 | 7 202 769 | 5 602 651 |
| `click_counter_test` (Vulkan, hidden window) | 975 | 984 | 78 363 | 63 883 |
| `flex_layout` | 71 | 73 | 127 330 | 99 824 |

In `stress`, spawning (463 → 455 ms) and the average tick (43.0 → 42.6 ms) did not get slower, but the ticks right
after despawning everything, which look each column up by a name of 16 to 22 bytes once per entity, did: 89 → 100
ms for sixty ticks. A dictionary lookup by a key longer than 15 bytes is about 10% slower than before (a lookup
loop over four such keys: 190 → 210 ms), because the key is passed as sixteen bytes rather than a pointer and
compared through the form it takes. Before `code_at` became the compiler's (the third D203 commit) the same loop
took 315 ms: the hash re-copied the key for every character.
