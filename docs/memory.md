# Memory

Spite uses **reference counting**, JavaScript-like. A scalar (a number, a `Boolean`, an enum value) is a plain
value, copied wherever it goes. Everything else -- a class instance, `List<T>`, `Dictionary<T>`, `String`, a
union, an object literal -- is a **reference**: assigning it, passing it, storing it in a field, a list or a
dictionary, and returning it all share the exact same object. There is no reference syntax to write: a reference
is the default. When the last reference to an object goes -- a scope ends, a field is overwritten, an element is
removed -- the object's `drop()` runs, if it has one, its own references are released, and it is freed. There is
no garbage collector and no pause.

What that costs while the program runs is a count in every object and one addition or subtraction each time a
reference is kept or let go -- plain arithmetic, atomic only in a program that starts a thread -- and nothing
else: no collector, no runtime to ship ([D177](decisions.md)). A singleton is not counted at all
([D142](decisions.md)), and a number, a `Boolean` or an enum value is never an object.

## Do: know that sharing is visible

Two names can hold the same object. A mutation through either one shows up through the other, because there is
only ever one object:

```gdscript title=sharing_basics/box.spite
var label = "unnamed"

func Box(starting_label: String) {
    label = starting_label
}
```
```gdscript title=sharing_basics/sharing_basics.spite entry
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

```gdscript title=copy_basics/box.spite
var label = "unnamed"

func Box(starting_label: String) {
    label = starting_label
}
```
```gdscript title=copy_basics/copy_basics.spite entry
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

`deep_copy()` does not follow a cycle safely: see [the rules](#memory--implemented).

## `drop()` runs once, right before the object is freed

A class may define a zero-argument `func drop() { ... }` for cleanup (closing a handle, clearing a
back-reference). The compiler calls it automatically the moment the last reference goes away -- never by name:

```gdscript title=drop_basics/resource.spite
var console = Console()
var name = "unnamed"

func Resource(new_name: String) {
    name = new_name
}

func drop() {
    console.print("dropped", name)
}
```
```gdscript title=drop_basics/drop_basics.spite entry
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

```gdscript title=tree_basics/tree_node.spite
union TreeNode {
    Leaf
    Branch
}
```
```gdscript title=tree_basics/leaf.spite
var held_value = 0

func Leaf(starting_value: Integer) {
    held_value = starting_value
}

func total(): Integer {
    return held_value
}
```
```gdscript title=tree_basics/branch.spite
var left: TreeNode? = null
var right: TreeNode? = null

func Branch(left_node: TreeNode, right_node: TreeNode) {
    left = left_node
    right = right_node
}

func total(): Integer {
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
```gdscript title=tree_basics/tree_basics.spite entry
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

Two objects that hold each other, directly or through several hops, keep each other's count above zero, so
neither is ever freed. In a long-running program, clear the `T?` field that closes the loop when you are done with
it. `--debug-memory`, below, is how you find one; the rule, and the weak reference planned to fix it, are in
[the rules](#memory--implemented).

## `--debug-memory`

`spite program --debug-memory` builds with an allocation table and prints `allocations: N frees: N` right before
the program exits. A mismatch means something leaked; when the two do not balance, it also prints a
**leaked-object summary by class name**, naming which classes' instances are still alive -- which is what makes a
leaked cycle visible instead of an unexplained count. Every program on these pages is run this way, and must
balance. The table exists only in a `--debug-memory` build ([what it records](#--debug-memory-1)).

## `Memory` is the floor, and you can build on it

Memory is its own namespace, because it is the most basic thing a program has and the most dangerous
([D151](decisions.md)). It holds two kinds of class:

- **`Memory.Address` is a place in memory** (`library/memory/address.spite`). It is a number, eight bytes, kept
  in a register like a `Long` and cast to and from one by the ordinary casting rule, so `address + 16` is the
  address sixteen bytes on. What makes it an address is what it answers: `read_long(offset)`,
  `write_float(offset, value)` and the rest read and write the value at `address + offset`.
- **An allocator is who owns memory**: `Memory.Heap()` is the one every object uses unless told otherwise
  ([D152](decisions.md)). It hands out addresses and takes them back; `Memory.Arena` is another
  ([below](#choosing-an-allocator-memoryallocator)).

Every type in the standard library is Spite over these two: `String`, `List<T>` and `Dictionary<T>` keep their
bytes in memory the heap hands out, and each number says in its own file how much memory it is. A container of
your own is written over the heap and `TypedMemory<T>` (below), with nothing the compiler does for `List<T>`
that it would not do for yours -- read `library/list.spite` for a complete one. What is built of this floor and
what is not yet is in [the rules](#the-floor-memoryaddress-memoryheap-and-typedmemoryt--implemented-os-pages-planned).

### Where `String` and `Integer` keep their memory

A type's storage is attributes at the top of its file (D108). `library/string.spite` starts with the memory a
`String` is:

```gdscript
var _bytes: Memory.Address = 0
var _length: Long = 0
```

Sixteen bytes, kept wherever the `String` is -- in a local, an attribute, a list's element -- and never an object
of their own. `_bytes` is where its characters are, with a 0 after the last one for C, and `_length` is how many
there are. Where the characters live is the compiler's choice ([D203](decisions.md)): text of up to 15 bytes is
kept in those sixteen bytes themselves, so it allocates nothing and is never counted; longer text is one block the
heap hands out, with its reference count and capacity in front of the characters; and a written text (`"hello"`)
points at the characters the program already carries, which are never counted or freed. Everything else is Spite
in the same file: `length()` answers `_length`, `equals` and `less_than` compare `_bytes` with `compare_bytes`, and
`slice` ends in `_bytes.text(length)`. What stays C is what depends on where the characters are: reading one
(`code_at`, which answers the 0 after the last one past the end, and is one load in a loop over the text), making
text from bytes (`Memory.Address.text`), joining two texts (`sum`), counting a block's references, and growing text
in place, which fills the sixteen bytes first and moves the text into a block once it passes 15 bytes -- and only
when nothing else holds that block.

`library/integer.spite` starts with the memory an `Integer` is:

```gdscript
var heap = Memory.Heap()
var _memory = heap.allocate(4)
```

Four bytes, allocated like everything else, and placed by the compiler: a number's memory is a register (or
wherever the C compiler keeps an `int32_t`), so there is no address behind `this` and nothing to free. The heap
is bound to `heap` first, as every singleton is ([classes_and_files.md](classes_and_files.md#singletons)); that
binding is never a field of the number. Every number file says the same with its own width (`Long` 8, `Short` 2,
`Byte` 1, `Boolean` 1, `Double` 8, `Memory.Address` 8, ...), the compiler checks it against the C type it emits,
and `count.memory.bytes` reads it. Inside a number, `this` is the value itself, not `_memory`.

What a container of your own works with:

- **Allocating:** `heap.allocate(bytes)` returns a `Memory.Address`, `heap.resize(address, bytes)` grows it and
  `heap.free(address)` gives it back, from your `drop()`. The bytes are not cleared, so write before you read.
- **Where it lives is the compiler's choice** ([placement](#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed)):
  a buffer a function allocates, uses and frees itself lands in the function's own frame, with no allocation at
  all; anything else is on the heap. You write the same `allocate` and `free` either way.
- **Reading and writing an address:** `address.read_long(offset)`, `address.write_float(offset, value)` and the
  other widths are primitives of the language, one machine operation each, and **only `library/` may call them**.
  `copy_to`, `compare_bytes`, `text(length)` and `terminated_text()` are open to every program.
- **Values of any type:** a generic class holds a `TypedMemory<$value_type>`, whose `read_value`, `write_value`
  and `release_value` store a value of that type at an index and keep its reference count right. It is exactly
  what `library/list.spite` uses for its elements, and it is how a program's own class reaches memory.

A ring buffer that keeps the last few values it was given, whose slots are on the heap, and a sum whose
numbers the compiler places in the function's frame, so the live allocations do not move while it runs:

```gdscript title=ring_buffer_program/ring_buffer.spite
generic $value_type

var heap = Memory.Heap()
var values = TypedMemory<$value_type>()
var slots: Memory.Address = 0
var capacity = 0
var start = 0
var used = 0

func RingBuffer(starting_capacity: Integer) {
    capacity = starting_capacity
    var value_bytes = values.value_bytes()
    slots = heap.allocate(value_bytes * capacity)
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

func count(): Integer {
    return used
}

func drop() {
    var index = 0
    while index < used {
        values.release_value(slots, (start + index) % capacity)
        index = index + 1
    }
    heap.free(slots)
}
```
```gdscript title=ring_buffer_program/ring_buffer_program.spite entry
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
    var total = sum_in_the_frame(4)
    console.print(total)
}

func sum_in_the_frame(count: Integer): Long {
    var heap = Memory.Heap()
    var numbers = TypedMemory<Long>()
    var before = heap.live_allocations()
    var slots = heap.allocate(count * 8)
    var index = 0
    while index < count {
        numbers.write_value(slots, index, index * 10)
        index = index + 1
    }
    var total: Long = 0
    index = 0
    while index < count {
        total = total + numbers.read_value(slots, index)
        index = index + 1
    }
    var during = heap.live_allocations()
    heap.free(slots)
    console.print("allocated while summing:", during - before)
    return total
}
```
```output
grace 3
allocated while summing: 0
60
```

### Where a value lives: `.memory`

Every named value can see its own memory through reflection: `value.memory` is a `Spite.Memory`
(`library/spite/memory.spite`) with a read-only `address`, `bytes` and `section` -- `'heap'`, `'stack'` or
`'constant'` (the text of a literal, which is part of the program). A class instance or a list answers with
its object; a `String` with its characters; a number held in a local with the local itself. It is built only
where a program reads it, so it costs nothing anywhere else ([the rule](#where-a-value-lives-memory-1)).

```gdscript title=memory_sections/memory_sections.spite entry
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

### Choosing an allocator: `.memory.allocator`

Every object is made on `Memory.Heap` unless its program says otherwise, and it says so on the line right after
the object is made ([D152](decisions.md)):

```gdscript
var spark = Particle("spark", 1.5)
spark.memory.allocator = arena
```

The compiler reads the two lines as one intent: `spark` is made in `arena` from the start. Nothing is allocated
on the heap first and moved, and nothing is looked up while the program runs -- the constructor is handed the
arena's memory instead of the heap's. The standard library's allocator besides the heap is
**`Memory.Arena(block_bytes)`**, which hands out memory from blocks of that size, one after another, never gives
any of it back one piece at a time, and frees every block at once when the arena itself goes. An object made in
an arena holds the arena, so the arena cannot go while anything made in it is alive.

- **Only right after it is made.** A constructor, `List<T>()` or `.copy()` on one line, and the allocator on the
  next. To move an object that was already used, copy it and set the copy's allocator, as `kept` does below.
- **Only an object.** A number or a `String` is placed by the compiler, not by an allocator.
- **A list holds references** ([D154](decisions.md)): giving a `List` an allocator places the list itself there,
  and each element lives wherever it was made.
- **It costs nothing where it is not used** ([D177](decisions.md)): only a class some line gives an allocator
  grows, by sixteen bytes per object.

The exact rules, the errors and what is not built yet are in
[the rules](#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-vectort-planned).

```gdscript title=arena_particles/particle.spite
var name = ""
var life = 0.0

func Particle(new_name: String, new_life: Float) {
    name = new_name
    life = new_life
}
```
```gdscript title=arena_particles/arena_particles.spite entry
var console = Console()
var heap = Memory.Heap()

func ArenaParticles() {
    var arena = Memory.Arena(4096)
    var before = heap.live_allocations()
    var spark = Particle("spark", 1.5)
    spark.memory.allocator = arena
    var ember = Particle("ember", 2.0)
    ember.memory.allocator = arena
    var during = heap.live_allocations()
    console.print(spark.name, ember.name, "heap allocations:", during - before)
    var ash = Particle("ash", 0.5)
    var kept = ash.copy()
    kept.memory.allocator = arena
    kept.life = 3.0
    console.print(ash.life, kept.life)
}
```
```output
spark ember heap allocations: 1
0.5 3
```

The one heap allocation is the arena's first block; both particles are inside it.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Memory  **[implemented]**

D1 (decided by Mortaro): **reference counting is the memory model**, JavaScript-like. Every non-scalar value (a
class instance, `List<T>`, `Dictionary<T>`, `String`, a union of classes, an object literal) is a reference:
passing it, assigning it, storing it in a field/list/dictionary, and returning it all share the exact same object.
Scalars (every numeric type, `Boolean`, an enum value) are plain values, copied. There is no reference syntax
(no `&`): a parameter of a class type is written `Type`. A copy is always explicit: `copy()`/`deep_copy()` (below).

- **Retain and release.** Every reference is counted. Binding one to a `var`/parameter, or storing it into a
  field/list element/dictionary value, retains it (bumps the count); a scope ending, a value being overwritten, or
  an element being removed from a container releases it (drops the count). When a release brings the count to
  zero: the class's own `drop()` function runs first, if it declared one, then every attribute that itself needs
  releasing is released, then the object is freed. A singleton is never counted: fetching and letting go of one
  does nothing, and it is destroyed at exit ([D142](decisions.md),
  [classes_and_files.md](classes_and_files.md#singletons)).
- **Run-time cost (D177).** A count in every object's header and one addition or subtraction per retain and
  release. The counts are atomic only in a program that can share an object between threads (one that makes a
  `Concurrent` or a `Parallel`, runs a `parallel_each_` pass, or is built with `--repl-port` or `--hot-reload`);
  every other program counts with plain arithmetic
  ([optimizations.md](optimizations.md#atomic-reference-counts-only-with-threads)).
- **`drop()`.** A class may define `func drop() { ... }` to run cleanup the moment its last reference goes (closing
  a file handle, logging, clearing a back-reference to help break a cycle by hand -- see below). It takes no
  parameters and returns nothing; the compiler calls it automatically, never by name.
- **Identity vs equality.** `==` on two class instances calls `equals` if the class defines one ([Operators](functions_and_operators.md#operators--implemented));
  otherwise it compares **identity** -- are these two references the same object.
  `String` always compares by content, never by identity (sharing a `String`'s buffer is unobservable, since it is
  immutable, and so is its absence: text of up to 15 bytes has no buffer to share, each holder keeping it in its
  own sixteen bytes, [D203](decisions.md)).
- **`copy()`/`deep_copy()`** (names **proposed by Claude, unconfirmed**). Every non-scalar value has both:
  `copy()` is shallow -- a fresh object, its own attributes/elements the exact same references the source had
  (retained, not duplicated; a `String` field needs no special handling either way, since it is immutable).
  `deep_copy()` recurses: every reference-kind attribute/element gets its own `deep_copy()`/independent buffer
  instead of being shared. **Cycles are not supported** by `deep_copy()` -- a self-referential (or mutually
  referential) structure recurses forever; break the cycle by hand first if you need to deep-copy one.
- **Cycles leak.** Reference counting cannot free a cycle (two objects holding a reference to each other, directly
  or through several hops): neither one's count ever reaches zero. This is a known, accepted tradeoff, not a bug --
  break a cycle by hand when you are done with it (set the back-reference to `null` inside `drop()`-time logic, or
  clear a `T?` field that closes the loop) if it matters for a long-running program. **[planned]** A future
  opt-in type (e.g. a weak reference) is the intended real fix; not implemented yet.
- Parameters follow the same rule as everything else in [Functions](functions_and_operators.md#functions--implemented): a scalar is passed by value (copied); anything
  else is passed by reference (the same object, retained for the callee's own binding and released when the
  callee's scope ends); nothing is written at the call site.

Every function/method return retains its result, so a getter's returned value is always a fresh, independent
reference -- including through attribute read interception ([Operators](functions_and_operators.md#operators--implemented)): a `person.age`-shaped read answered by a
getter, used directly as a call/print argument rather than stored, is released like any other temporary
(`ExpressionResult.is_owning` in the generator is how a read that compiles to a call, but still
parses as a `.member`/`.index` expression -- an intercepted attribute read, `arguments.some_key`, the
`attributes[attribute]` template form -- tells every caller whether it is independently owned regardless of what
the plain AST shape alone would suggest).

#### `--debug-memory`

`--debug-memory` reports total allocations and frees right before the program exits (`allocations: N frees: N`),
and they must balance exactly -- not just be bounded -- for every example, every documentation program,
`tests/references` and the compiler's own runs. When they do not, it also prints a **leaked-object summary by class
name**, naming which classes' instances are still live. The table keeps each live allocation's class beside it and
forgets it on free, so the summary names exactly the objects still alive, the same ones on every run; memory that
is not an object (a list's elements, a string's bytes) is counted but not named. An object a program leaked is
named even when it points at a singleton, which is still destroyed at exit (D142). The compiler's own bookkeeping
-- the list behind `.instances`, the list of singletons to destroy -- is allocated outside the table and not counted
(D143).

**Run-time cost (D177).** The table and the summary exist only in a `--debug-memory` build. Every other build
counts live allocations with one addition per allocation and one subtraction per free, which is what
`Memory.Heap().live_allocations()` answers.

#### Where a value lives: `.memory`

Every named value has a read-only `.memory`, a `Spite.Memory` (`library/spite/memory.spite`): `address`, `bytes`,
and `section`, one of `'heap'`, `'stack'` or `'constant'`. A class instance or a list answers with its object, a
`String` with its characters (`'constant'` for a literal, whose characters are part of the program; for text made
while the program runs, `'heap'` when it is longer than 15 bytes and lives in a block, and otherwise wherever the
value itself is, since the characters are in it: `'stack'` in a local, `'heap'` in an attribute, [D203](decisions.md)),
and a number held in a local with the local itself (`'stack'`). A class with an attribute of its own named `memory` answers that
attribute instead. `.memory` is built only where a program reads it, so it costs nothing anywhere else (D152); the
same object is where an allocator is set ([below](#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-vectort-planned)).

#### The floor: `Memory.Address`, `Memory.Heap` and `TypedMemory<T>`  **[implemented; OS pages planned]**

`Memory` is a namespace, not an object (D151): there is no `Memory()` to make.

- **`Memory.Address`** (`library/memory/address.spite`) is a number class of eight bytes, the place in memory. It
  casts to and from `Long` like every number, so `address + 16` is the address sixteen bytes on, and a foreign
  function's `Long` result is an address by assignment. `copy_to(target, bytes)`, `compare_bytes(other, bytes)`,
  `text(length)` and `terminated_text()` are open to every program.
- **Reading and writing** (D178): `read_byte`, `read_short`, `read_unsigned_short`, `read_integer`,
  `read_unsigned_integer`, `read_long`, `read_float` and `read_double`, each `(offset)`, the matching
  `write_*(offset, value)`, and `exchange_long`, `read_long_atomically` and `write_long_atomically` for memory
  threads share. Each is a primitive of the language, like `+`: the compiler writes it where it is called as one
  load, store or atomic instruction, with no call and no check, so reading a field costs what it costs in C
  ([optimizations.md](optimizations.md#reading-an-address-is-one-machine-operation)). **Only `library/` may call
  them**: a function of a class the standard library declares, including one a program reopens (so
  `--final-classes` output, which reopens every class it prints, still compiles). Anywhere else:
  `'read_long' reads or writes the memory at an address, which only library/ does: use a class of the standard library, or 'TypedMemory<T>' for values of any type`
  (`diagnostics/memory_access`). On a `Long` that is not an address: `a Long has no function 'read_integer':
  reading and writing memory is a function of the address, so the value needs to be a 'Memory.Address'`.
- **`Memory.Heap()`** is the default allocator, a singleton with `allocate(bytes: Long): Memory.Address`,
  `resize(address, bytes): Memory.Address`, `free(address)` and `live_allocations(): Integer`. Nothing frees an
  allocation for you: a class that allocates frees in its `drop()`. The bytes are not cleared: a new block, and
  the part `resize` adds, hold whatever was there before, so a byte must be written before it is read -- a read of
  a byte nobody wrote can pass on one machine and crash on the next, since what the heap holds depends even on how
  the program was launched. Where an allocation lives is the compiler's choice ([placement](#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed)).
  The heap holds nothing, so in a production build it is one static object and costs no allocation
  ([optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **`TypedMemory<$value_type>`** stores a value of any type -- a class, a `String`, a number or a nullable -- at an
  index of an address: `read_value(address, index)`, `write_value(address, index, value)`,
  `release_value(address, index)` and `value_bytes()`, keeping reference counts right for that type. It is one
  shared instance per type (a singleton that holds nothing), and it is what `library/list.spite` uses for its
  elements. Its three value functions use the address the way a read does, so a frame-placed allocation may be
  handed to them.
- **A number's storage** is two ordinary lines at the top of its file, `var heap = Memory.Heap()` and
  `var _memory = heap.allocate(4)` (D145's two lines, in D178's names; the `heap` binding is never a field of the
  number, D110). The size must be the C type's: `'Integer' declares 8 bytes of memory, and the compiler lays it out as
  int32_t, which is 4` (`diagnostics/number_memory`). Inside a number `this` is the value, and reading `_memory` is
  `'_memory' is the memory Integer is kept in, and the compiler keeps the value itself there: write 'this'`
  (`diagnostics/number_memory_read`). `count.memory.bytes` reads the size.

**Not built** (D178): the heap asking the operating system for pages itself, and `copy_to` and `compare_bytes` as
plain Spite through `DynamicLibrary`. Today `Memory.Heap`'s four functions are bodies the compiler supplies over
the C library's `realloc` and `free` (with the live-allocation counter), and `copy_to` and `compare_bytes` are
written in place as `memmove` and `memcmp`; `library/memory/heap.spite` declares only `singleton`, so D147's "zero
hidden code" is not reached for the heap yet.

#### Placement: the compiler decides where memory lives  **[implemented; the rule proposed by Claude, unconfirmed]**

D108 (decided by Mortaro): "Memory should be a abstraction that lets us allocate heap/stack/register but our
compiler decides best use placement." A program has one way to ask for raw memory, `Memory.Heap`'s `allocate`
(D151), and one way to give it back, `free`; where the bytes live is the compiler's choice, and the program's text
is the same whichever it makes:

- **Register:** a number's own memory, declared in its file as `var heap = Memory.Heap()` and then
  `var _memory = heap.allocate(4)` (for `Integer`; the `heap` binding is never a field, D110). The wrapper is flattened: a number is its C scalar, and `this` is the value.
- **Frame:** `var name = heap.allocate(bytes)` in a function, when a later statement of the same block
  is `heap.free(name)` and every other use of `name` reads or writes through it (`name.read_long(offset)`, also
  as `(name + offset)`), copies or compares with it (`copy_to`, `compare_bytes`), turns it into `text`, or hands
  it to a `TypedMemory`'s `read_value`, `write_value` or `release_value` -- it is never stored, returned,
  assigned, resized or passed to anything else, and neither `name` nor `heap` is declared or assigned again
  after it. The compiler gives it a slot of 256 bytes
  in the function's frame (exactly the size, for a literal size up to 256), uses the heap when a run-time size
  is larger, and makes the `free` a no-op for the slot. A loop body is a block like any other, so the slot is
  reused on every pass. So `Double.bits()` allocates nothing, and `Long.to_string()` and `upper_case()` of a
  short text allocate only the `String` they return -- which is nothing when it is 15 bytes or fewer.
- **In the value:** text of up to 15 bytes is kept in the sixteen bytes of the `String` itself ([D203](decisions.md),
  [optimizations.md](optimizations.md#short-text-lives-inside-the-string)).
- **Constant:** a `String` literal's characters are part of the program (its `.memory.section` is `'constant'`).
- **Heap:** everything else.

There is no way to ask for the stack by name (D98's `allocate_stack_bytes` is gone): it would be a second way to
allocate, and one a program could get wrong by keeping an address past the return. What makes a data structure's
layout efficient stays its author's -- one allocation holding many values at offsets they choose,
`TypedMemory<$value_type>` for values of any type, `resize` to grow. Placement runs entirely while compiling and
costs nothing at run time (D177); a frame slot is cheaper than the heap call it replaces.

#### Allocators: `.memory.allocator`  **[implemented for objects; a list's buffer and `Vector<T>` planned]**

D150-D154 (decided by Mortaro): types never name an allocator; an **object** does, through its tree-shakeable
`.memory`, on the line right after it is made -- `var scratch = List<Integer>()` then `scratch.memory.allocator =
arena` -- and the compiler reads the two lines as where the object was always meant to live (D152), so it is made
there from the start. Setting it after the object was used is a compile error naming `copy()` (D153). What is
built (2026-09-25; the readings marked are proposed by Claude, unconfirmed):

- **An allocator is a class that answers `allocate(bytes: Long): Memory.Address` and `free(address:
  Memory.Address)`** (the D150 shape; no `type` is declared for it, per D151). `Memory.Heap` is the default, and
  setting it changes nothing. `Memory.Arena(block_bytes)` (`library/memory/arena.spite`, Spite over the heap)
  hands out 16-byte-aligned memory from blocks of that size, chaining a new block when one is full, never frees
  one piece at a time, and frees every block when the arena itself is dropped (proposed by Claude, unconfirmed:
  no `reset()` yet, because resetting under live objects is the undecided safety rule of
  `mortaros_allocators_proposal.md` question 3).
- **Where it may be set** (proposed by Claude, unconfirmed): only as the statement directly after `var name =
  ...` whose value is a constructor call, `List<T>()`/`Dictionary<T>()` or `.copy()` of a class whose copy the
  compiler writes, and only to a name or a path of names (so evaluating it earlier than written changes nothing).
  Anything else is an error (`diagnostics/allocator_after_use`):
  - after the object was used (D153): `'spark' was already used, so its allocator can no longer change: an
    object's allocator is set on the line right after it is made. To move it, copy it and set the copy's: 'var
    moved = spark.copy()' and then 'moved.memory.allocator = ...'`;
  - on a value no constructor made, such as a reflection object (`var place = text.memory`): `'place' gets an
    allocator only on the line right after the line that makes it: a constructor, 'List<T>()' or '.copy()'`;
  - on a number or a `String`: `only an object gets an allocator of its own, and 'count' is a value of type
    Integer, which the compiler places itself`;
  - to a class that is not an allocator: `'Console' is not an allocator: an allocator answers 'allocate(bytes:
    Long): Memory.Address' and 'free(address: Memory.Address)', like 'Memory.Arena'`.
- **How it is compiled**: the construction becomes `C___make_in(allocator, give_back, address, arguments...)` with
  the address from the allocator's `allocate`, so nothing is allocated twice or moved. An object made this way
  holds a reference to its allocator and the allocator's `free`, and gives its memory back through them when its
  count reaches zero; the allocator therefore outlives everything made in it. **Tree-shaken (D177)**: only a class
  some line gives an allocator carries the two hidden pointers (16 bytes per object of that class), and nothing
  else in the program changes; there is no run-time registry and no current-allocator state.
- **Lists** (D154): a `List` given an allocator is made there itself; each element lives wherever it was made,
  since a list holds references.
- **Not built:** D154's list buffer following its list's allocator (a list placed in an arena keeps its buffer of
  references on the heap), `Vector<T>` with its items inline, `Memory.Frame` (`mortaros_missing_decisions.md`
  item 173), `reset()` on an arena, and reading `.memory.allocator` back.
