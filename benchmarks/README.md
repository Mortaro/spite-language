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
| `vector_items` | 200 000 `Velocity` items in a `List<Velocity>` and in a `Vector<Velocity>`, 100 ticks of `each_integrate()` and a fused `filter_moving().sum_across()` on each; prints the microseconds per tick of both |
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

### `Vector<T>`: items inline

`vector_items`, best of seven, the compiler of the commit that adds `Vector<T>`:

| layout | microseconds per tick | allocations while ticking |
|---|---|---|
| `List<Velocity>` | 443 | 0 |
| `Vector<Velocity>` | 211 | 0 |

A tick is `each_integrate()` over 200 000 items and a fused `filter_moving().sum_across()`. The list's objects were
made one after another, so they sit close together on the heap, which is the best case for a list; the vector reads
12 bytes per item where the list reads an 8-byte reference and then a 20-byte object somewhere else. The 200 149
allocations are filling: the 200 000 `Velocity` objects the list holds, each also copied into the vector's block,
which grows by doubling (a vector filled on its own would make each object only to copy it and let it go, which a
planned optimisation in [optimizations.md](../docs/optimizations.md#other-planned-optimisations) removes).

Shaped like SlopEngine's `examples/stress` (200 000 bodies, `Move` adding velocity to position and `Regenerate`
adding to health, 20 ticks, one thread), with each component in its own column indexed by row: 1.0 ms per tick with
four `Vector` columns against 5.9 ms with four `List` columns (whose objects were made interleaved). SlopEngine's own
column code could not be moved to `Vector` as it is: its `Row` keeps each component in an attribute of a `type` row
for the whole system call, which is exactly the keeping a borrowed item may not do (D204); its stress example runs at
51 ms per tick on the same machine.
