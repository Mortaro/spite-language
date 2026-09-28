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
| `dictionary_keys` | a `Dictionary` keyed by 50 000 numbers (by text made from them before D224) and one by 2 000 names, set and read |
| `number_keys` | 100 000 entries set and a million lookups, once keyed by the number itself and once by text made from it (`index.to_string()`); prints the milliseconds of each |
| `text_building` | appending to text in a loop, `"word{index}"` pieces and `join` |
| `reflection_walks` | a Symbol walk (`show_attributes`), a `.attributes` walk reading `.value`, and `JsonWriter` |
| `serialisation` | 100 000 small objects written and read back as JSON (`JsonWriter`/`JsonReader`, one text each) and as bytes (`BinaryWriter.append_to` into one `List<Byte>`, a `Vector<Byte>` before D225, `BinaryReader`); prints the sizes and the milliseconds of each step |
| `function_values` | `each(f)`, `filter(f)`, `sum(f)`, `count(f)` and a function value passed 200 000 times |
| `small_allocations` | three small objects made and dropped per pass, three million passes, and `copy()` |
| `parallel_calls` | 20 000 rounds of two `Parallel`s |
| `vector_items` | 200 000 `Velocity` items in a `List<Velocity>` and in a `Vector<Velocity>`, 100 ticks of `each_integrate()` and a fused `filter_moving().sum_across()` on each; prints the microseconds per tick of both |
| `vector_rows` | 200 000 entities with position and velocity, half with health and regeneration, `Move` and `Regenerate` systems taking a `type` row per entity for 20 ticks: once from four `Vector` columns as rows of borrowed items (D206), once from four `List` columns through a reused row object; prints the microseconds per tick of both |
| `sparse_rows` | 200 000 entities kept in sparse sets (a generic singleton `Column<T>` per component, each entity at a different place in each), position and velocity for all, health and regeneration for half, `Move` and `Regenerate` taking a walked row per entity for 20 ticks: once with `Vector` columns and rows filled by D217's walk (borrowed items, an `Entity` made in the frame), once with every column a `List` of references and a row object reused across the tick; prints the microseconds per tick of both |
| `items_storage` | 200 000 `Velocity` items (they fit a `Vector`) in a `Vector` and an `Items`, and 200 000 `Trail` objects (they hold a `List`) in a `List` and an `Items` sharing the same objects: filling, 100 ticks of `each_integrate()` and a fused `filter_moving().sum_across()`, and 100 ticks of 200 000 `[]` reads and writes at scattered places; three rounds, each printing microseconds per tick of all four |
| `matched_rows` | SlopEngine's walked-row shape: 200 000 entities in sparse sets over `Vector` columns, `Move` and `Regenerate` taking a walked row per entity for 20 ticks, each entity's places found by a `Matcher<$row_type>` object into its own `List<Integer>` (a `Vector<Integer>` before D225): once passing `matcher.rows` straight to the walk (D221), once copying the places into the runner's vector first, as a runner had to before; three rounds, each printing microseconds per tick of both |
| `lent_arguments` | 200 000 entities in sparse sets over `Items` columns, systems taking their components as arguments (`Move(position, velocity)`, `Regenerate(health, regeneration)`, `Drift(velocity)`), 20 ticks: once with `system.phase_each(made_arguments(found))` passing borrowed items (D220), once copying each argument out of its column, passing it and storing it back; three rounds, each printing microseconds per tick of both. Measured 6.9 ms against 34.3 ms a tick (best of five, `clang -O2`) |
| `stress` | SlopEngine's `examples/stress` shape: component columns as generic singletons, `system/` classes with `update_each` over `type` rows filled by a Symbol walk, 50 000 entities, 20 ticks |
| `console_lines` | 200 000 `console.print` lines, each written out as it is printed; `run.sh` times it into a pipe, and redirected to a file is where the write per line costs (about 700 ms against 140 ms buffered until exit, [standard_library.md](../docs/standard_library.md#system-classes--implemented)) |
| `game_maths` | a million `position + velocity.scaled(delta)` steps on `Vector3`, 200 000 `Matrix4` products and a million `transform_point`s; each answer is a new object, so the allocation count is the point |
| `half_precision` | ten million `Float.to_half_precision()` and `half_precision_to_float()` round trips, over the bit views `Float.bits()` and `UnsignedInteger.bits_as_float()` (D215); prints the milliseconds. Before the bit views were C unions: 482 ms at `-O0`, 13 ms from `-O1`; after: 416 ms and 13 ms; 15 allocations either way, none per conversion |
| `maths_stopgaps` | two million passes of sine, cosine, arc tangent, square root, floor and a power of two on a `Float`, first through the pure-Spite stopgaps SlopEngine wrote while Spite had no maths (`slop/math/scalar.spite`, copied in as `stopgap_scalar.spite`), then through the number classes' own maths functions; prints the milliseconds of each and the largest error of the stopgap `sine` over one turn |
| `plain_loops` | a million `Float`s and `Integer`s: `into[index] = from[index] * 1.5 + 0.25` over two `List<Float>`, the same in place over one `List<Float>` (a `Vector<Float>` before D225, printed as `Vector scale` then and `in place` now), a `Float` sum and an `Integer` sum, each a plain `while` over `count()`; three rounds, each printing microseconds per pass of all four. The loops the C compiler vectorises once the count is read once and the items unchecked ([optimizations.md](../docs/optimizations.md#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked)) |

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

### Rows of borrowed items (D206)

`vector_rows`, the program's own two timers (best of five runs, `clang -O2`, the compiler of the commit that adds
rows). Each entity's columns are found through a `Vector<Integer>` per component, the same for both layouts, so the
difference is only where the components live and how the system is handed them:

| layout | microseconds per tick |
|---|---|
| four `Vector` columns, a row of borrowed items per entity (in the frame, nothing counted) | 2 381 |
| four `List` columns, one row object reused across the tick (two counted assignments per entity) | 5 823 |

The 600 171 allocations are filling: 200 000 positions and velocities, 100 000 healths and regenerations, each
kept in its list and copied into its vector, and the two row objects per list tick; the rows make none. The same
walk written by hand in one loop, each item named with `var position = positions[row]` and no system call, ran at
about 2.0 ms a tick in a copy of the program: the row and the call through the `type` cost a fifth more than
indexing the columns directly, in exchange for systems that are ordinary functions over a `type`. (The 1.0 ms
quoted in D206 is the earlier, different program described at the end of the `Vector<T>` section above.)

`stress` itself keeps its generic `Row` singleton, which fills a row by a Symbol walk and holds it in an
attribute, so it cannot hold borrowed items (D212 makes a local filled by a Symbol walk a row; `stress` still keeps
its row in an attribute). A copy with `Vector` columns and a hand-written runner that builds a row literal per
entity for each system ran the whole program (50 000 entities, 20 ticks) in 89 ms against `stress`'s 129 ms, best
of seven interleaved.

### Walked rows over sparse columns (D217)

`sparse_rows`, the program's own two timers (best of five runs, `clang -O2`, the compiler of the commit that builds
D217). Both sides keep every component in a sparse set and find each entity's place in each column with the same
generic walk; they differ only in where the components live and how the row is made:

| layout | microseconds per tick |
|---|---|
| `Vector` columns, a walked row per entity (borrowed items, an `Entity` made in the frame, nothing counted) | 7 417 |
| `List` columns of references (an `Entity` column too), one row object reused across the tick, filled by the ordinary walk | 24 022 |

The 800 115 allocations are filling: 200 000 entities, positions and velocities and 100 000 healths and
regenerations, each kept in its reference column and copied into its vector, and the sparse sets; the walked rows
make none. Most of the 7.4 ms is finding the places, a sparse lookup through a generic singleton per attribute,
which both sides pay.

### Places read from another object (D221)

`matched_rows`, `clang -O2`, three runs of three rounds each on Mortaro's machine. The compiler before D221 could
not write the walk that takes `matcher.rows` (the row fell back to an ordinary `type` value and keeping a borrowed
item in it was an error), so its program has the copying runner in both places:

| compiler | walk over `matcher.rows`, µs per tick | places copied first, µs per tick |
|---|---|---|
| before (`c1edb49`) | -- | 8 012-8 391 |
| after | 6 701-6 943 | 8 038-8 352 |

The copy was a loop of `append`s per entity through a vector the runner kept; reading the places where they are
removes it, about 17% of the tick. `sparse_rows` and `lent_arguments` compile to the same C before and after.

### `Items<T>`: storage chosen while compiling (D218)

`items_storage`, the program's own timers, three rounds in one run (`clang -O2`, the compiler of the commit that
builds D218, on a machine another session was also using). `Items<Velocity>` is inline, `Items<Trail>`
references; the `List<Trail>` and `Items<Trail>` hold the same 200 000 objects:

| collection | fill µs | template tick µs | `[]` tick µs |
|---|---|---|---|
| `Vector<Velocity>` | 6 788 | 201-218 | 573-601 |
| `Items<Velocity>` | 6 967 | 193-212 | 510-533 |
| `List<Trail>` | 15 481 | 534-645 | 911-1 518 |
| `Items<Trail>` | 15 466 | 515-668 | 818-948 |

The templates are the same C either way. `[]` is faster than both because `Items.get_at` keeps its crash report
in a function of its own and is inlined where it is called; with the report inside it (the first build), the
`Items<Trail>` reads took 1 600-2 400 µs against the list's 1 000-1 900, because `List.get_at` was inlined and
`Items.get_at` was not. `benchmarks/sparse_rows` with its `Column` holding an `Items` instead of a `Vector` ran
6 863-7 010 µs a tick against 7 190-7 471 µs, five runs each, alternating.

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

### JSON and binary (D208)

`serialisation`, the compiler of the commit that adds `BinaryWriter` and `BinaryReader`, `clang -O2`: 100 000
objects of seven attributes (an `Integer`, a short `String`, two `Float`s, a `Short`, a `Boolean`, an enum). The
allocations are those of the writes and reads alone, counted with `--debug-memory` by a copy of the program that
does one half at a time.

| format | size | write ms | read ms | allocations |
|---|---|---|---|---|
| JSON, one text per object | 9 380 963 bytes | 212 | 241 | 9 347 676 |
| binary, all appended to one `Vector<Byte>` | 2 389 000 bytes | 16 | 8 | 300 010 |

The binary side allocates three times per object: the `BinaryWriter`, its cursor, and the object read back; its
walk is a singleton that holds nothing, so it allocates nothing itself.

### Maths functions against SlopEngine's stopgaps

`maths_stopgaps`, built with `clang -O2` by the compiler that added the maths functions, best of seven runs on
Mortaro's Windows machine (the C library is the Universal C Runtime's). Both passes add up the same six results
per angle, so they do the same work.

| pass | milliseconds |
|---|---|
| the stopgaps: a Taylor series for `sine` and `cosine`, a polynomial `arc_tangent`, 24 Newton steps for `square_root`, doubling and a series for `power_of_two` | 199 |
| the maths functions: `sinf`, `cosf`, `atan2f`, `sqrtf`, `floorf` and `powf`, written where they are called | 97 |

Without the power of two the two passes took 187 and 40 ms: `powf` is the one call here that costs more than a few
nanoseconds. The stopgap `sine` is also off by up to 3.6e-6 over one turn, where `sinf` is within one unit in the
last place.

### Game maths (D213)

`game_maths`, `clang -O2`, best of five on Mortaro's machine, with `--debug-memory` for the count: 3 200 067
allocations, which is one per answer -- two per vector step, one per matrix product and one per `transform_point`.

| pass | milliseconds |
|---|---|
| a million `position + velocity.scaled(delta)` on `Vector3` | 26 |
| 200 000 `Matrix4` products | 6 |
| a million `Matrix4.transform_point` | 0 (clang removes the allocation and the loop, since only a sum survives) |

### Dictionaries keyed by numbers (D224)

A dictionary given whole-number keys stores and hashes the numbers ([collections.md](../docs/collections.md#keyed-by-numbers),
[optimizations.md](../docs/optimizations.md#a-dictionary-keyed-by-numbers-hashes-the-numbers)). `clang -O2` on
Mortaro's Windows machine, best of five runs of `number_keys` (100 000 entries, a million lookups), the same source
compiled before D224 -- where `dictionary[index]` quietly turned the number into text -- and after:

| step | before ms | after ms |
|---|---|---|
| fill, keyed by the number | 10 | 5 |
| a million lookups, keyed by the number | 106 | 8 |
| a million lookups, keyed by `index.to_string()` | 65 | 66 |
| the whole program | 459 | 184 |

The text-keyed half does not move: a dictionary given text keys compiles to the code it did before. A lookup by
the number itself is about eight times faster than one by the text made from it. `dictionary_keys`, whose
50 000 number-derived keys are now the numbers, went from 179 to 142 ms, the 2 000 names unchanged.
