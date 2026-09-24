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

## `Memory` is the floor, and you can build on it

Every type in the standard library is Spite over one class, `Memory` (`library/memory.spite`): `String`,
`List<T>` and `Dictionary<T>` keep their bytes in memory it hands out, and the numbers are values the compiler
lays out (D98, D101). A container of your own is written the same way, with nothing the compiler does for
`List<T>` that it would not do for yours.

- **Heap:** `memory.allocate_bytes(bytes)` returns an address (a `Long`); `resize` grows it and `free` gives it
  back. Nothing frees it for you: a class that allocates frees in its `drop()`.
- **Stack:** `memory.allocate_stack_bytes(bytes)` returns an address in the calling function's own frame. It
  costs nothing to free, because it is gone when that function returns -- so never keep the address, and do
  not ask for it inside a loop that runs many times, since each call takes more of the frame.
- **Typed values:** `read_byte`/`read_int`/`read_long`/`read_double` and the matching `write_*` read and write
  numbers at an address. For values of any type -- a class, a `String`, a nullable number -- a generic class
  holds a `TypedMemory<$value_type>`, whose `read_value(address, index)`, `write_value(address, index, value)`,
  `release_value(address, index)` and `value_bytes()` keep reference counts right for that type. It is one
  shared instance per type, and it is exactly what `library/list.spite` uses for its elements.

A ring buffer that keeps the last few values it was given, over the heap, and a sum over the stack:

```spite title=ring_buffer_program/ring_buffer.spite
var memory = Memory()
var values = TypedMemory<$value_type>()
var slots: Long = 0
var capacity = 0
var start = 0
var used = 0

func RingBuffer<$value_type>(starting_capacity: Int) {
    capacity = starting_capacity
    var value_bytes = values.value_bytes()
    slots = memory.allocate_bytes(value_bytes * capacity)
}

func push(value: $value_type) {
    if used == capacity {
        values.release_value(slots, start)
        values.write_value(slots, start, value)
        start = (start + 1) % capacity
        return
    }
    values.write_value(slots, (start + used) % capacity, value)
    used = used + 1
}

func oldest(): $value_type {
    return values.read_value(slots, start)
}

func count(): Int {
    return used
}

func drop() {
    var index = 0
    while index < used {
        values.release_value(slots, (start + index) % capacity)
        index = index + 1
    }
    memory.free(slots)
}
```
```spite title=ring_buffer_program/ring_buffer_program.spite entry
var console = Console()

func RingBufferProgram() {
    var names = RingBuffer<String>(3)
    names.push("ada")
    names.push("grace")
    names.push("linus")
    names.push("barbara")
    var oldest = names.oldest()
    var names_count = names.count()
    console.print(oldest, names_count)
    var total = sum_on_the_stack(4)
    console.print(total)
}

func sum_on_the_stack(count: Int): Long {
    var memory = Memory()
    var numbers = memory.allocate_stack_bytes(count * 8)
    var index = 0
    while index < count {
        memory.write_long(numbers, index * 8, index * 10)
        index = index + 1
    }
    var total: Long = 0
    index = 0
    while index < count {
        total = total + memory.read_long(numbers, index * 8)
        index = index + 1
    }
    return total
}
```
```output
grace 3
60
```

### Where a value lives: `.memory`

Every named value can see its own memory through reflection: `value.memory` is a `Spite.Memory`
(`library/spite/memory.spite`) with a read-only `address`, `bytes` and `section` -- `'heap'`, `'stack'` or
`'constant'` (the text of a literal, which is part of the program). A class instance or a list answers with
its object; a `String` with its characters; a number held in a local with the local itself. It is built only
where a program reads it, so it costs nothing anywhere else. A class with an attribute of its own named
`memory` (most of the standard library holds one) answers that attribute instead.

```spite title=memory_sections/memory_sections.spite entry
var console = Console()

func MemorySections() {
    var count = 3
    var count_memory = count.memory
    console.print(count_memory.section, count_memory.bytes)
    var literal = "fixed"
    var literal_memory = literal.memory
    var has_address = literal_memory.address != 0
    console.print(literal_memory.section, literal_memory.bytes, has_address)
}
```
```output
stack 4
constant 5 true
```
