# Memory ownership and allocators: a proposal to decide

Mortaro asked (2026-09-25): what gets SlopEngine to beat Bevy, whether users can choose allocators easily ("our ECS
does not want reference counting, it wants arena ring buffers"), and why `Memory` and a proposed `Address` look
like two classes for one thing. Worked out with the SlopEngine session from its real hot path. Everything here is a
proposal (Claude, unconfirmed); the questions to answer are at the end.

## Where the time goes today

SlopEngine's stress example: 200,000 entities, 2 systems, 2 components per row, **50-60 ms per tick** -- about
**150 ns per entity per system**. Bevy's simple iteration is about **1 ns**. Estimated from the code (not yet
profiled):

| Cost | Why | Share (estimate) |
|---|---|---|
| Every component is its own heap object | a column holds pointers; 800,000 objects scattered in memory | largest |
| A list of every entity id built each tick | `candidates()` allocates and fills 200,000 ids | large |
| Matching each entity three times | sparse lookups through bounds-checked lists | large |
| 4-6 retain/release per component visit | reference counting, atomic when threads are on | 15-30% |
| Attribute access through a walk | generic read/write per field | medium |

So reference counting is real but not the main problem: **the layout is**. Bevy is fast because a component is plain
data packed next to its neighbours, read without a pointer, a header or a count.

## The one idea that matters most: value classes

A **value class** is copied, not referenced: no header, no reference count, no identity. A list of them is one
contiguous block of the data itself.

```gdscript
# slop/position.spite
value

var x: Float = 0.0
var y: Float = 0.0
```

```gdscript
func update_each(position: Position, velocity: Velocity) {
    position.x = position.x + velocity.x     # writes straight into the column
    position.y = position.y + velocity.y
}
```

- A `List<Position>` becomes `x, y, x, y, ...` in memory; iterating it is a pointer increment, as in C or Bevy.
- Passing one copies it (16 bytes here), so there is nothing to count and nothing to free.
- `String` is already a value class in the compiler (`library/string.spite`); this makes the idea available to
  user classes.
- **Tradeoff:** a value has no identity -- two equal `Position`s are the same thing, `==` compares fields, and a
  method that changes it changes that copy. A value can hold a `String` (the string stays reference counted); it
  cannot hold itself.
- **Estimated effect:** most of the gap. With inline columns and a single fused loop per system, simple iteration can
  approach Bevy's; SlopEngine's sparse sets should win `add_remove` and scheduling outright.

## Allocators: how a program chooses where memory comes from

Three shapes are common. SlopEngine's ranking, which matches Spite's "no noise" rule:

**(a) A class says where its instances live** (best for things that are always short-lived):

```gdscript
allocator FrameArena        # every Particle is made in the frame arena, freed all at once each frame

var position = Position()
var life: Float = 1.0
```

**(b) A scoped block for everything made inside it** (what an engine uses so users do not have to):

```gdscript
func run_system() {
    frame.arena {                       # hypothetical syntax: made here, freed when the frame resets
        system.update_each(rows())
    }
}
```

**(c) An explicit arena object** (for library and engine internals):

```gdscript
var arena = Arena(1048576)
var scratch = arena.make(List<Integer>())
arena.reset()
```

**Rejected: Zig-style passing an allocator to every function** -- every function in a system file would gain a
parameter, which is exactly the noise Spite removes.

- **Tradeoff of arenas:** freeing everything at once is nearly free (reset one pointer), but an object must not
  outlive its arena. **That has to be a compile error, not a crash:** a frame-lived value cannot be stored into an
  attribute, a component, a list or anything else that outlives the frame; the error says to copy it out. Debug
  builds can add a generation check as a backstop.
- Ring buffers (commands, events) are just an arena that wraps around; free-list slots with generation counters
  handle entity ids.

## Memory and Address: one place, one owner

The confusion is real. SlopEngine writes `memory.read_long(header, 16)` a hundred times, with `Memory` as a bag of
free functions. The split that reads best:

- **`Address`** is *a place in memory*: a number (like `Long`) with typed reads and writes --
  `header.read_long(16)`, `header.write_float(8, 1.5)`. Its reads and writes are the machine's own operations (item
  113 in the decisions file), the same kind of primitive as `+`.
- **`Memory`, `Arena`, `FrameArena`** are *who owns memory*: allocators that hand out `Address`es and take them back.
  `Memory` is the default one (pages from the operating system, a small-object allocator written in Spite on top).

So there is not "two ways to read bytes": reading is always on `Address`, and allocating is always on an allocator.
User-swappable allocators are then just more classes with the same shape as `Memory`.

## What wins which Bevy benchmark (ecs_bench_suite and Bevy's own)

| Benchmark | Today | With value classes and a fused loop |
|---|---|---|
| `simple_iter`, `heavy_compute` | lose 10-100x | winnable |
| `add_remove` | lose | likely win (sparse sets never move data between archetypes) |
| `schedule`, parallel systems | lose | likely win (access known at compile time, nothing fetched per tick) |
| `simple_insert` | lose | winnable once spawning is batched |
| `frag_iter`, many-component queries | lose | likely still lose (archetypes give perfect locality; sparse sets pay a lookup per extra component) |
| `serialize` | nothing comparable | `Json<T>` exists; binary format needed |

## Questions for Mortaro

1. **Value classes**: add a `value` header line (a class is copied, not referenced, laid out inline)? This is the
   big one for performance.
2. **Allocator choice**: which shapes -- (a) a class header `allocator X`, (b) a scoped block for engines, (c) explicit
   arena objects -- or a different one?
3. **Safety rule**: a frame-lived value stored somewhere that outlives its frame is a compile error. Agree?
4. **Memory vs Address**: `Address` is the place (reads and writes), allocators (`Memory`, `Arena`, ...) are the
   owners. Agree, and does that settle item 113?
