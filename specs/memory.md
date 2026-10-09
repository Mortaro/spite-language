# Memory

The specification of [Memory](../docs/memory.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## The memory model

**Reference counting is the memory model**, JavaScript-like. Every non-scalar value (a
class instance, `List<T>`, `Dictionary<Key, Value>`, `String`, a union of classes, an object literal) is a reference:
passing it, assigning it, storing it in a field/list/dictionary, and returning it all share the exact same object.
Scalars (every numeric type, `Boolean`, an enum value) are plain values, copied. There is no reference syntax
(no `&`): a parameter of a class type is written `Type`. A copy is always explicit: `copy()`/`deep_copy()` (below).

- **Retain and release.** Every reference is counted. Binding one to a `var`/parameter, or storing it into a
  field/list element/dictionary value, retains it (bumps the count); a scope ending, a value being overwritten, or
  an element being removed from a container releases it (drops the count). When a release brings the count to
  zero: the class's own `drop()` function runs first, if it declared one, then every attribute that itself needs
  releasing is released, then the object is freed. A singleton is never counted: fetching and letting go of one
  does nothing, and it is destroyed at exit ([classes_and_files.md](../docs/classes_and_files.md#singletons)).
- **Run-time cost.** A count in every object's header and one addition or subtraction per retain and
  release. The counts are atomic only in a program that can share an object between threads (one that makes a
  `Concurrent` or a `Parallel`, runs a `parallel_each_` pass, or is built with `--repl-port` or `--hot-reload`),
  and in a production build only for the classes whose objects code on another thread can retain or release;
  every other program, and every other class, counts with plain arithmetic
  ([optimizations.md](../docs/optimizations.md#atomic-reference-counts-only-with-threads),
  [plain counts](../docs/optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class)).
- **`drop()`.** A class may define `func drop() { ... }` to run cleanup the moment its last reference goes (closing
  a file handle, logging, clearing a back-reference to help break a cycle by hand, see below). It takes no
  parameters and returns nothing; the compiler calls it automatically, never by name.
- **Identity vs equality.** `==` on two class instances calls `equals` if the class defines one ([Operators](functions_and_operators.md#operators));
  otherwise it compares **identity**: are these two references the same object.
  A union compares identity the same way. A comparison releases whatever operand it produced itself (an item
  read with `[]`, a call's result), so `kept[index] == shape` holds nothing afterwards.
  `String` always compares by content, never by identity (sharing a `String`'s buffer is unobservable, since it is
  immutable, and so is its absence: text of up to 15 bytes has no buffer to share, each holder keeping it in its
  own sixteen bytes).
- **`copy()`/`deep_copy()`**. Every non-scalar value has both:
  `copy()` is shallow: a fresh object, its own attributes/elements the exact same references the source had
  (retained, not duplicated; a `String` field needs no special handling either way, since it is immutable).
  `deep_copy()` recurses: every reference-kind attribute/element gets its own `deep_copy()`/independent buffer
  instead of being shared. An attribute or element typed as a union or a `type` shape is copied through the class
  of the value it holds at run time, so a `Cargo` holding a `Crate` gets a new `Crate`; a `String`, a number held
  in one and a singleton are shared, since none of them can be changed through the copy. A union or a shape value
  answers `deep_copy()` itself (`var copied = cargo.deep_copy()`, a `Cargo`). A class that declares its own
  `deep_copy()` is copied through it wherever it is reached, which is how a `Vector` or an `Items` attribute gets a
  block of its own instead of sharing the original's. A self-referring structure (a tree, a linked list, an
  expression whose operands are expressions) is copied node by node (`conformance/stage6/deep_copy_unions`).
  **`deep_copy()` copies a graph with its shape**: a structure whose references lead back to an object already being
  copied (a parent holding its children, each holding its parent) gets one copy of each object, and the copied
  references point at the copies, so the copy has the same cycles as the original (and leaks as every cycle does,
  unless one side is a `Weak`). A `Weak` attribute's copy holds the copy of its object when the same `deep_copy()`
  copies that object, before or after reaching the `Weak`, and the original object otherwise, since a `Weak` does
  not own what it holds. The compiler writes each class's deep copy: one whose attributes can reach itself, reach a
  `Weak`, or are held by a `Weak` remembers the objects copied during the call in a table made for that call; every
  other class is copied plainly, with no table (`conformance/stage6/deep_copy_graphs`). An object reached twice
  through classes that cannot reach themselves (two attributes holding the same leaf) is copied twice.
- **Cycles leak.** Reference counting cannot free a cycle (two objects holding a reference to each other, directly
  or through several hops): neither one's count ever reaches zero. This is a known, accepted tradeoff, not a bug.
  Break a cycle by hand when you are done with it (set the back-reference to `null` inside `drop()`-time logic, or
  clear a `T?` field that closes the loop) if it matters for a long-running program, or hold one side with
  `Weak<T>`.
- **`Weak<T>` holds an object without counting it** (a generic library class, not a
  keyword; `library/weak.spite`). `Weak(object)` makes one, `Weak<T>(null)` an empty one, and `get(): T?` answers
  the object while something else keeps it alive and `null` once it has been freed. The first `Weak` of an object
  makes a small box holding its address, found through a table keyed
  by address; a class some `Weak` holds tells the table when an instance is freed, so its box answers `null`; the
  box goes when the last `Weak` of it does. A program that never makes a `Weak` carries none of this: the table,
  the boxes and the one check in the freeing of each weakly held class are written only for the classes a `Weak`
  holds, and `T` must be a class (`Weak<Integer>` is an error, since a number is kept by value)
  (`conformance/stage6/weak_parent`). **A `Weak` stays on the program's own thread**: the table has no lock, so a `Parallel(work)` whose work reaches a class some `Weak` holds, or a class with a
  `Weak` attribute, is a compile error at the `Parallel`: `'Parallel(tree.grow)' runs on another thread, and what it
  runs reaches 'Branch', which is a Weak or is held by one: a Weak is kept in one table that is not shared between
  threads, so a Weak and the objects it holds stay on the program's own thread. Hold the object itself in the
  work, or keep the Weak out of what the Parallel reaches` (`diagnostics/weak_across_threads`). What the work
  reaches is the same walk over calls that decides which singletons need a lock.
- Parameters follow the same rule as everything else in [Functions](functions_and_operators.md#functions): a scalar is passed by value (copied); anything
  else is passed by reference (the same object, retained for the callee's own binding and released when the
  callee's scope ends); nothing is written at the call site. A name the caller already holds for the whole call
  (its own parameter, or a local it owns) is passed without that count when the callee never assigns the
  parameter: nothing the call runs can let the caller's name go, so the
  count would change nothing a program can see ([optimizations.md](../docs/optimizations.md#an-argument-its-caller-holds-is-passed-without-counting)).

Every function/method return retains its result, so a getter's returned value is always a fresh, independent
reference, including through attribute read interception ([Operators](functions_and_operators.md#operators)): a `person.age`-shaped read answered by a
getter, used directly as a call/print argument rather than stored, is released like any other temporary
(`ExpressionResult.is_owning` in the generator is how a read that compiles to a call, but still
parses as a `.member`/`.index` expression (an intercepted attribute read, `arguments.some_key`, the
`attributes[attribute]` template form), tells every caller whether it is independently owned regardless of what
the plain AST shape alone would suggest).

### Assigning a name read from a list changes only the name

A local declared from an item of a `List` or a `Dictionary` names that item: `var layout = layouts[index]` and
`layout.width = 3` write the item the list holds. **Assigning the local names something else**: `layout = made`
changes `layout` and leaves the list holding the old item. When nothing reads the local after that assignment, the
assignment can only be a write meant for the list that never reaches it, so it is a compile error at the
assignment:

- **Read directly**: `'layout' was read from 'layouts[index]', so assigning it here changes only 'layout', and
  nothing reads 'layout' after: 'layouts' still holds the old item. Write 'layouts[index] = made' to replace the
  item, or give the new value a 'var' of its own` (`diagnostics/replaced_items`).
- **Answered by a function**: a call to a function of the program whose every `return` answers an item of a `List`
  or `Dictionary` attribute (or `null`), directly or through another such function, up to four calls deep. The
  error names the item and its class, and the function of that class that writes the list when it has one:
  `'layout' holds 'values[row]' of 'Column', answered by 'lookup.of(entity)', so assigning it here changes only
  'layout', and nothing reads 'layout' after: 'Column' still holds the old item. Replace it through
  'Column.write_at', which writes 'values', or give the new value a 'var' of its own`; with no such function, `Replace it through a
  function of 'Column' that writes 'values[row]'`.

The rule is judged in the block that declares the local, and in the branches of an `if` or a `switch` that is the
last statement naming it: the assignment must be the last place the local is named, and no earlier statement may
assign it (an earlier assignment means the old value is no longer the item). An assignment inside a `while` loop
whose local is declared outside it may be read by the next pass and is left alone, as is a local declared from
anything that is not an item read: a constructor, a `copy()`, a library function such as `first()`, or a function
that may answer something else or reads its list through a local, a getter, a function value or a `type`. A
library class is not checked.

### `--debug-memory`

`--debug-memory` reports total allocations and frees right before the program exits (`allocations: N frees: N`),
and they must balance exactly, not just be bounded, for every example, every documentation program,
`tests/references` and the compiler's own runs. When they do not, it also prints a **leaked-object summary by class
name**, naming which classes' instances are still live. The table keeps each live allocation's class beside it and
forgets it on free, so the summary names exactly the objects still alive, the same ones on every run; memory that
is not an object (a list's elements, a string's bytes) is counted but not named. An object a program leaked is
named even when it points at a singleton, which is still destroyed at exit. The compiler's own bookkeeping
(the list behind `.instances`, the list of singletons to destroy) is allocated outside the table and not counted.

**Fresh memory is filled with a fixed pattern.** Every byte an allocation or a growing `resize` hands out that the program has not written is `0xA5`, so a
`Long` read from it is -6510615555426900571 and an address read from it points nowhere: reading bytes nobody wrote
fails the same way on every run, instead of depending on what the heap held, which the launcher's environment
decides. A `resize` keeps the bytes it moves and fills only the new ones; the table keeps each allocation's size
for that. Only a `--debug-memory` build does this.

**Run-time cost.** The table and the summary exist only in a `--debug-memory` build: no other build's C
has them. Every other build allocates with the C library's `malloc`, `realloc` and `free` and nothing else (a
production build takes the objects a list holds from their class's pool, [below](#placement-the-compiler-decides-where-memory-lives),
whose runs of blocks are themselves from `malloc`), unless the program reads `Memory.Heap().live_allocations()` or `live_bytes()`: then, and only then, each
allocation adds one to a counter and each free subtracts one, which is what `live_allocations()` answers, and the
bytes the C library holds for each block (its usable size, `_msize`, `malloc_usable_size` or `malloc_size`) are
added and subtracted the same way, which is what `live_bytes()` answers; a `--debug-memory` build answers the
bytes asked for, from its table
([optimizations.md](../docs/optimizations.md#allocation-is-the-c-librarys-counted-only-where-read)).

### Where a value lives: `.memory`

Every named value has a read-only `.memory`, a `Spite.Memory` (`library/spite/memory.spite`): `address`, `bytes`,
and `section`, one of `'heap'`, `'stack'` or `'constant'`. A class instance or a list answers with its object
(`'stack'` for an object the compiler placed in the frame, [Placement](#placement-the-compiler-decides-where-memory-lives)), a
`String` with its characters (`'constant'` for a literal, whose characters are part of the program; for text made
while the program runs, `'heap'` when it is longer than 15 bytes and lives in a block, and otherwise wherever the
value itself is, since the characters are in it: `'stack'` in a local, `'heap'` in an attribute),
and a number held in a local with the local itself (`'stack'`). A class with an attribute of its own named `memory` answers that
attribute instead. `.memory` is built only where a program reads it, so it costs nothing anywhere else; the
same object is where an allocator is set ([below](#allocators-memoryallocator)).

### The floor: `Memory.Address`, `Memory.Heap` and `TypedMemory<T>`

`Memory` is a namespace, not an object: there is no `Memory()` to make.

- **`Memory.Address`** (`library/memory/address.spite`) is a number class of eight bytes, the place in memory. It
  casts to and from `Long` like every number, so `address + 16` is the address sixteen bytes on, and a foreign
  function's `Long` result is an address by assignment. `copy_to(target, bytes)`, `compare_bytes(other, bytes)`,
  `text(length)` and `terminated_text()` are open to every program.
- **Reading and writing**: `read_byte`, `read_short`, `read_unsigned_short`, `read_integer`,
  `read_unsigned_integer`, `read_long`, `read_float` and `read_double`, each `(offset)`, the matching
  `write_*(offset, value)`, and `exchange_long`, `read_long_atomically`, `write_long_atomically`,
  `add_long_atomically` (answering the sum) and `compare_and_swap_long(offset, expected, desired)` (answering
  whether it swapped) for memory threads share; a program uses them through `Atomic<T>`
  ([concurrency.md](../docs/concurrency.md#a-number-every-thread-shares-atomict)). Each is a primitive of the language, like `+`: the compiler writes it where it is called as one
  load, store or atomic instruction, with no call and no check, so reading a field costs what it costs in C
  ([optimizations.md](../docs/optimizations.md#reading-an-address-is-one-machine-operation)). **Only `library/` may call
  them**: a function of a class the standard library declares, including one a program reopens (so
  `--final-classes` output, which reopens every class it prints, still compiles). Anywhere else:
  `'read_long' reads or writes the memory at an address, which only library/ does: use a class of the standard library, or 'TypedMemory<T>' for values of any type`
  (`diagnostics/memory_access`). On a `Long` that is not an address: `a Long has no function 'read_integer':
  reading and writing memory is a function of the address, so the value needs to be a 'Memory.Address'`.
- **`Memory.Heap()`** is the default allocator, a singleton with `allocate(bytes: Long): Memory.Address`,
  `resize(address, bytes): Memory.Address`, `free(address)`, `live_allocations(): Integer` and `live_bytes(): Long`
  (`Program().live_bytes()` answers the same). Nothing frees an
  allocation for you: a class that allocates frees in its `drop()`. The bytes are not cleared: a new block, and
  the part `resize` adds, hold whatever was there before, so a byte must be written before it is read. A read of
  a byte nobody wrote can pass on one machine and crash on the next, since what the heap holds depends even on how
  the program was launched. Where an allocation lives is the compiler's choice ([placement](#placement-the-compiler-decides-where-memory-lives)).
  The heap holds nothing, so in a production build it is one static object and costs no allocation
  ([optimizations.md](../docs/optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **`TypedMemory<$value_type>`** stores a value of any type (a class, a `String`, a number or a nullable) at an
  index of an address: `read_value(address, index)`, `write_value(address, index, value)`,
  `release_value(address, index)` and `value_bytes()`, keeping reference counts right for that type. It is one
  shared instance per type (a singleton that holds nothing), and it is what `library/list.spite` uses for its
  elements. Its three value functions use the address the way a read does, so a frame-placed allocation may be
  handed to them.
- **A number's storage** is one line at the top of its file, `var _memory = Memory.Bytes(4)`. The bytes are written out, and any other spelling is `a value states its size as 'var _memory =
  Memory.Bytes(bytes)', with the bytes written out: ...` (`diagnostics/number_memory_spelling`). The size must be
  the C type's: `'Integer' declares 8 bytes of memory, and the compiler lays it out as
  int32_t, which is 4` (`diagnostics/number_memory`). Inside a number `this` is the value, and reading `_memory` is
  `'_memory' is the memory Integer is kept in, and the compiler keeps the value itself there: write 'this'`
  (`diagnostics/number_memory_read`). `count.memory.bytes` reads the size.

### Placement: the compiler decides where memory lives

Memory is an abstraction that lets a program allocate on the heap, the stack or in a register, while the
compiler decides the best placement. A program has one way to ask for raw memory, `Memory.Heap`'s `allocate`,
and one way to give it back, `free`; where the bytes live is the compiler's choice, and the program's text
is the same whichever it makes:

- **Register:** a number's own memory, whose size its file states as `var _memory = Memory.Bytes(4)` (for
  `Integer`). The wrapper is flattened: a number is its C scalar, and `this` is the value.
- **Frame:** `var name = heap.allocate(bytes)` in a function, when a later statement of the same block
  is `heap.free(name)` and every other use of `name` reads or writes through it (`name.read_long(offset)`, also
  through a local made by adding an offset, `var start = name + offset`, whose uses meet the same rule), copies or compares with it (`copy_to`, `compare_bytes`), turns it into `text`, or hands
  it to a `TypedMemory`'s `read_value`, `write_value` or `release_value` (a `TypedMemory` the class binds; the same
  names on any other class are an ordinary call, `conformance/stage6/kept_buffer_address`), or lends it to a function of the same
  class, called by its bare name, whose `Memory.Address` parameter is proven to keep nothing (the same rules,
  applied to the parameter in that function's body, and to the functions it lends it on to; a recursive lend and a
  `--hot-reload` build, whose functions can be swapped, prove nothing), or, in a file of `library/` only, lends it
  to a function of a `DynamicLibrary` attribute of the same class (`kernel.QueryPerformanceCounter(counter)`: the
  standard library's own operating-system calls, which fill the memory and keep nothing; a program's foreign
  library could keep the address, so its calls still move it to the heap), and it is never stored, returned,
  assigned, resized or passed to anything else, and neither `name` nor `heap` is declared or assigned again
  after it. The compiler gives it a slot of 256 bytes
  in the function's frame (exactly the size, for a literal size up to 256), uses the heap when a run-time size
  is larger, and makes the `free` a no-op for the slot. A loop body is a block like any other, so the slot is
  reused on every pass. So `Long.to_string()` and `upper_case()` of a
  short text allocate only the `String` they return, which is nothing when it is 15 bytes or fewer.
- **In the value:** text of up to 15 bytes is kept in the sixteen bytes of the `String` itself
  ([optimizations.md](../docs/optimizations.md#short-text-lives-inside-the-string)).
- **Constant:** a `String` literal's characters are part of the program (its `.memory.section` is `'constant'`).
- **Frame, for objects:** an instance of a class of only
  numbers, `Boolean`s, enum values and singletons (no `drop()`, not a singleton, a constructor that keeps nothing,
  no `.instances` read of its class) that never leaves the function that made it lives in that function's frame,
  and so does the fresh answer a function writes into its caller's slot and a temporary inside an expression. Never
  leaving means: read and written only through its attributes, handed only to functions proven to keep nothing
  (the same proof as for a buffer's slot, run on each parameter and on the object a function is called on), compared, asked for
  its `.memory`, given a new fresh object, and returned only from a function that answers its class (copied to
  the heap there). Its `.memory.section` is `'stack'`. Not in the inspectable builds, nor in a function that waits
  ([optimizations.md](../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)). A local made
  by its constructor lives in the frame on the same terms when its class also holds text, lists, dictionaries or
  other objects (and is not a container): what it holds is let go where the local's scope ends, and returning it
  moves it to the heap, attributes and all.
- **Heap, in its class's pool:** an object on the heap whose class is not a singleton and frees its objects only
  through its own release, when the class holds no `Memory.Address` attribute or the program keeps its objects in a
  list (the item of a `List`, or of anything else built on `TypedMemory<T>`, such as a `Dictionary`'s values), and,
  in a program with threads, no code that can run on another thread counts, makes, copies or frees
  one of its objects. The class's objects come from blocks of their own size side by side, in runs of 16 objects
  doubling until a run is at least 256 KiB, each run starting on a 64-byte boundary; an object let go is kept for
  the class's next object and is never handed to another class or back to the system while the program runs. Its
  `.memory.section` is `'heap'`. Not in a `--debug-memory` build, a program that reads `live_allocations()` or
  `live_bytes()`, or the inspectable builds
  ([optimizations.md](../docs/optimizations.md#objects-of-one-class-sit-together)).
- **Heap:** everything else.

There is no way to ask for the stack by name: it would be a second way to
allocate, and one a program could get wrong by keeping an address past the return. What makes a data structure's
layout efficient stays its author's: one allocation holding many values at offsets they choose,
`TypedMemory<$value_type>` for values of any type, `resize` to grow. Placement runs entirely while compiling and
costs nothing at run time; a frame slot is cheaper than the heap call it replaces.

### Allocators: `.memory.allocator`

Types never name an allocator; an **object** does, through its tree-shakeable
`.memory`, on the line right after it is made (`var scratch = List<Integer>()` then `scratch.memory.allocator =
arena`), and the compiler reads the two lines as where the object was always meant to live, so it is made
there from the start. Setting it after the object was used is a compile error naming `copy()`. The rules:

- **An allocator is a class that answers `allocate(bytes: Long): Memory.Address` and `free(address:
  Memory.Address)`** (no `type` is declared for it). `Memory.Heap` is the default, and
  setting it changes nothing. `Memory.Arena(block_bytes)` (`library/memory/arena.spite`, Spite over the heap)
  hands out 16-byte-aligned memory from blocks of that size, chaining a new block when one is full, never frees
  one piece at a time, and frees every block when the arena itself is dropped.
- **Where it may be set:** only as the statement directly after `var name =
  ...` whose value is a constructor call, `List<T>()`/`Dictionary<Key, Value>()` or `.copy()` of a class whose copy the
  compiler writes, and only to a name or a path of names (so evaluating it earlier than written changes nothing).
  Anything else is an error (`diagnostics/allocator_after_use`):
  - after the object was used: `'spark' was already used, so its allocator can no longer change: an
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
  count reaches zero; the allocator therefore outlives everything made in it. **Tree-shaken**: only a class
  some line gives an allocator carries the two hidden pointers (16 bytes per object of that class), and nothing
  else in the program changes; there is no run-time registry and no current-allocator state.
- **Lists:** a `List` given an allocator is made there, and so is its buffer of references, each time it grows;
  each element lives wherever it was made, since a list holds references. A `Vector<T>`'s block of items and an
  `Items<T>`' storage follow their object the same way. A `Dictionary` given an allocator is made there, and its
  table and its lists of keys and values stay on the heap.
- **Reading it back:** `value.memory.allocator`, for a name or a path of names, answers the allocator the object
  was made in, `Memory.Heap` unless a line said otherwise (an object the compiler placed in a frame answers
  `Memory.Heap`, the allocator it is made in once it leaves). Inside a class, `memory.allocator` is the
  allocator of the object the function runs on, unless the class has an attribute of its own named `memory`.
  The answer is one of the program's allocators, a union of `Memory.Heap` and every class that is an allocator:
  `allocator == Memory.Arena` asks which and narrows, and `allocate` and `free` can be called on it as on any
  union whose members all answer them. A class that keeps memory for its object (a buffer, a block of items)
  allocates and frees it through `memory.allocator`, so it follows its object into any allocator; growing it
  is allocating the new size, copying, and freeing the old, except on the heap, where `heap.resize` grows it
  in place (`library/list.spite`'s `_resized`). What such a call may do is what the `allocate` or `free` of each
  of the program's allocators does: an item borrowed from a `Vector` stays readable past a `List`'s growth
  unless one of them changes the size of the collection it is borrowed from (`diagnostics/allocator_resizes`).
  The errors:
  - on a number or a `String`: `only an object has an allocator of its own, and this is a value of type
    Integer, which the compiler places itself`;
  - on a value that is not a name or a path of names: `only a named object answers its allocator: give this
    object a name with 'var' first`;
  - `memory.allocator` inside a singleton or a number: `'memory.allocator' is the allocator of the object a
    function runs on, and 'Counter' is not made as an object of its own: only an object has an allocator`.
- **An allocator is not generic:** an instance of a generic class is never an allocator, since the program's
  allocators are known before any generic class is made: `'Pool' is a generic class, and an
  allocator is a class of its own that is not generic, like 'Memory.Arena'`. A class is an allocator only
  when `allocate` takes one `Long` and answers a `Memory.Address`, and `free` takes one `Memory.Address` and
  answers nothing; with other parameters it is not one (`'Console' is not an allocator`, above).

### Borrowed items of a `Vector<T>`

**Reading an item of a `Vector<T>` gives a borrowed reference into the vector**, and the compiler proves at
compile time that it is never kept past its use. The rules:

- **Layout.** A vector of a class keeps one block of memory: each item is the class's attributes laid out as its
  object would lay them out, with no header, no class and no reference count, one after another
  (`InlineMemory<T>`, `library/inline_memory.spite`, whose functions the compiler writes per item type). A vector
  of numbers, `Boolean`s, enums or `String`s is a plain array of them, like a list's buffer.
- **Only a class is borrowed.** A whole number
  of any size, `Float`, `Double`, `Boolean`, an enum or `Memory.Address` is kept in a `List`, and
  `Vector<Integer>` or `Items<Integer>` is an error naming `List<Integer>`, so every item of a `Vector` is a class
  or a `String`, read as a counted reference as a list's is. `conformance/stage6/plain_items`,
  `diagnostics/plain_items`.
- **A borrowed item** is what `vector[index]`, `vector.get_at(index)`, `vector.find_by_<member>(value)` and the
  item a member template visits are, once the `T?` a read answers is narrowed: `crash velocities[index]`,
  `assert`, `if`, or a proof the compiler holds.
  It is the address of the item inside the block, and reading or writing its attributes (`first.down = 2.0`,
  `velocities[0].across`, `first.integrate()`) reads and writes the vector's own item. It is never retained or
  released: it costs nothing to take and nothing to let go.
- **Where it may be named.** `var first = velocities[index]` names one, and the name is borrowed for the rest of
  its block. Each of these is an error, named for where the item was going, and each names `copy()`, which makes an
  independent object from it (the generated copy reads only the attributes):
  - kept in an attribute: `'stored' is borrowed from 'velocities' and cannot be kept in the attribute 'kept':
    keep 'stored.copy()', an independent object`;
  - put in a list (`append`, `prepend`, `insert`, `set_at` or `list[index] = `): `... cannot be put in a list: a
    list keeps 'stored.copy()', an independent object`;
  - returned: `'velocities[0]' is borrowed from 'velocities' and cannot be returned: return 'velocities[0].copy()',
    an independent object`;
  - made into a function value: `'stored.integrate' would keep 'stored', which is borrowed from 'velocities', in a
    function value: make it from a copy, 'var kept = stored.copy()' and then 'kept.integrate'`;
  - passing it as an argument is **not** among them: the item is lent to the call (below);
  - given a second name (`var alias = stored`): `'stored' is borrowed from 'velocities', and a borrowed item has
    one name: use 'stored' itself instead of 'alias', or keep 'stored.copy()', an independent object`;
  - assigned again (`first = velocities[1]`): `'first' is borrowed from 'velocities' and is not assigned again:
    read another item with a new 'var', such as 'var next = velocities[index]'`;
  - borrowed from a collection nothing keeps (`var found = velocities.copy().find_by_name("fast")`, or
    `velocities.filter_moving()[0]`), which is let go at the end of the line with the item in it: `'found' would
    be borrowed from 'velocities.copy()', which nothing keeps, so it is let go at the end of this line and the
    item with it: keep it in a 'var' first and borrow from that` (`diagnostics/found_item_borrows`). Any call
    answering the collection counts, since a borrowed item is never counted to keep its collection alive.
- **Not past a change of size.** Growing a vector may move its block, and removing an item moves the ones after
  it, so a borrowed name is not read after a statement that may change its vector's size or move its block:
  `append`, `prepend`, `insert`, `reserve`, `remove_at`, `remove_first`, `remove_last`, `remove_swapping`,
  `remove_where`/`remove_where_<member>`, `truncate`, `swap` (it changes which item an index names) or
  `clear` on it, assigning the vector or anything on its path, or a call that may do one of those. `reserve` adds no
  item but may move the block as surely as `append` does, so it counts as growing
  (`diagnostics/vector_reserve_borrows`), and so do the library's own `make_room`, `_make_room` and
  `_reserve_exactly`, which a program reopening `Vector` or `Items` could call. A call is followed with the call
  effects (`generation/call_effects.spite`): a function that appends to or reserves room in a vector records a
  `grow:` effect on it as a
  removal records `shrink:`, through its callers and through the parameters it was passed, and a call through a
  function value may do anything. A statement in a loop that may change the size ends the borrow for the whole
  loop. The error is on the statement that changes it: `'again' is borrowed from 'velocities', and 'spawn()' on
  line 13 may move the items of 'velocities', so 'again' is not read after it: read 'velocities[index]' again after
  that line, or keep 'again.copy()', an independent object`. A vector named by a local variable is taken to change
  only through the calls it is passed to and through calls that resize an attribute holding a `Vector`.
- **What a call can reach.** The call effects follow only what the call can run, so a call that cannot resize the
  item's vector is not refused:
  - **A collection read with `[ ]` from an attribute is that attribute's element.** `buffers[buffer].append(command)`
    grows an item of `World.buffers`, not "some collection": when `buffers` is a `List<List<Command>>` it can only
    move the items of a `List`, never those of an `Items` or `Vector` borrowed from, so `entity.add_component(...)`,
    which queues a command into such a buffer, leaves an item of `Column<Position>().values` borrowed. The grown
    item still counts when its class is the vector's own (a `List<Items<Position>>` item may be that very vector),
    when its class is not known, when the item is itself read with `[ ]` from another item, and for a vector named
    by a local variable (`diagnostics/dispatched_resizes`).
  - **A call through a `type` reaches the classes it is actually handed.** `console.print("at", place.left)` calls
    `to_string()` on each of its arguments through `Printable`; at the call the arguments are a `String` and an
    `Integer`, so only `String.to_string` and `Integer.to_string` are followed, not every `to_string` in the
    program. The classes come from the arguments at the call: a text or `Boolean` literal, or a name or attribute
    path whose type is a class, a number, a `Boolean` or `String`, read through a `type`'s own attribute too
    (`follower.entity.id`) and through a local declared before the call. They pass into the functions the
    arguments are handed on to, a variadic list keeping the classes of the arguments it was made from while no
    function it reaches assigns, appends to, stores or hands it anywhere else. Anything the compiler cannot place
    (a nullable, a union, a `type`, the result of a call, a parameter reassigned in the function) is followed
    through every function of that name, so a class whose `to_string()` grows the vector is still
    refused when it is printed (`console.print("printing", loud)` in `diagnostics/dispatched_resizes`).
  - **Cost.** None at run time; both are read while compiling from the effects already gathered.
    `conformance/stage6/queued_borrows` holds an item across a queued command and a print.
- **Inside the item's class.** A function of the item's class runs on a borrowed item when a template or a call
  reaches it, so it may not use `this` as a value (the item-class error in
  [collections.md's rules](collections.md#vectort)).
- **Only the library is trusted.** The functions of `Vector`, `Items` and `InlineMemory` in `library/` hand out
  and move borrowed items by design, so these rules are not checked inside them. A function a program adds to one
  of those classes by reopening it in its own file ([packages.md](../docs/packages.md)) is checked like any other code,
  a member template of its own included: an item it reads with `values.item_at(items, index)` is borrowed, so it
  is not returned, kept or put in a list (`diagnostics/reopened_vector_borrows`, `diagnostics/own_template_borrows`).
- **What stays an object.** `append(value)` and `set_at(index, value)` copy the value's attributes in, counting
  each `String` attribute once more, and the value stays an ordinary object; `remove_at`, `clear` and dropping the
  vector release the `String`s of the items they remove.
- **Run-time cost.** None beyond the block: every rule above is proven while compiling, a borrowed item is
  one address, and a program that makes no `Vector` carries none of `library/vector.spite` or `InlineMemory`.
  Measured in `benchmarks/particles`, a `Vector` of particles stepped in place against the same array of structs
  in C.
- **A row of borrowed items.** An object literal written as `var row = {...}` or `var row: Moving = {...}`
  whose attributes include a borrowed item (`positions[index]`, or a name already borrowed from a vector) is a
  **row**: each attribute is a borrowed item or a plain value (a number, `Boolean` or enum), and anything else is
  `'mixed' borrows items from a Vector, so each of its attributes is a borrowed item or a plain value: 'name'
  holds a String, which the row would have to keep`. The row is a borrowed name like the items it holds:
  - It is **passed** only as an argument of a function called by name (`mover.update_each(row)`, `report(row)`)
    whose parameter at that place is a `type`. That parameter is then a row too for the length of the call: the
    compiler writes a second copy of the function, `<name>___lent_<positions>`, in which reading
    `moving.position` answers the item uncounted and the parameter is never released, and calls it; every other
    caller keeps the ordinary function, so passing a class instance to the same `update_each` is unchanged. A row
    passed to any other function is `'row' holds items borrowed from 'positions' and 'velocities' for this call
    and is passed only to a function called by name whose parameter is a 'type', which borrows it for that call:
    pass the attributes the function needs`.
  - It is never **kept**, in the function that makes it or the one it is passed to: `'moving' holds items
    borrowed from 'positions' and 'velocities' for this call and cannot be kept in the attribute 'kept': keep the
    values it needs, or a 'copy()' of one of its items`, and the same for being returned (`... and cannot be
    returned: return the values the caller needs, or a 'copy()' of one of its items`), put in a list (`... cannot
    be put in a list: a list keeps a 'copy()' of one of its items`), given a second name (`..., and a row has one
    name: use 'row' itself instead of 'alias'`) or assigned again (`... and is not assigned again: make each row
    with a new 'var'`). A required function of its `type` read as a value is `'row.describe' would keep 'row',
    ... in a function value: call 'row.describe()' instead`.
  - Its **attributes** are borrowed items: `moving.position.left = 3.0` and `moving.velocity.slow_down()` work on
    the vector's item, `var position = moving.position` names it (borrowed, with every rule above), and
    `moving.position.integrate` as a function value is `'moving.position.integrate' would keep 'moving.position',
    which is borrowed from 'moving', in a function value: ...`. The row's own attributes are not assigned:
    `'moving' holds items borrowed from 'positions' and 'velocities' for this call, so 'moving.position' is not
    assigned: write the item's attributes, such as 'moving.position.<attribute> = ...'`.
  - **The call may not resize what it borrows from.** A statement that passes the row, directly or inside an
    `if`, `while` or `switch` after it, is checked with the call effects above: a call whose `grow:` or `shrink:`
    facts reach a vector the row borrows from is `'row' borrows items from 'positions' for the call it is passed
    to, and 'keeper.spawn_while_moving()' on line 13 may move the items of 'positions': a function that is handed
    a row may not append to or remove from the vectors it borrows from, directly or through what it calls`, and
    a call through a function value may do anything. After the call the row follows the rule for any borrowed
    name: it is not read past a line that may change the size of one of its vectors.
  - **Cost.** The row is a struct in the frame of the function that makes it, with a header whose count is never
    touched, and a read through the parameter is the `type`'s uncounted read, one class test and a load
    ([optimizations.md](../docs/optimizations.md#a-row-of-borrowed-items-lives-in-the-frame)). A program that makes no
    row carries none of it. `benchmarks/a_row_of_borrowed_items_lives_in_the_frame`: 20 000 000 rows over four
    `Vector` columns, none allocated, measured against C making a row per call.
- **A row filled by a walk.** A local declared as a `type` with `= null` (`var row: $row_type = null`) and filled on the very next
  line by a plural walk of its own class (`fill_attributes(row, index)`, whose template ranges over that `type`
  with `attribute: Symbol<$row_type>`) is a row when the walk writes out to one: the template's body is the
  statement `row.attributes[attribute] = <value>`, after any number of `crash` lines and `var` lines naming a
  value (`var stored_row = rows[attribute.index]`, each written out where its name is read, so a hoisted
  `values[stored_row]` walks exactly as `values[rows[attribute.index]]` does) or naming an object the template
  goes on to change (below), the value built from `<object>.attributes[attribute]` (read as
  `<object>.<the attribute's name>`), member reads, `[ ]`, the template's other parameters and whole numbers, and
  the call's arguments are the row and names, attribute paths (`matcher.rows`) or number or `Boolean`
  literals. The compiler then writes, in place of the two lines, each attribute's `crash` lines walked as the
  value is (`crash columns.position[index]`), then the literal, one attribute for each attribute of the `type` in
  its order (`{position: columns.position[index], velocity: columns.velocity[index]}`), and every rule above
  applies to it unchanged, the check for a resize starting on the line after the walk.
  **A walked read is narrowed by the template's `crash` lines**
  (every `[]` answers `T?`, and a row's attribute is an item, never a `T?`). The walk cannot prove its index: the
  places come from a list the runner filled, and nothing the compiler knows ties them to the columns' counts. So
  the template states each read it relies on, innermost first (`crash rows[attribute.index]`, then `var
  stored_row = rows[attribute.index]` and `crash Column<attribute.class>().values[stored_row]`); a read the lines do not narrow is the usual `this
  value may be null` error on the walk's line. The lines sit inside the branch they narrow, so a template that
  chooses per attribute keeps its shape (`if attribute.class.fits_vector() { crash ...; row.attributes[attribute] =
  Column<attribute.class>().values[...] } else { crash ...; row.attributes[attribute] =
  ReferenceColumn<attribute.class>().values[...] }`), and a reference read from a `List` column is narrowed the same
  way. A `crash` line is walked like the fill line, and a generic
  singleton made with no arguments is a path root (`Column<Position>().values[found[1]]` narrows like a name's
  read), so each line narrows the very read the literal makes. Cost: a walked `crash` line
  reads the item once into a local and tests it, and the literal (or the lent argument) uses that local instead
  of reading again, so a row costs one compare per read; the borrowed items, the
  frame-made row and the resize check are unchanged (`benchmarks/a_walked_crash_lines_read_is_the_rows_read`). The template is not called, so it is not
  compiled for that walk. A walk of any other shape leaves the local an ordinary `type` value, and storing a
  borrowed item in it is the error for keeping one. Nothing runs for it: the rewrite is done while compiling.
- **A walked row over sparse columns.** A walk that fills a
  row may also be written with:
  - **A generic singleton per attribute.** `Column<attribute.class>()` in the line is the singleton made with the
    walked attribute's class, written out as `Column<Position>()` for `position`; its `Vector` attribute is the
    source the item is borrowed from (`'row' borrows items from 'Column<Position>().values' ...`), and the resize
    check follows the singleton's attribute through the call effects as it follows a class's own attribute. A
    singleton made from `attribute.class` is never an attribute, so it may be written where it is used, inline
    included (the usual errors for a singleton made inline leave it out).
  - **The place of the attribute.** `attribute.index` is the attribute's place among those walked, from 0, a
    constant written into the line (`found[attribute.index]` becomes `found[1]`), and it may be read in any
    function a walk of attributes calls. The places are read from a `List<Integer>`, whose numbers are values of their
    own, so the list may be any object's: the runner's own attribute, a walk argument such
    as `matcher.rows`, or `matcher.rows[attribute.index]` read in the line itself. It is read once, when the row is
    made, and no rule of the row reaches it, so a runner does not copy another object's places into its own
    vector first. Read elsewhere, it is `'attribute.index' is the attribute's place among
    the attributes walked, so it is read in a function a walk of attributes calls, such as 'fill_attribute(attribute:
    Symbol<Row>, ...)'`. The line's parts are `<value>.attributes[attribute]`, attribute reads, `[ ]`, calls
    (constructions included), the walk's parameters, `attribute.index` and whole numbers; anything else in the line
    of a walk that would borrow is `'row' is a row filled by the walk on the next line, and its line for
    'position' reads 'attribute.index + 1', which a walked row cannot write out: ... so work out 'attribute.index
    + 1' before the walk and pass it in`.
  - **A local the template changes.** A `var` that later
    lines of the branch change, by assigning into it (`own.id = entity`, through `set_id` when the class has one) or
    by calling one of its functions (`own.raise(1)`), is made as a local before the row, those lines run where they
    stand among the `crash` lines, and the row's attribute is that local. When the `var` is a construction the row
    could make in the frame (see *Mixed attributes*), the row's attribute is the local itself, and neither those lines
    nor the functions they call (nor, in turn, the functions of its class those call) use `this` as a value, it is
    made in the frame and allocates nothing; otherwise it is an ordinary counted object. Lines that change
    anything else make the walk ordinary calls. `conformance/stage6/walked_row_locals`.
  - **A choice per attribute.** The template's statement may be an `if` whose conditions are decided while
    compiling for each attribute (`attribute.class == Entity`, `attribute.class.fits_vector()`,
    `attribute.class.has_function(...)`), each branch holding one such line and its `crash` and `var` lines; the branch
    taken for an attribute is what is written out for it.
  - **Mixed attributes.** Beside borrowed items, an attribute of a walked row may hold any other value. A
    construction of a class that could be a `Vector` item and holds nothing counted (`Entity(entity)`) is made in
    the frame, never allocated, and is borrowed like the items: it lives exactly as long as the row, and its
    constructor may not use `this` as a value. Any other counted value (a reference read from a reference column,
    a `String`) is counted once when the row is made and let go at the end of the row's block, so a call that
    removes it from its column cannot free it under the row. The resize check covers the borrowed items' vectors
    only. A row written as a literal keeps the rule for a row: borrowed items and plain values only.
  - **A reference column's element lent to the row.** The count above is left out when it cannot
    matter: the attribute's line calls a function of a singleton (`Column<Trail>().at(found[3])`) whose body is
    only `return <list>[<index>]` after `crash` of that same read (which is asked for, since the read is a `T?`),
    where `<list>` is an attribute of the singleton holding a `List` of a class that nothing assigns after the
    singleton is made and `<index>` is a whole-number parameter or literal, the function takes nothing counted, and
    the rest of the row's block is proven to let go of nothing: every call in it is one the compiler can name (a
    phase template's call named as the phase's own function), and none of them, nor anything they reach, lets go of
    an object, removes from or replaces into a list, assigns an attribute holding an object or calls through a
    function value, and no class whose objects can be let go in the meantime has a `drop()` reaching that
    singleton or its list. The row then holds the element as it lies in the list, never retained or released, read
    through a copy of the function, `<name>___lent_element`, which answers exactly what the function answers, crash
    included. In a program where a `Parallel` reaches the singleton, the element is also held against other
    threads for the rest of the block: the block takes the singleton's readers' side or its lock once, and
    only when what the block runs reaches no singleton and nothing that waits; otherwise the line calls the function
    and counts its result as above. Nothing is an error: every other caller of the function, and every line that
    keeps the element, counts it as before. **Cost**: none; the lock or readers' side is taken once for the block
    where the call would have taken it once. `conformance/stage6/lent_list_elements`,
    `conformance/stage6/lent_list_elements_parallel`
    ([optimizations.md](../docs/optimizations.md#a-row-of-borrowed-items-lives-in-the-frame)).
  - **Cost.** Nothing runs for any of it: the singleton is the one a program would reach anyway, `attribute.index`
    is a constant, the choice is made while compiling, and the frame-made object is a struct in the frame.
    `benchmarks/a_walked_crash_lines_read_is_the_rows_read` walks rows over sparse columns and measures them
    against C.
- **Items of an `Items<T>`.** An [`Items<T>`](collections.md#itemst) whose `T` fits a
  `Vector` lends its items exactly as a `Vector` does: `items[index]`, `get_at(index)` and the item a template
  visits are borrowed, and every rule above applies to them, rows and walked rows included; `remove_swapping`
  counts as a removal wherever `remove_at` does, on the collection or through the call effects. An `Items<T>`
  whose `T` does not fit lends nothing: its items are counted references, and none of these rules apply to them.
  In a walked row, `Column<attribute.class>().values[stored_row]` over an `Items` value is therefore a
  borrowed item for a class that fits and a counted value for one that does not, a mix decided per
  attribute while compiling. Every error about an `Items` item begins with the choice that made it borrowed:
  `'Velocity' fits a Vector, so the items of 'velocities' are borrowed: ...`. Nothing runs for any of it.
- **Arguments a plural fills.** A call whose whole argument list is the plural of a template over that function's
  arguments, written as a statement of its own (`system.phase_each(made_arguments(found))`), stands for exactly
  one call, and its results may carry borrowed items into it. For each argument in order, the compiler folds the
  template's body for that argument (an `if` decided while compiling keeps only its branch, as in a walked row)
  and writes it out before the call when what is left is:
  - **one `return` of a walked line**, after any `crash` lines that narrow its reads (written out before the
    call, walked for each argument, as a walked row's are), built as a walked row's line is
    (`<value>.attributes[...]` aside):
    attribute reads, `[ ]`, calls and constructions, the template's other parameters (read as the plural's
    arguments), `argument.index` and whole numbers, with `Column<argument.class>()` the singleton made with the
    argument's class: `return Column<argument.class>().values[stored_row]` after `var stored_row =
    rows[argument.index]` (a `var` naming a walked value is written out where its name is read). It is a local of the call;
    a borrowed item from a `Vector`, or from an `Items` whose class fits one, is a borrowed name, and the function
    is called through its `___lent_<positions>` copy, in which that parameter is a borrowed name for the call: it
    is not retained or released, and every rule above holds for it (it is not kept, returned, given a second name
    or assigned; it may be lent on to a call). Any other value (`Entity(entity)`, a reference from an `Items` that does not fit) is
    an ordinary local, counted for the call and let go after it.
  - **a walked row declared, filled and returned** (`var row: $row_type = null`, `fill_attributes(row, rows)`,
    `return row`, or `var row: argument.class = null`), which is written out as the walked row it is, under
    every rule above, and passed as a row. Folding `argument.class == $row_type` is what lets a generic
    `Stream<$system_type, $row_type>` fill the one argument of its row type this way
    (`conformance/stage6/streamed_rows`).
  Any other body is the template's ordinary call; a call none of whose arguments borrows is left as
  it was. A parameter the function ignores, named `_position`, is filled and passed like the others, and a
  local written for it drops the underscore, since the call reads it
  (`conformance/stage6/ignored_lent_argument`). **The call may not resize what it borrows from**: a call whose call effects may append to or remove
  from a borrowed argument's collection is `'Position' fits a Vector, so the items of 'Column<Position>().values'
  are borrowed: 'position' is borrowed from 'Column<Position>().values' for the one call
  'system.phase_each(made_arguments(found))' stands for, and 'system.update_each()' on line 20 may move the items
  of 'Column<Position>().values': a function handed borrowed arguments may not append to or remove from the
  collections they borrow from, directly or through what it calls` (a row keeps its own error). **Anywhere else a
  result keeps the refusal for a borrowed item**: the template called by its single name (`made_position(found)`) is an ordinary
  function, and its `return` of a borrowed item is the error for returning one, which then adds `-- a template
  over the arguments of 'phase_each' may hand back a borrowed item only through its plural, as the whole argument
  list of the one call it fills: 'phase_each(made_arguments(...))'` (`diagnostics/lent_arguments`).
  **Cost.** The written-out arguments cost what the walked line costs; the template is not called, so it is not
  compiled for that call ([optimizations.md](../docs/optimizations.md#a-row-of-borrowed-items-lives-in-the-frame);
  `conformance/stage6/lent_arguments`).
- **An item lent to the caller.** A function one of whose `return`s reads an item straight from a singleton's
  storage (`return column.values[row]` or `return values[index]`, `get_at` too, where the path is attributes
  of the function's class, the object holding the last attribute is a singleton, and that attribute is a `Vector`
  or an `Items` whose class fits one) **lends its result**: the item is returned uncounted and its caller never
  lets it go. It is decided per class the function is compiled for, so `Lookup<Label>.of` over an `Items` of
  references returns a counted reference. **The returns are read per instance with compile-time
  conditions folded**: an `if` whose condition is made only of `$T.fits_vector()`,
  `$T.has_function("name")` with a plain literal name, and a generic parameter compared with a class (`$component_type != Entity`), joined with
  `and`, `or` and `not`, keeps only its taken branch (`function_waits` is left unfolded here, since answering it
  this early would decide it before the functions that ask it are compiled), and a statement after an `if` folded true whose branch returns is dead,
  so it does not count either. `Lookup.of` written as `if $component_type.fits_vector() and $component_type !=
  Entity { return column.values[row] }` followed by `return column.value_at(row)` lends for every instance whose
  class fits and returns an owned value for `Entity` and every class that does not fit, and each caller treats
  the result as its instance does (`conformance/stage6/folded_lends`). At every caller:
  - the result is a borrowed item of `<Singleton>().<attribute>` (`'layout' is borrowed from
    'Column<Layout>().values'`), and every rule above applies to it: `var layout = lookup.of(entity)` names it,
    `layout.order = 3` and `lookup.of(entity).order = 3` write the stored item, and it is not kept in an
    attribute, put in a list, given a second name, assigned again, captured in a function value or read past a line whose call effects may append to or remove from that storage, whose error ends
    `... so 'grown' is not read after it: ask for the item again after that line, or keep 'grown.copy()', an
    independent object`;
  - **it is not returned further**: only the read from the singleton's storage lends, so `return lookup.of(0)`, or
    returning a name that holds it, is the error for returning a borrowed item, ending `-- a function lends its
    caller an item only by reading it from a singleton's Items or Vector itself, never one that was lent to it
   `;
  - a name already used in the block is not reused for it: `'tally' already names a value earlier in this block,
    and an item borrowed from 'Column<Tally>().values' is given a name of its own: call it something new, such as
    'var tally_item = ...'` (this applies to every borrowed item);
  - **narrowing keeps it borrowed**: `if layout { ... }`, `if layout and
    layout.order > 0 { ... }` make it a plain `Layout` inside the block that is still the borrowed item, so passing it to a call lends it and keeping, listing or returning it is the
    error above. If the narrowed name were an ordinary local, a call would retain and release the stored item,
    which is a double free under `--debug-memory` and heap corruption in production
    (`conformance/stage6/narrowed_lends`, `diagnostics/lent_escapes`);
  - **it is called by name only**: a lending function is never made a function value (`var find =
    lookup.of`, `entities.each(lookup.of)`), whose caller would let go of an item it does not own, with the error `'of' lends its
    caller an item of 'Column<Justify>().values', which only a call written by name can borrow: a function
    value's caller would let go of an item it does not own, so 'of' is not made a function value; call 'of(...)'
    by name where the item is wanted`, and its class does not fit a `type` requiring that function: `'Lookup'
    does not fit type 'Finder': its 'of' lends its caller an item of 'Column<Justify>().values', which only
    a call written by name can borrow: a call through the type would let go of an item it does not own`.
  A function that lends returns only such items or `null`: any other value would be an object the caller never
  lets go, so it is `'at_or_made' lends its caller an item of 'Scores().values', which the caller writes in place
  and never lets go, so each of its returns is an item of 'Scores().values' or null: 'made' is neither, so return
  null for 'none', with a '?' result, or make it a function of its own`. A storage path through a local, a
  parameter or an object that is not a singleton keeps the refusal for a borrowed item, since that storage may not
  outlive the call. **Cost**: none, since the return is the item's address, with no retain, and the caller releases nothing
  (`conformance/stage6/lent_results`, `diagnostics/lent_results`, `conformance/stage6/folded_lends`,
  `conformance/stage6/narrowed_lends`, `diagnostics/lent_escapes`).
- **An item lent to a call.** A borrowed item passed as an
  argument, whether a borrowed name (`mouse`, including one lent to the function itself), an item read with `[ ]`
  (`mice[0]`) or a row's attribute (`device.mouse`), at a place whose parameter is not a union or a variadic
  list, to a function of the program called by name, is **lent** to that call. The compiler writes a second copy
  of the function, `<name>___lent_<positions>` (the same mechanism as for rows and for the arguments a plural fills), in
  which that parameter is a borrowed name for the call: it is never retained or released, reading and writing it
  reads and writes the caller's item, and passing it on to another call lends it again, to that function's own
  `___lent_` copy. Every caller that passes a class instance keeps the ordinary function.
  - **The function keeps nothing.** Every rule above holds for the lent parameter inside the copy, so each way of
    keeping it is an error in that function naming the parameter and the call that lent it (`'mouse' is lent to
    'keep' for one call, by the call on line 9 of 'LentToCalls': it is the caller's item, read and written
    in place,` followed by the site):
    - kept in an attribute: `... so 'keep' does not keep it in the attribute 'last': keep the values it needs, or
      have the caller pass something 'keep' may keep`;
    - put in a list: `... so 'list' does not put it in a list, which would keep it: keep the values it needs`;
    - returned: `... so 'hand_back' does not return it: return the values the caller needs`;
    - made into a function value: `'mouse.move' would keep 'mouse' in a function value, and 'mouse' is lent to
      'capture' for one call, so 'capture' does not keep it: call 'mouse.move()' directly, or keep the
      values it needs`;
    - given a second name: `... and a lent item has one name: use 'mouse' itself instead of 'alias'`.
    These are the facts escape analysis proves per parameter ([optimizations.md](../docs/optimizations.md)): the function
    keeps nothing its parameter reaches. They are checked here by compiling the function under the borrow rules, which
    names the very line that would keep it.
  - **The call may not resize what the item is borrowed from.** A statement that lends an item is checked with
    the call effects: a call whose `grow:` or `shrink:` facts reach the collection the item is borrowed from (for
    a row's attribute, each collection the row borrows from) is `'mouse' is borrowed from 'mice' and lent to
    'grow' for its call, and 'input.grow()' on line 19 may move the items of 'mice': a function lent a borrowed
    item may not append to or remove from the collection it is borrowed from, directly or through what it calls;
    change the size of 'mice' before or after the call`. An item lent on from inside a lent function is
    covered by the check at the call that first lent it, whose call effects include everything the function
    reaches. The calls a plural fills keep their own error.
  - **Where it is not lent, the error says why, and never offers a copy the function would write** (a write
    to a copy vanishes silently). Passing to a library function: `'first' is borrowed from 'other', and 'append'
    is a library function, which is never lent an item: pass the attributes it needs, or 'first.copy()' where a
    snapshot is what it wants`; to a union parameter: `... and 'f' takes it as a union, which holds its own values
    and is never lent an item: pass the attributes it needs, or give 'f' a parameter of the item's own class`;
    any other expression (such as a call that lends its result, written inside the argument): `... only a named
    item, an item read with '[ ]' or a row's attribute is lent to a call: name it first, 'var item =
    ...', and pass 'item'`.
  - **Cost.** None: the argument is the item's address, the parameter is never counted, and nothing is copied.
    The `___lent_` copy is compiled only for the positions a program lends, and a program that lends nothing
    carries none (tree-shaken like any function). `conformance/stage6/lent_to_calls` (an input system handing a
    walked row's mouse and keyboard to `apply(event, mouse, keyboard)`, which writes them and passes them on,
    with balanced memory and the writes read back from the vectors), `diagnostics/lent_to_calls`.
- **A walked line names the attribute's class wherever it was declared.** `Column<attribute.class>()` in a walked
  row or a plural's argument is written out with the class's full name, so a row type declared in another namespace
  (`mouse: Component.Mouse` in `Input`) names the same class from the runner's file.

`diagnostics/vector_borrows`, `diagnostics/vector_items`, `diagnostics/vector_rows`, `conformance/stage6/vector_items`,
`conformance/stage6/vector_rows`, `conformance/stage6/walked_rows`, `diagnostics/walked_rows`,
`conformance/stage6/sparse_rows`, `diagnostics/sparse_rows`, `conformance/stage6/items_columns`,
`diagnostics/items_borrows`, `conformance/stage6/lent_arguments`, `conformance/stage6/streamed_rows`,
`diagnostics/lent_arguments`, `conformance/stage6/lent_to_calls`, `diagnostics/lent_to_calls`.

---

Next: [Metaprogramming](metaprogramming.md).
