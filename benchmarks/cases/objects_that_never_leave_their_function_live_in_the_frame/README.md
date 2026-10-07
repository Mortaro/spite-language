# Objects that never leave their function live in the frame

`velocity = velocity + pulled` and `var moved = velocity.scaled(0.001)` read as a new `Offset` per answer, since
every class is a reference. The compiler proves none of these objects outlives `simulate`, so each one gets a slot
in the function's own frame, `scaled` and `sum` write their answer straight into the caller's slot, and the loop
allocates nothing.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame).
- The proof: [Frame objects](../../../docs/proofs.md#objects-that-never-leave-their-function-live-in-the-frame).

## The four forms

- [`naive/`](naive/): a program's own class of three `Float`s, `Offset`, with `sum` (the `+`) and `scaled`, and
  20 000 000 steps of a falling point: the velocity pulled by gravity, the position moved by the velocity.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct for `Offset`, each
  answer of `sum` and `scaled` a `malloc`, and the object it replaces freed: four allocations a step.
- [`expert.c`](expert.c): the same work tuned by hand: the three numbers held by value, every step written out in
  the loop, nothing allocated.
- [`generated.c`](generated.c): what the compiler writes for `Offset`, `simulate` and the two functions it calls.

## What to look at in generated.c

Every `Offset` in `Naive_simulate___into` is a local `Offset spite_slot_<n>;` in the frame, and every answer comes
from a `___into` version of its function that fills the slot it is handed (`Offset_scaled___into(gravity_, ...,
&spite_slot_3)`). `velocity = velocity + pulled` is worked out in a second slot and copied back with
`Offset___copy_fields`, and `simulate` itself, which returns its `position`, writes it into the slot `Naive` passes
(`spite_result`), so the object is never moved to the heap either. Under `--debug-memory` the whole program makes
6 allocations in all and none in the loop, where `naive.c` makes 80 000 003. `expert.c` holds the same numbers
in registers.

One thing still left in: the argument of `sum` is counted, `Offset___retain(pulled_)` before the call and
`Offset___release(other_)` inside it. On a frame object that count is never read, so it is only two additions a
call, but the [held argument](../../../docs/optimizations.md#an-argument-its-caller-holds-is-passed-without-counting)
copy that drops it for a call by name is not used for an operator written as `+`.

## Timings

<!-- timings -->
<!-- /timings -->
