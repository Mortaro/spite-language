# The high IR

The high IR is the whole program after the compiler has understood it and before anything is decided about how it
runs. It is built by the lowering ([the flow](README.md#the-flow)), rewritten by the high passes, and lowered to the
[low IR](low_ir.md). Its code is `compiler/source/intermediate/` (namespace `Intermediate`).

- [What it is](#what-it-is)
- [The form](#the-form)
- [The passes](#the-passes)
- [Reading it](#reading-it)

## What it is

It is **desugared**: every operator, index, interpolation, `switch` and `assert` is already the call or the branch it
means, so a pass never needs a case per syntax. It is **monomorphised**: every template, generic class and value folded
while compiling is resolved, so every function, class and type in it is concrete. It is **checked**: a program that
reaches the high IR has no compile error left.

It has few kinds of operation on purpose ([D587, D599](../DECISIONS.md)). **Nothing that is a function in the end
becomes an operation of its own**: what the source can say as a call of a function is that call, and a pass decides
what the call becomes (a read of a list item is a call of `get_at`, a write a call of `set_at`). A new intrinsic is
only for what has no Spite function under it. The simpler the IR, the more the passes can see to be the same and
collapse; every operation kind added is a case every pass must learn.

It is held only while compiling and ships nothing: whatever a pass decides is decided once, and the program carries
only the result.

## The form

| Class | What it is |
|---|---|
| `Lowered` | the program: its layouts, its functions, its sites, the entry function, the build settings, and the foreign-library and symbol tables |
| `Layout` | a concrete class: its stored fields and what the passes decided about its storage |
| `Field` | a stored attribute: name, type, and whether it is atomic or changes |
| `Function` | a concrete function: owner, parameters, answer, blocks, values, and what the passes found |
| `Parameter` | name, type, the value that holds it, and its ownership |
| `Block` | its parameters, its operations, and one terminator |
| `Value` | one value in SSA form: its number, type, name and where it came from |
| `Operation` | one step that makes at most one value |
| `Terminator` | how a block ends |
| `Target` | a block to go to with the values it receives, and a tag for a switch case |
| `Site` | where a crash or a check reports from: path, line, owner, function, source text |

**Functions are SSA.** A local that differs by path arrives as a block parameter; a loop carries every local its body
assigns as the parameters of its test block.

**Operations.** There are eight kinds, and every pass handles all of them.

| Kind | What it is |
|---|---|
| `constant` | a literal of a concrete type, or the name of a function or a layout |
| `call` | a call of a concrete function, an intrinsic included, with its operands in order, the receiver first |
| `call value` | a call through a function value, which is the first operand |
| `load` | a field of an object |
| `store` | an object, then the value stored into its field; it consumes the value |
| `make` | a new object of a layout, not yet constructed; its constructor is the call right after it |
| `retain`, `release` | a count up or down, written only by the ownership pass |

**Checks are calls.** A check that can fail is an intrinsic call that can fail (`Integer.sum`, `Integer.to_byte`,
`Nullable.value`) carrying a site; a `crash` is a `halt` terminator with a site. A pass that proves a check never fails
marks the operation so; a proof only ever removes a check, never adds one.

**Forms.** A pass chooses how an operation is carried out by setting its `form` (`unchecked`, `borrowed`, `moved`,
`frame`, `in_place`, `locked`, `atomic` and the others its pass names), never by changing its kind or its callee. The
lowering to the low IR reads the form and decides nothing else.

**Terminators.** `jump` (one target with its arguments), `branch` (on a Boolean, the first target when true),
`switch` (on a tag, one target per union member or enum value), `return` (with an optional value), and `halt` (a crash,
with its site and what it shows).

**Intrinsics.** A function with no body whose meaning the lowering to the low IR knows, named `<Type>.<operation>`:
the arithmetic and comparisons of the numbers, `Boolean.not`, `String.join`, `Nullable.has`, `Nullable.value`,
`Nullable.wrap`, `Union.tag`, `Union.value`, `Union.wrap`, `Object.deep_copy`, `Function.bind`, `Foreign.call`,
`Arguments.get`, `Singleton.peek`, and the rest of a closed list.

**Sites.** Every report prints from one table of sites, so a crash says the same thing wherever its check ends up
after inlining, folding or moving.

## The passes

They run in this order, each rewriting the IR in place; each asks the read-only facts it needs (flow and loops, where
a value is defined and read, what a function reads, writes, calls and waits on, which classes several threads reach)
and rebuilds them after a rewrite. A verifier runs after every pass and refuses a value used but never defined, a use
its definition does not come before on every path, and a jump whose arguments do not match its target, naming the pass.
Builds that inspect a running program (the REPL, hot reload, development builds) skip the passes that would remove what
they inspect. What a user can observe of each is documented in [optimizations](../docs/optimizations.md) and
[proofs](../docs/proofs.md), in the same change as the pass.

1. **Inlining.** Which calls are replaced by the callee's body: small functions that are not recursive and hold no
   check whose report would change. A default the constructor replaces at once is never made, and an object a loop
   carries is not remade each pass.
2. **Proofs.** Which checks cannot fail and which conditions are known: closed computations folded, branches decided,
   loop-invariant reads lifted, arithmetic proved to fit, reads proved inside their bounds, tests against values a
   list never holds decided. A call keeps a proof only when what it may change does not touch what the proof used.
3. **Ownership.** Who counts what: every parameter is `owned` (the callee lets it go), `held` (the caller keeps it for
   the whole call) or `lent` (an item lent from a list, counted by no one); counts are written only where a value's
   origin needs one, moves replace counts, copies are decided, and appends to text are made in place. A row of
   borrowed items counts nothing unless it escapes.
4. **Placement.** Where memory lives: an object that never leaves its function, or a local list of known size, lives
   in the frame; a result is made in its caller's frame; objects of a class made on one thread are pooled.
5. **Arrangement.** How a class is laid out: a singleton that holds nothing is a static object, a list held only by
   another list lives in its slot, a table filled once is read as constants.
6. **Concurrency.** What runs at once and how threads share: waits that can overlap are started early, loops whose
   passes write only their own item run in pieces on several threads, a class no other thread reaches keeps plain
   counts, and each singleton gets the cheapest safe form. A decision is made only when the facts prove it.
7. **Folding.** Identical functions become one.
8. **Reach.** The tree shaker: from the entry and what the build needs, everything nothing reaches is dropped, so a
   program that does not use a feature carries none of it.

## Reading it

`--high-ir=<folder>` writes one `<class>.ir` file per class, after the passes. For this program:

```
var console = Console()
var total = 0

func counter() {
    var index = 0
    while index < 3 {
        add(index)
        index = index + 1
    }
    console.print(total)
}

func add(amount: Integer) {
    total = total + amount
}
```

`Counter.ir` reads (shortened):

```
function Counter_add(v0: receiver, v1 amount: owned Integer): nothing
    block 0():
        v2: Integer = load v0.total
        v3: Integer = call Integer.sum(v2, v1) site 67
        store total v0, v3
        return

constructor Counter_Counter(v0: receiver): nothing
    block 0():
        v1: Integer = constant 0
        jump  block 1(v1)
    block 1(v2):
        v3: Integer = constant 3
        v4: Boolean = call Integer.less_than(v2, v3)
        branch v4 block 2(), block 3()
    block 2():
        call Counter_add(v0, v2)
        v5: Integer = constant 1
        v6: Integer = call Integer.sum(v2, v5) [unchecked] site 50
        jump  block 1(v6)
    ...
```

`v<n>` is a value with its type after the colon. `block 1(v2)` takes `v2`, the loop counter, which arrives as `v1`
from block 0 and as `v6` from block 2. `[unchecked]` is the form a proof set: the loop bound proves `index + 1` fits,
so its check is gone, while `Counter_add`'s sum keeps `site 67` because nothing proves it. `owned` is the parameter's
ownership.
