# Memory

Spite uses **reference counting**, JavaScript-like. A scalar (a number, a `Bool`, an enum value) is a plain
value, copied wherever it goes. Everything else -- a class instance, `List<T>`, `Dictionary<T>`, `String`, a
union, an object literal -- is a **reference**: assigning it, passing it, storing it in a field, a list or a
dictionary, and returning it all share the exact same object. There is no reference syntax to write: a reference
is the default. When the last reference to an object goes -- a scope ends, a field is overwritten, an element is
removed -- the object's `drop()` runs, if it has one, its own references are released, and it is freed. There is
no garbage collector and no pause.

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

`==` on two class instances calls the class's `equals` when it has one
([functions_and_operators.md](functions_and_operators.md#every-operator-is-a-function)); otherwise it asks
whether they are the same object. A `String` always compares by content.

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

A tree, a linked list, or any other recursive structure needs nothing beyond an ordinary field, since a field of
a class type already holds a reference:

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

## `--debug_memory`

`spite program --debug_memory` builds with an allocation table and prints `allocations: N frees: N` right before
the program exits. A mismatch means something leaked; when the two do not balance, it also prints a
**leaked-object summary by class name**, naming which classes' instances are still alive -- which is what makes a
leaked cycle visible instead of an unexplained count. Every program on these pages is run this way, and must
balance.

## `Memory` is the floor, and you can build on it

Every type in the standard library is Spite over one class, `Memory` (`library/memory.spite`): `String`,
`List<T>` and `Dictionary<T>` keep their bytes in memory it hands out, and each number says in its own file how
much memory it is. A container of your own is written the same way, with nothing the compiler does for `List<T>`
that it would not do for yours -- read `library/list.spite` for a complete one.

### Where `String` and `Int` keep their memory

A type's storage is attributes at the top of its file (D108). `library/string.spite` starts with the memory a
`String` holds:

```spite
var _bytes: Long = 0
var _length: Long = 0
var _section: Spite.Memory.Section = 'heap'
var _capacity: Long = 0
```

`_bytes` is the address of its characters, which `Memory` handed out (with a 0 after the last one, for C);
`_length` is how many there are; `_capacity` is how many fit before the bytes have to grow, which only building
text in a loop uses; and `_section` is where the compiler placed them: `'heap'`, or `'constant'` for a literal,
whose characters are part of the program and are never counted or freed. The compiler writes the C layout of a
`String` from these declarations, in this order, after the header every object has (its reference count and its
class). Everything else is Spite in the same file: `length()` answers `_length`, `code_at(index)` reads a byte
of `_bytes` through `Memory` (the 0 after the last one is what it answers past the end), `slice` and `+` copy
through `Memory` and end in the constructor `String(bytes, length)`, which takes bytes `Memory` handed out, and
`drop()` gives `_bytes` back to `Memory` when the last reference goes. The only C about a `String` is what the
header decides: a `'constant'` text is never counted, and `text = text + piece` grows in place only when
nothing else holds the text.

`library/int.spite` starts with the memory an `Int` is:

```spite
var _memory = Memory().allocate_bytes(4)
```

Four bytes, allocated through `Memory` like everything else, and placed by the compiler: a number's memory is a
register (or wherever the C compiler keeps an `int32_t`), so there is no address behind `this` and nothing to
free. Every number file says the same with its own width (`Long` 8, `Short` 2, `Byte` 1, `Bool` 1, `Double` 8,
...), the compiler checks it against the C type it emits, and `count.memory.bytes` reads it. Inside a number,
`this` is the value itself: reading `_memory` is an error saying so.

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
generic $value_type

var memory = Memory()
var values = TypedMemory<$value_type>()
var slots: Long = 0
var capacity = 0
var start = 0
var used = 0

func RingBuffer(starting_capacity: Int) {
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
