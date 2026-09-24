# Memory

D1 (manual.md section 10): Spite uses **reference counting**, JavaScript-like. A scalar (`Int`, `Float`, `Bool`,
an enum value) is a plain value, copied wherever it goes. Everything else -- a class instance, `List<T>`,
`Dictionary<T>`, `String`, a union, an object literal -- is a **reference**: assigning it, passing it, storing
it in a field/list/dictionary, and returning it all share the exact same object. There is no `&` and no
`Heap<T>` -- a reference is just the default, so a self-referential class/union needs nothing special either.

## Do: know that sharing is visible

Two names can hold the same object. A mutation through either one shows up through the other, because there is
only ever one object:

```spite title=sharing_basics/box.spite
var label = "unnamed"

func Box(starting_label: String) {
    label = starting_label
}
```
```spite title=sharing_basics/sharing_basics.spite entry
var console = Console()

func SharingBasics() {
    var original = Box("a")
    var alias = original
    alias.label = "b"
    console.print("original label", original.label)
}
```
```output
original label b
```

## Do: call `copy()`/`deep_copy()` for an independent object

`copy()` makes a fresh object with the same attributes (still shared references for any attribute that is
itself a reference). `deep_copy()` recurses, giving every reference-kind attribute its own independent copy
too:

```spite title=copy_basics/box.spite
var label = "unnamed"

func Box(starting_label: String) {
    label = starting_label
}
```
```spite title=copy_basics/copy_basics.spite entry
var console = Console()

func CopyBasics() {
    var original = Box("a")
    var independent = original.copy()
    independent.label = "b"
    console.print("original label", original.label)
    console.print("independent label", independent.label)
}
```
```output
original label a
independent label b
```

`deep_copy()` does not support cycles: a self-referential (or mutually referential) structure recurses
forever, so break the cycle by hand first if you need to deep-copy one.

## `drop()` runs once, right before the object is freed

A class may define a zero-argument `func drop() { ... }` for cleanup (closing a handle, clearing a
back-reference). The compiler calls it automatically the moment the last reference goes away -- never by name:

```spite title=drop_basics/resource.spite
var console = Console()
var name = "unnamed"

func Resource(new_name: String) {
    name = new_name
}

func drop() {
    console.print("dropped", name)
}
```
```spite title=drop_basics/drop_basics.spite entry
var console = Console()

func DropBasics() {
    var resource = Resource("first")
    console.print("using", resource.name)
}
```
```output
using first
dropped first
```

## Self-referential classes and unions just work

A tree, a linked list, or any other recursive structure needs nothing beyond an ordinary field -- no `Heap<T>`,
no explicit indirection:

```spite title=tree_basics/tree_node.spite
union TreeNode {
    Leaf
    Branch
}
```
```spite title=tree_basics/leaf.spite
var held_value = 0

func Leaf(starting_value: Int) {
    held_value = starting_value
}

func total(): Int {
    return held_value
}
```
```spite title=tree_basics/branch.spite
var left: TreeNode? = null
var right: TreeNode? = null

func Branch(left_node: TreeNode, right_node: TreeNode) {
    left = left_node
    right = right_node
}

func total(): Int {
    var result = 0
    if left {
        result = result + left.total()
    }
    if right {
        result = result + right.total()
    }
    return result
}
```
```spite title=tree_basics/tree_basics.spite entry
var console = Console()

func TreeBasics() {
    var small_branch = Branch(Leaf(1), Leaf(2))
    var tree: TreeNode = Branch(small_branch, Leaf(3))
    var total = tree.total()
    console.print("total", total)
}
```
```output
total 6
```

## Cycles leak

Reference counting cannot free a cycle -- two objects (directly, or through several hops) holding a reference
to each other never reach a count of zero. This is a known, accepted tradeoff, not a bug: break the cycle by
hand when you are done with it (clear a `T?` field that closes the loop, ideally from `drop()`-time
logic on whichever side runs last) if it matters for a long-running program. **[planned]** A future opt-in weak
reference type is the intended real fix; not implemented yet.

## `--debug-memory`

`spite program.spite --debug-memory` builds with an allocation counter and prints `allocations: N frees: N`
right before the program exits. A mismatch means something leaked or double-freed; when it does not balance,
it also prints a **leaked-object summary by class name**, naming which classes' instances are still live --
exactly what makes a leaked cycle visible instead of an unexplained non-zero count.

**Known limitation:** a `person.age`-shaped read answered by a getter (not a raw field) is not always released
when used directly as a call/print argument rather than stored -- see [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
