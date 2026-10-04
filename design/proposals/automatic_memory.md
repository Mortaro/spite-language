# Automatic memory, layout and threading: what the compiler must prove to retire the Memory API

**Status:** an investigation Mortaro asked for (answering item 288), not a decision. Every rule below is proposed
by Claude, unconfirmed. Mortaro's direction: the `Memory` API (`Memory.Arena`, `.memory.allocator`, hand-made
frame scratch, hand structure-of-arrays) is a leak in the abstraction. Spite should know on its own when something
belongs in an arena, when a ring buffer, when a structure of arrays or an array of structures, and perhaps even
when to run in parallel or concurrently, so a program says what it means and gets the fastest form. The targets:
a game engine written in Spite much faster than the fastest engines in other languages, and Spite faster than
average C.

## The evidence: a real engine written in Spite

A read-only survey of a game engine package written in Spite (39,552 lines outside its examples) counted what the
program decides by hand that the compiler could decide instead. About 4,500 to 6,000 lines (12 to 15%) are memory,
layout or threading plumbing, not game or engine logic; counting the GPU plugins' struct and lifetime code, close
to 25%. Every one of those lines is a place a person could get it wrong, and most are why the engine is fast
today: its stress history went from 302 ms a tick to about 8 ms (200,000 entities, 2 systems) almost entirely by
hand placement (cached column headers, inline storage, padded guards, per-runner buffers).

| What the program decides by hand | What the compiler would have to prove to decide it instead |
|---|---|
| Component storage: a 64-byte raw header with magic offsets, parallel arrays per field, swap-remove | which classes a set holds; that every access goes through one API (no alias into the arrays); which fields each pass reads |
| Borrow an item in place, or copy it and write it back | no reference to the item escapes the pass, and nothing resizes the storage while it is borrowed |
| Deferred, budgeted freeing (4096 a tick, then forcing the allocator to settle) | where removed values' lifetimes end; that the frees of a batch are independent; a cost model to spread them |
| Per-frame scratch pools, cleared and reused | the value's lifetime ends with one pass of the loop (it never outlives the tick), so an arena reset per pass is safe |
| Per-call OS and GPU structures, allocated and freed in one function, aligned by hand | no escape past the call (placement's frame proof, extended to foreign calls) and the target's layout rules |
| GPU buffers freed only after the fence two frames later | the GPU's last use, tied to a fence: a lifetime plain data flow cannot see |
| Byte offsets and strides that match shaders | a layout shared with the shader's declaration, and no other reader of the buffer |
| Hand structure-of-arrays in physics, navigation and lights; tombstones; generation stamps instead of clearing | fields read separately in hot loops; slot indices are the only identity; a cleared array is never read before written |
| Which systems run together in a stage | each function's read and write sets per field, including writes through singletons and lent values |
| Per-thread command queues applied in order; a locked id counter | structural changes commute or are ordered; the counter is the only shared state (atomic, or a range per thread) |
| Band sizes for `Parallel` splits (1024 blocks, 65,536 pixels), a raw address to skip the sharing check | iterations are independent (disjoint outputs, read-only input); the input outlives every task; a cost model for grain |
| A `Concurrent` per IO wait, drained up to 8 a frame | which functions wait (already known); snapshots are never written; a bound on waits in flight |

## What the compiler already proves

Much of this is not new work: it is existing proofs ([docs/proofs.md](../../docs/proofs.md)) carried further.

- **Placement** already puts an object that never escapes its function in the frame, and a constant list in
  constant data. A frame arena is the same proof with "the loop pass" in place of "the call".
- **Call effects** (what a function writes, whether it waits, whether it may add to a list) already decide when a
  `Vector` may lend its items. They are the read and write sets a scheduler needs.
- **The `Parallel` sharing check** already refuses a task that writes what another reads. Turned around, it is
  the proof that two pieces of work may run at once.
- **Escape analysis** written for the lost-write error (D474) already follows a local into calls, attributes,
  lists and closures.

## Proposed path, cheapest first (proposed by Claude, unconfirmed)

1. **Frame arenas, decided by the compiler** (answers item 288's original question). A loop whose pass ends in a
   wait (`program.sleep`, a frame present, a `Concurrent` wait) is a frame. A value made in a pass that is proven
   never to outlive it goes into an arena the compiler resets at the end of the pass. A value that would outlive it
   is placed on the heap as today, so nothing is ever freed early; `--debug-memory` counts arena placements apart,
   and `docs/optimizations.md` says when it applies. This deletes the engine's hand scratch pools.
2. **Structure of arrays, decided by the compiler**, for a `List` of a class whose items have no identity outside
   it (no reference to an item escapes; the slot is its only name) and whose hot loops read a subset of the
   fields. This is D222's inline-object case plus a field-access count; the one-list study's measurements say where
   it pays and where a refusal costs.
3. **Ring buffers**: a list used only as a queue (append at one end, remove at the other) with a bound the compiler
   can see becomes a ring. Unbounded queues stay growable.
4. **Deferred freeing**: a batch of objects let go together whose `drop`s are independent is freed in slices
   between passes of the frame loop, with no budget written by hand.
5. **Scheduling**: per-field read and write sets of each system decide which run together, and a loop over
   independent items runs in parallel when the cost model says it pays. The one-list study found that a blind
   parallel `each` makes light bodies 2.4x slower, so this needs a cost model before it is automatic.
6. **What stays explicit, for now**: GPU lifetimes tied to fences and layouts shared with shaders. Both need a
   declaration the compiler can read (a foreign resource's lifetime, a shared layout), not a guess.

When each step is built, `Memory.Arena`, hand pools and hand structure-of-arrays become things a program no longer
needs to write. Removing them from the language, and whether `Parallel` and `Concurrent` stay as things a program
writes at all, is Mortaro's call once the automatic forms are measured against the hand-written engine.

## Zero runtime and tree shaking

Every step is a choice of code made while compiling: an arena is a pointer bump and a reset placed by the compiler,
a ring is index arithmetic, structure of arrays is a different struct layout. A program that has no frame loop
gets no arena; a list that is never a queue is never a ring. Nothing ships a scheduler or a collector.

## Measured (2026-10-04, D501)

The engine's stress example (200 000 entities, `Move` and `Regenerate` running at once), built `--optimized` on a
4-core Linux machine, before anything below was built: a median tick of about 60 ms, the same as the compiler the
engine was last validated against (`8e971f26`), so nothing had regressed. Where the time went:

- **Not memory.** A tick makes about 96 objects whatever the entity count (20 more ticks made 1 920 more
  allocations at 20 000 entities). A frame arena would save at most those, a few microseconds of a tick. The
  `stress` loop calls `app.tick()` with no wait, so it has no frames to give an arena anyway. Step 1 stays the plan
  for programs whose frames allocate; it buys this benchmark nothing.
- **Locks.** Compiled with every singleton's lock left out, a tick took 10 to 17 ms: about three quarters of it was
  locking. Nearly all of that was one lock: both systems read `Columns.stage_counter` once per entity and component
  through `Columns`' lock, so two cores queued for one cache line to read one number. That is the proposal's "the
  counter is the only shared state (atomic)" row, built as an attribute atomic on its own
  ([optimizations.md](../../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)): a tick
  went from about 63 ms to about 20 ms.
- **What is left.** The rest of the gap to the lock-free 10 to 17 ms is the row matcher's and the columns' locks,
  taken once per entity and call although each system's runner is the only one using its row: holding them for
  the whole stream (a counted loop taking several singletons' locks once, in a fixed order) is the next step for
  this benchmark, ahead of structure of arrays.

## How it is measured

The engine package's stress example, compiled by this tree's compiler, before and after each step, with the hand
code removed in a worktree folder that loads the engine and reopens the classes it changes. A step is kept only if
the automatic form is at least as fast as the hand form it replaces.
