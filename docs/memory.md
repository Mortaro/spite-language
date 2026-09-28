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

## A `Vector` lends its items

A `Vector<T>` is the one place a value is not a counted reference: it holds its items inline, one block of their
attributes with no header ([collections.md](collections.md#vectort-items-inline)), and `velocities[index]` is the
item inside that block, **borrowed**. Writing its attributes writes the vector's item; taking it and letting it go
costs nothing. In exchange the compiler never lets it be kept -- in an attribute, a list, a returned value, a
function value, or past a line that may grow or shrink the vector -- and each of those errors names `copy()`,
which makes an independent object ([the rules](#borrowed-items-of-a-vectort--implemented)).

Only an item that is a class is lent. An item that is a plain value -- a number, a `Boolean`, an enum -- is
copied as it is read ([D221](decisions.md)): `var count = counts[index]` is a number of its own, as reading a
list's number is, so it may be kept, passed, returned and read after `counts` grows, and a vector of numbers takes
a passed function (`counts.each(found.append)`, `weights.filter(is_heavy).sum(double_of)`) as a list does.

### A row of borrowed items, for one call

An engine keeps each component in its own `Vector` and hands a system one entity's components at a time. The
row is an object literal of borrowed items, and the system takes it as a `type`
([D206](decisions.md)):

```gdscript title=vector_row_doc/position.spite
var left = 0.0
```
```gdscript title=vector_row_doc/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=vector_row_doc/mover.spite
type Moving {
    position: Position
    velocity: Velocity
}

func update_each(moving: Moving) {
    moving.position.left = moving.position.left + moving.velocity.across
}
```
```gdscript title=vector_row_doc/vector_row_doc.spite entry
var console = Console()
var positions = Vector<Position>()
var velocities = Vector<Velocity>()
var mover = Mover()

func VectorRowDoc() {
    var position = Position()
    positions.append(position)
    var velocity = Velocity(2.5)
    velocities.append(velocity)
    var tick = 0
    while tick < 4 {
        var row: Moving = {position: positions[0], velocity: velocities[0]}
        mover.update_each(row)
        tick = tick + 1
    }
    console.print(positions[0].left)
}
```
```output
10
```

The row is made in the function's frame, not on the heap, and nothing in it is counted: `update_each` writes the
vectors' own items. In exchange the row lives for exactly the call it is passed to. It is never kept, returned,
put in a list or given a second name, by the function that makes it or the one it is passed to, and the call may
not append to or remove from a vector the row borrows from, directly or through anything it calls
([the rules](#borrowed-items-of-a-vectort--implemented)).

A generic runner does not know the attributes of the `type` it is given, so it cannot write the literal. It fills
the row with a `Symbol` walk instead, and a local of the `type` declared `= null` and filled by the walk on the
next line is a row exactly like the literal ([D212](decisions.md)):

```gdscript title=walked_row_doc/position.spite
var left = 0.0
```
```gdscript title=walked_row_doc/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=walked_row_doc/mover.spite
type Moving {
    position: Position
    velocity: Velocity
}

func update_each(moving: Moving) {
    moving.position.left = moving.position.left + moving.velocity.across
}
```
```gdscript title=walked_row_doc/columns.spite
singleton

var position = Vector<Position>()
var velocity = Vector<Velocity>()

func size(): Integer {
    return position.count()
}
```
```gdscript title=walked_row_doc/runner.spite
generic $row_type

var columns = Columns()

func run(mover: Mover) {
    var index = 0
    while index < columns.size() {
        var row: $row_type = null
        fill_attributes(row, index)
        mover.update_each(row)
        index = index + 1
    }
}

func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, index: Integer) {
    row.attributes[attribute] = columns.attributes[attribute][index]
}
```
```gdscript title=walked_row_doc/walked_row_doc.spite entry
var console = Console()
var columns = Columns()
var runner = Runner<Moving>()
var mover = Mover()

func WalkedRowDoc() {
    var position = Position()
    columns.position.append(position)
    var velocity = Velocity(2.5)
    columns.velocity.append(velocity)
    runner.run(mover)
    runner.run(mover)
    console.print(columns.position[0].left)
}
```
```output
5
```

The compiler writes the walk out where it is called: for `Runner<Moving>` the two lines become
`var row: Moving = {position: columns.position[index], velocity: columns.velocity[index]}`, so the row costs what
the literal costs and follows the same rules, and `fill_attribute` is never called.

An engine that adds and removes components all the time keeps each one in a **sparse set**: a generic singleton
`Column<Position>` whose `Vector` is packed, so each entity sits at a different place in each column. The runner
works out those places first, one per attribute, and the walk's line reads each attribute from its own column at
its own place: `Column<attribute.class>().values[found[attribute.index]]`, where `attribute.index` is the
attribute's place in the `type` ([D217](decisions.md)). The places are numbers, copied as they are read, so they
may come from any `Vector<Integer>` -- the runner's own, or another object's, `fill_attributes(row,
matcher.rows)` ([D221](decisions.md)). A class that cannot be a `Vector` item stays a reference
in a `List`, `attribute.class.fits_vector()` choosing while compiling, and the row may hold it beside the borrowed
items, as it may hold an `Entity` made from the id:

```gdscript title=sparse_row_doc/position.spite
var left = 0.0
```
```gdscript title=sparse_row_doc/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=sparse_row_doc/entity.spite
var id = 0

func Entity(new_id: Integer) {
    id = new_id
}
```
```gdscript title=sparse_row_doc/trail.spite
var lefts = List<Float>()
var last = 0.0
var owner = -1
```
```gdscript title=sparse_row_doc/mover.spite
type Moving {
    entity: Entity
    position: Position
    velocity: Velocity
    trail: Trail
}

func update_each(moving: Moving) {
    moving.position.left = moving.position.left + moving.velocity.across
    moving.trail.lefts.append(moving.position.left)
    moving.trail.last = moving.position.left
    moving.trail.owner = moving.entity.id
}
```
```gdscript title=sparse_row_doc/column.spite
singleton

generic $component_type

var dense_of = List<Integer>()
var values = Vector<$component_type>()

func insert(entity: Integer, value: $component_type) {
    while dense_of.count() <= entity {
        dense_of.append(-1)
    }
    dense_of[entity] = values.count()
    values.append(value)
}

func dense_index(entity: Integer): Integer {
    crash dense_of[entity]
    return dense_of[entity]
}
```
```gdscript title=sparse_row_doc/reference_column.spite
singleton

generic $component_type

var dense_of = List<Integer>()
var values = List<$component_type>()

func insert(entity: Integer, value: $component_type) {
    while dense_of.count() <= entity {
        dense_of.append(-1)
    }
    dense_of[entity] = values.count()
    values.append(value)
}

func dense_index(entity: Integer): Integer {
    crash dense_of[entity]
    return dense_of[entity]
}

func at(dense: Integer): $component_type {
    crash values[dense]
    return values[dense]
}
```
```gdscript title=sparse_row_doc/runner.spite
generic $row_type

var found = Vector<Integer>()

func run(mover: Mover, entity_count: Integer) {
    var entity = 0
    while entity < entity_count {
        found.clear()
        find_attributes(entity)
        var row: $row_type = null
        fill_attributes(row, found, entity)
        mover.update_each(row)
        entity = entity + 1
    }
}

func find_attribute(attribute: Symbol<$row_type>, entity: Integer) {
    if attribute.class == Entity {
        found.append(entity)
    } else if attribute.class.fits_vector() {
        var column = Column<attribute.class>()
        var dense = column.dense_index(entity)
        found.append(dense)
    } else {
        var column = ReferenceColumn<attribute.class>()
        var dense = column.dense_index(entity)
        found.append(dense)
    }
}

func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, rows: Vector<Integer>, entity: Integer) {
    if attribute.class == Entity {
        row.attributes[attribute] = Entity(entity)
    } else if attribute.class.fits_vector() {
        row.attributes[attribute] = Column<attribute.class>().values[rows[attribute.index]]
    } else {
        row.attributes[attribute] = ReferenceColumn<attribute.class>().at(rows[attribute.index])
    }
}
```
```gdscript title=sparse_row_doc/sparse_row_doc.spite entry
var console = Console()
var runner = Runner<Moving>()
var mover = Mover()
var positions = Column<Position>()
var velocities = Column<Velocity>()
var trails = ReferenceColumn<Trail>()

func SparseRowDoc() {
    var first = Position()
    positions.insert(0, first)
    var second = Position()
    positions.insert(1, second)
    var slow = Velocity(1.0)
    velocities.insert(1, slow)
    var fast = Velocity(4.0)
    velocities.insert(0, fast)
    var first_trail = Trail()
    trails.insert(0, first_trail)
    var second_trail = Trail()
    trails.insert(1, second_trail)
    runner.run(mover, 2)
    runner.run(mover, 2)
    var first_steps = first_trail.lefts.count()
    var second_steps = second_trail.lefts.count()
    console.print(first_trail.owner, first_trail.last, first_steps)
    console.print(second_trail.owner, second_trail.last, second_steps)
}
```
```output
0 8 2
1 2 2
```

For `Runner<Moving>` the walk is written out as `{entity: <an Entity in the frame>, position:
Column<Position>().values[found[1]], velocity: Column<Velocity>().values[found[2]], trail:
ReferenceColumn<Trail>().at(found[3])}`: the `Entity` is made in the frame, since `Entity` could be a `Vector`
item, and the `Trail` is counted once for the row and let go after the call. Only `Position` and `Velocity` are
borrowed, so only their vectors are checked for a resize during the call.

Two column classes, and a choice in every line that reaches them, are only there because a `Vector<Trail>` cannot
be made. An [`Items<T>`](collections.md#itemst-the-storage-chosen-for-you) makes that choice itself, inline when
the class fits and by reference when it does not ([D218](decisions.md)), so one `Column<$component_type>` holding
`var values = Items<$component_type>()` serves every component, and the fill template needs no `fits_vector()`:

```gdscript
func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, rows: Items<Integer>, entity: Integer) {
    if attribute.class == Entity {
        row.attributes[attribute] = Entity(entity)
    } else {
        row.attributes[attribute] = Column<attribute.class>().values[rows[attribute.index]]
    }
}
```

For `Moving` the row borrows `Column<Position>().values[found[1]]` and `Column<Velocity>().values[found[2]]` and
counts `Column<Trail>().values[found[3]]` for the call, exactly as above, and a column removes an entity with
`values.remove_swapping(dense)`, which a system handed the row may not reach for a column it borrows from
(`conformance/stage6/items_columns`).

### Borrowed arguments, for one call

A system may take its components as arguments instead of a row, `update_each(position: Position, velocity:
Velocity)`, and a generic runner fills them with a template over the function's arguments whose plural is the
call's whole argument list ([metaprogramming.md](metaprogramming.md#asking-for-a-function-and-walking-its-arguments)).
The template's line is the walked row's line, read per argument: `argument.index` is the argument's place, so
`Column<argument.class>().values[rows[argument.index]]` is that component's item. The plural stands for exactly
one call the compiler writes, so its results may be borrowed items for that call and no longer
([D220](decisions.md)):

```gdscript title=lent_arguments_doc/position.spite
var left = 0.0
```
```gdscript title=lent_arguments_doc/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=lent_arguments_doc/mover.spite
func update_each(position: Position, velocity: Velocity) {
    position.left = position.left + velocity.across
}
```
```gdscript title=lent_arguments_doc/column.spite
singleton

generic $component_type

var values = Items<$component_type>()
```
```gdscript title=lent_arguments_doc/runner.spite
generic $system_type

enum Phase {
    'update'
}

var system = $system_type()
var found = Items<Integer>()

func run(index: Integer) {
    found.clear()
    found.append(index)
    found.append(index)
    run_phases_each()
}

func run_phase_each(phase: Symbol<$system_type.phase_each>) {
    system.phase_each(made_arguments(found))
}

func made_argument(argument: Symbol<$system_type.phase_each>, rows: Items<Integer>): argument.class {
    return Column<argument.class>().values[rows[argument.index]]
}
```
```gdscript title=lent_arguments_doc/lent_arguments_doc.spite entry
var console = Console()
var positions = Column<Position>()
var velocities = Column<Velocity>()
var runner = Runner<Mover>()

func LentArgumentsDoc() {
    var position = Position()
    positions.values.append(position)
    var velocity = Velocity(2.5)
    velocities.values.append(velocity)
    runner.run(0)
    runner.run(0)
    console.print(positions.values[0].left)
}
```
```output
5
```

For `Runner<Mover>` the call is written out as `system.update_each(Column<Position>().values[found[0]],
Column<Velocity>().values[found[1]])`: `update_each` writes the columns' own items, and nothing is copied, counted
or allocated. Every rule for a borrowed item holds for the call: `update_each` may not keep `position` or return
it, it may lend it on to a call of its own ([below](#an-item-lent-to-a-call)), and nothing it reaches may append
to or remove from a column it borrows from. `made_position`
called anywhere else is an ordinary function, and returning a borrowed item from it is the error for returning one.
The template may also fill a whole walked row for an argument that is a `type`
([the rules](#borrowed-items-of-a-vectort--implemented)).

### An item lent to the caller

A singleton lives until the program ends, so an item of its `Items` or `Vector` can be handed to a caller: a
function that returns `column.values[row]`, read straight from a singleton's storage, **lends** the stored item,
and the caller writes it in place ([D230](decisions.md)):

```gdscript title=lent_result_doc/column.spite
singleton

generic $component_type

var values = Items<$component_type>()
```
```gdscript title=lent_result_doc/lookup.spite
generic $component_type

var column = Column<$component_type>()

func of(row: Integer): $component_type? {
    assert row < column.values.count()
    return column.values[row]
}
```
```gdscript title=lent_result_doc/layout.spite
var order = 0
```
```gdscript title=lent_result_doc/lent_result_doc.spite entry
var console = Console()
var layouts = Column<Layout>()
var lookup = Lookup<Layout>()

func LentResultDoc() {
    var made = Layout()
    layouts.values.append(made)
    var layout = lookup.of(0)
    crash layout
    layout.order = 3
    console.print(layouts.values[0].order)
}
```
```output
3
```

Nothing is copied or counted, so `layout.order = 3` is never a write to a copy that is then thrown away. The
borrow now belongs to the caller, and every rule for a borrowed item holds there: `layout` is not kept, put in a
list, returned further or given a second name, and it is not read past a line that may append to or remove from
`Column<Layout>().values`. It may be lent on to a call, as the next section shows, and `if layout { ... }`
narrows it without ending the borrow. A function that lends gives back only such items, or `null`, and it is
called by name: it is never made a function value or called through a `type`, whose caller could not know the
item is not its own. In a generic class the choice is made per instance, with the conditions on its generic
parameters folded first, so one `of` may lend a fitting class's item and hand back an owned value for the rest.

### An item lent to a call

A borrowed item -- a name read from a `Vector` or `Items`, `velocities[index]` itself, a row's attribute, an item
lent to the caller -- may be passed as an ordinary argument. It is **lent for the call**: the function reads and
writes the caller's own item and may lend it on to the functions it calls, and when the call returns the borrow is
the caller's again ([D257](decisions.md)):

```gdscript title=lent_call_doc/mouse.spite
var left = 0
var pressed = false
```
```gdscript title=lent_call_doc/input.spite
func apply(step: Integer, mouse: Mouse) {
    mouse.left = mouse.left + step
    press(mouse)
}

func press(mouse: Mouse) {
    mouse.pressed = true
}
```
```gdscript title=lent_call_doc/lent_call_doc.spite entry
var console = Console()
var mice = Vector<Mouse>()
var input = Input()

func LentCallDoc() {
    var made = Mouse()
    mice.append(made)
    var mouse = mice[0]
    input.apply(3, mouse)
    input.apply(4, mice[0])
    console.print(mice[0].left, mice[0].pressed)
}
```
```output
7 true
```

`apply` and `press` write the vector's item: nothing is copied, counted or allocated, so no write can land on a
copy and vanish. What the function may not do is keep the item past the call -- store it in an attribute or a
list, return it, or make a function value of it -- and each of those is an error in the function, naming the call
that lent it: `'mouse' is lent to 'keep' for one call (D257), by the call on line 9 of 'LentToCalls': it is the
caller's item, read and written in place, so 'keep' does not keep it in the attribute 'last': keep the values it
needs, ...`. Nothing the call reaches may append to or remove from the collection the item is borrowed from,
since that could move it while the function holds it.

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
    var first_leaf = Leaf(1)
    var second_leaf = Leaf(2)
    var small_branch = Branch(first_leaf, second_leaf)
    var third_leaf = Leaf(3)
    var tree: TreeNode = Branch(small_branch, third_leaf)
    var total = tree.total()
    console.print("total", total)
}
```
```output
total 6
```

## Cycles leak

Two objects that hold each other, directly or through several hops, keep each other's count above zero, so
neither is ever freed. The back reference is written with `Weak<T>` instead ([D197](decisions.md)): it holds a
`T` without counting it, and `get()` answers a `T?` that is `null` once the object is freed, so the usual
narrowing does the rest. A parent that owns its children and a child that knows its parent is the common case:

```gdscript title=weak_family/parent.spite
var name = ""
var children = List<Child>()

func Parent(new_name: String) {
    name = new_name
}

func adopt(child_name: String) {
    var child = Child(child_name, this)
    children.append(child)
}
```
```gdscript title=weak_family/child.spite
var name = ""
var parent = Weak<Parent>(null)

func Child(new_name: String, new_parent: Parent) {
    name = new_name
    parent = Weak(new_parent)
}
```
```gdscript title=weak_family/weak_family.spite entry
var console = Console()

func WeakFamily() {
    var parent = Parent("ada")
    parent.adopt("grace")
    var child = parent.children.first()
    crash child
    var found = child.parent.get()
    crash found
    console.print(child.name, "belongs to", found.name)
}
```
```output
grace belongs to ada
```

Everything above is freed when `WeakFamily` returns, which is what `--debug-memory`, below, checks: a cycle left
in shows up there as leaked objects by class. Without `Weak`, clear the `T?` field that closes the loop when you
are done with it ([the rules](#memory--implemented)).

## `--debug-memory`

`spite program --debug-memory` builds with an allocation table and prints `allocations: N frees: N` right before
the program exits. A mismatch means something leaked; when the two do not balance, it also prints a
**leaked-object summary by class name**, naming which classes' instances are still alive -- which is what makes a
leaked cycle visible instead of an unexplained count. Every program on these pages is run this way, and must
balance. It also fills every byte it hands out that nobody has written yet with the same pattern, so a program that
reads memory it never wrote fails the same way on every run and from every launcher, instead of reading whatever
the heap held. The table exists only in a `--debug-memory` build ([what it records](#--debug-memory-1)).

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
var heap = Memory.Heap()
var numbers = TypedMemory<Long>()

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
  and each element lives wherever it was made. A `Vector` holds its items inline
  ([collections.md](collections.md#vectort-items-inline)); giving one an allocator places the vector object, and
  its block of items stays on the heap for now.
- **It costs nothing where it is not used** ([D177](decisions.md)): only a class some line gives an allocator
  grows, by sixteen bytes per object.

The exact rules, the errors and what is not built yet are in
[the rules](#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-a-vectors-block-planned).

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
  A union compares identity the same way. A comparison releases whatever operand it produced itself -- an item
  read with `[]`, a call's result -- so `kept[index] == shape` holds nothing afterwards ([D258](decisions.md)).
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
  clear a `T?` field that closes the loop) if it matters for a long-running program, or hold one side with
  `Weak<T>`.
- **`Weak<T>` holds an object without counting it** (D197, decided by Mortaro: a generic library class, not a
  keyword; `library/weak.spite`). `Weak(object)` makes one, `Weak<T>(null)` an empty one, and `get(): T?` answers
  the object while something else keeps it alive and `null` once it has been freed. As built (proposed by Claude,
  unconfirmed): the first `Weak` of an object makes a small box holding its address, found through a table keyed
  by address; a class some `Weak` holds tells the table when an instance is freed, so its box answers `null`; the
  box goes when the last `Weak` of it does. A program that never makes a `Weak` carries none of this: the table,
  the boxes and the one check in the freeing of each weakly held class are written only for the classes a `Weak`
  holds, and `T` must be a class (`Weak<Integer>` is an error, since a number is kept by value)
  (`conformance/stage6/weak_parent`). **A `Weak` stays on the program's own thread** (D211, decided by Claude under
  D205): the table has no lock, so a `Parallel(work)` whose work reaches a class some `Weak` holds, or a class with a
  `Weak` attribute, is a compile error at the `Parallel`: `'Parallel(tree.grow)' runs on another thread, and what it
  runs reaches 'Branch', which is a Weak or is held by one: a Weak is kept in one table that is not shared between
  threads, so a Weak and the objects it holds stay on the program's own thread. Hold the object itself in the
  work, or keep the Weak out of what the Parallel reaches` (`diagnostics/weak_across_threads`). What the work
  reaches is the same walk over calls that decides which singletons need a lock.
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

**Fresh memory is filled with a fixed pattern** (D211, decided by Claude under D205; the pattern proposed by
Claude). Every byte an allocation or a growing `resize` hands out that the program has not written is `0xA5`, so a
`Long` read from it is -6510615555426900571 and an address read from it points nowhere: reading bytes nobody wrote
fails the same way on every run, where it used to depend on what the heap held, which the launcher's environment
decided. A `resize` keeps the bytes it moves and fills only the new ones; the table keeps each allocation's size
for that. Only a `--debug-memory` build does this.

**Run-time cost (D177).** The table and the summary exist only in a `--debug-memory` build: no other build's C
has them. Every other build allocates with the C library's `malloc`, `realloc` and `free` and nothing else,
unless the program reads `Memory.Heap().live_allocations()`: then, and only then, each allocation adds one to a
counter and each free subtracts one, which is what it answers
([optimizations.md](optimizations.md#allocation-is-the-c-librarys-counted-only-where-read)).

#### Where a value lives: `.memory`

Every named value has a read-only `.memory`, a `Spite.Memory` (`library/spite/memory.spite`): `address`, `bytes`,
and `section`, one of `'heap'`, `'stack'` or `'constant'`. A class instance or a list answers with its object
(`'stack'` for an object the compiler placed in the frame, [Placement](#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed)), a
`String` with its characters (`'constant'` for a literal, whose characters are part of the program; for text made
while the program runs, `'heap'` when it is longer than 15 bytes and lives in a block, and otherwise wherever the
value itself is, since the characters are in it: `'stack'` in a local, `'heap'` in an attribute, [D203](decisions.md)),
and a number held in a local with the local itself (`'stack'`). A class with an attribute of its own named `memory` answers that
attribute instead. `.memory` is built only where a program reads it, so it costs nothing anywhere else (D152); the
same object is where an allocator is set ([below](#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-a-vectors-block-planned)).

#### The floor: `Memory.Address`, `Memory.Heap` and `TypedMemory<T>`  **[implemented; OS pages planned]**

`Memory` is a namespace, not an object (D151): there is no `Memory()` to make.

- **`Memory.Address`** (`library/memory/address.spite`) is a number class of eight bytes, the place in memory. It
  casts to and from `Long` like every number, so `address + 16` is the address sixteen bytes on, and a foreign
  function's `Long` result is an address by assignment. `copy_to(target, bytes)`, `compare_bytes(other, bytes)`,
  `text(length)` and `terminated_text()` are open to every program.
- **Reading and writing** (D178): `read_byte`, `read_short`, `read_unsigned_short`, `read_integer`,
  `read_unsigned_integer`, `read_long`, `read_float` and `read_double`, each `(offset)`, the matching
  `write_*(offset, value)`, and `exchange_long`, `read_long_atomically`, `write_long_atomically`,
  `add_long_atomically` (answering the sum) and `compare_and_swap_long(offset, expected, desired)` (answering
  whether it swapped) for memory threads share; a program uses them through `Atomic<T>`
  ([concurrency.md](concurrency.md#a-number-every-thread-shares-atomict)). Each is a primitive of the language, like `+`: the compiler writes it where it is called as one
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
  it to a `TypedMemory`'s `read_value`, `write_value` or `release_value`, or lends it to a function of the same
  class, called by its bare name, whose `Memory.Address` parameter is proven to keep nothing (D211: the same rules,
  applied to the parameter in that function's body, and to the functions it lends it on to; a recursive lend and a
  `--hot-reload` build, whose functions can be swapped, prove nothing), or, in a file of `library/` only, lends it
  to a function of a `DynamicLibrary` attribute of the same class (`kernel.QueryPerformanceCounter(counter)`: the
  standard library's own operating-system calls, which fill the memory and keep nothing; a program's foreign
  library could keep the address, so its calls still move it to the heap; proposed by Claude, unconfirmed) -- it is never stored, returned,
  assigned, resized or passed to anything else, and neither `name` nor `heap` is declared or assigned again
  after it. The compiler gives it a slot of 256 bytes
  in the function's frame (exactly the size, for a literal size up to 256), uses the heap when a run-time size
  is larger, and makes the `free` a no-op for the slot. A loop body is a block like any other, so the slot is
  reused on every pass. So `Long.to_string()` and `upper_case()` of a
  short text allocate only the `String` they return -- which is nothing when it is 15 bytes or fewer.
- **In the value:** text of up to 15 bytes is kept in the sixteen bytes of the `String` itself ([D203](decisions.md),
  [optimizations.md](optimizations.md#short-text-lives-inside-the-string)).
- **Constant:** a `String` literal's characters are part of the program (its `.memory.section` is `'constant'`).
- **Frame, for objects** (proposed by Claude, unconfirmed; decided under D205/D214): an instance of a class of only
  numbers, `Boolean`s, enum values and singletons -- no `drop()`, not a singleton, a constructor that keeps nothing, no
  `.instances` read of its class -- that never leaves the function that made it lives in that function's frame,
  and so does the fresh answer a function writes into its caller's slot and a temporary inside an expression. Never
  leaving means: read and written only through its attributes, handed only to functions proven to keep nothing
  (the same proof as D211's, run on each parameter and on the object a function is called on), compared, asked for
  its `.memory`, given a new fresh object, and returned only from a function that answers its class (copied to
  the heap there). Its `.memory.section` is `'stack'`. Not in the inspectable builds, nor in a function that waits
  ([optimizations.md](optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)).
- **Heap:** everything else.

There is no way to ask for the stack by name (D98's `allocate_stack_bytes` is gone): it would be a second way to
allocate, and one a program could get wrong by keeping an address past the return. What makes a data structure's
layout efficient stays its author's -- one allocation holding many values at offsets they choose,
`TypedMemory<$value_type>` for values of any type, `resize` to grow. Placement runs entirely while compiling and
costs nothing at run time (D177); a frame slot is cheaper than the heap call it replaces.

#### Allocators: `.memory.allocator`  **[implemented for objects; a list's buffer and a vector's block planned]**

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
  references on the heap), and likewise a `Vector<T>`'s block of items (built, with its items inline, but its block
  is on the heap wherever the vector object is: item 175's proposal for a class reading its own allocator is still
  open), `Memory.Frame` (`mortaros_missing_decisions.md`
  item 173), `reset()` on an arena, and reading `.memory.allocator` back.

#### Borrowed items of a `Vector<T>`  **[implemented]**

D204 (decided by Mortaro, answering `mortaros_missing_decisions.md` item 182): **reading an item of a
`Vector<T>` gives a borrowed reference into the vector**, and the compiler proves at compile time that it is never
kept past its use. What is built (the error texts and the readings marked are proposed by Claude, unconfirmed):

- **Layout.** A vector of a class keeps one block of memory: each item is the class's attributes laid out as its
  object would lay them out, with no header, no class and no reference count, one after another
  (`InlineMemory<T>`, `library/inline_memory.spite`, whose functions the compiler writes per item type). A vector
  of numbers, `Boolean`s, enums or `String`s is a plain array of them, like a list's buffer.
- **Only a class is borrowed; a plain value is copied** (D221, decided by Claude under D205 and D214; the
  readings below proposed by Claude, unconfirmed). An item of a `Vector<T>` or an `Items<T>` whose `T` is a plain
  value -- a whole number of any size, `Float`, `Double`, `Boolean`, an enum, `Memory.Address` -- is copied when
  it is read and never borrowed: none of the rules below applies to it, so `var first = numbers[0]` may be kept in
  an attribute or a list, returned, passed, given a second name, captured and read after `numbers` grows, as a
  list's number may. The passed-function forms `each`, `map`, `filter`, `count`, `any`, `all` and `sum` work on
  such a vector or `Items` as on a list ([collections.md](collections.md#passing-a-function-for-each-element)),
  and a list's own functions may be passed (`numbers.each(found.append)`). A `String` is not a plain value for
  this rule: it is read as a counted reference, as a list's is, which is safe, but the passed-function forms stay
  a list's, the conservative choice (a `String` item holds a reference to its text beyond 15 bytes). Nothing is
  weakened: a copied number cannot outlive or be moved away from anything, and a copy of eight bytes costs no
  more than an address. `conformance/stage6/plain_items`, `diagnostics/plain_items`.
- **A borrowed item** is what `vector[index]`, `vector.get_at(index)` and the item a member template visits are.
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
  - passing it as an argument is **not** among them: the item is lent to the call (D257, below);
  - given a second name (`var alias = stored`): `'stored' is borrowed from 'velocities', and a borrowed item has
    one name: use 'stored' itself instead of 'alias', or keep 'stored.copy()', an independent object`;
  - assigned again (`first = velocities[1]`): `'first' is borrowed from 'velocities' and is not assigned again:
    read another item with a new 'var', such as 'var next = velocities[index]'`.
- **Not past a change of size.** Growing a vector may move its block, and removing an item moves the ones after
  it, so a borrowed name is not read after a statement that may change its vector's size or move its block:
  `append`, `prepend`, `insert`, `reserve`, `remove_at`, `remove_first`, `remove_last`, `remove_swapping` or
  `clear` on it, assigning the vector or anything on its path, or a call that may do one of those. `reserve` adds no
  item but may move the block as surely as `append` does, so it counts as growing (proposed by Claude,
  unconfirmed; `diagnostics/vector_reserve_borrows`), and so do the library's own `make_room`, `_make_room` and
  `_reserve_exactly`, which a program reopening `Vector` or `Items` could call. A call is followed with D169's call
  effects (`generation/call_effects.spite`): a function that appends to or reserves room in a vector records a
  `grow:` effect on it as a
  removal records `shrink:`, through its callers and through the parameters it was passed, and a call through a
  function value may do anything. A statement in a loop that may change the size ends the borrow for the whole
  loop. The error is on the statement that changes it: `'again' is borrowed from 'velocities', and 'spawn()' on
  line 13 may move the items of 'velocities', so 'again' is not read after it: read 'velocities[index]' again after
  that line, or keep 'again.copy()', an independent object`. A vector named by a local variable is taken to change
  only through the calls it is passed to and through calls that resize an attribute holding a `Vector`.
- **What a call can reach** (D262, decided by Claude under D205 and D244, a bug fix: the readings below proposed
  by Claude, unconfirmed). The call effects follow only what the call can run, so a call that cannot resize the
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
    -- a nullable, a union, a `type`, the result of a call, a parameter reassigned in the function -- is followed
    through every function of that name, as before, so a class whose `to_string()` grows the vector is still
    refused when it is printed (`console.print("printing", loud)` in `diagnostics/dispatched_resizes`).
  - **Cost.** None at run time; both are read while compiling from the effects already gathered.
    `conformance/stage6/queued_borrows` holds an item across a queued command and a print.
- **Inside the item's class.** A function of the item's class runs on a borrowed item when a template or a call
  reaches it, so it may not use `this` as a value (the item-class error in
  [collections.md's rules](collections.md#vectort--implemented)).
- **Only the library is trusted.** The functions of `Vector`, `Items` and `InlineMemory` in `library/` hand out
  and move borrowed items by design, so these rules are not checked inside them. A function a program adds to one
  of those classes by reopening it in its own file ([packages.md](packages.md)) is checked like any other code
  (proposed by Claude, unconfirmed; `diagnostics/reopened_vector_borrows`).
- **What stays an object.** `append(value)` and `set_at(index, value)` copy the value's attributes in, counting
  each `String` attribute once more, and the value stays an ordinary object; `remove_at`, `clear` and dropping the
  vector release the `String`s of the items they remove.
- **Run-time cost (D177).** None beyond the block: every rule above is proven while compiling, a borrowed item is
  one address, and a program that makes no `Vector` carries none of `library/vector.spite` or `InlineMemory`.
  Measured in `benchmarks/vector_items`.
- **A row of borrowed items** (D206, decided by Claude under D205; the error texts and the readings below are
  proposed by Claude, unconfirmed). An object literal written as `var row = {...}` or `var row: Moving = {...}`
  whose attributes include a borrowed item -- `positions[index]`, or a name already borrowed from a vector -- is a
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
    ([optimizations.md](optimizations.md#a-row-of-borrowed-items-lives-in-the-frame)). A program that makes no
    row carries none of it. `benchmarks/vector_rows`: 200 000 entities, two systems, 2.4 ms a tick with `Vector`
    columns and rows against 6.0 ms with `List` columns and a reused row object (best of five, `clang -O2`).
- **A row filled by a walk** (D212, decided by Claude under D205; the readings below proposed by Claude,
  unconfirmed). A local declared as a `type` with `= null` (`var row: $row_type = null`) and filled on the very next
  line by a plural walk of its own class (`fill_attributes(row, index)`, whose template ranges over that `type`
  with `attribute: Symbol<$row_type>`) is a row when the walk writes out to one: the template's body is the one
  statement `row.attributes[attribute] = <value>`, the value built from `<object>.attributes[attribute]` (read as
  `<object>.<the attribute's name>`), member reads, `[ ]`, the template's other parameters and whole numbers, and
  the call's arguments are the row and names, attribute paths (`matcher.rows`, D221) or number or `Boolean`
  literals. The compiler then writes the literal
  in place of the two lines, one attribute for each attribute of the `type` in its order
  (`{position: columns.position[index], velocity: columns.velocity[index]}`), and every rule above applies to it
  unchanged, the check for a resize starting on the line after the walk. The template is not called, so it is not
  compiled for that walk. A walk of any other shape leaves the local an ordinary `type` value, and storing a
  borrowed item in it is the error for keeping one. Nothing runs for it: the rewrite is done while compiling.
- **A walked row over sparse columns** (D217, decided by Claude under D205; the names `attribute.index` and
  `fits_vector()` provisional under D214, the readings below proposed by Claude, unconfirmed). A walk that fills a
  row may also be written with:
  - **A generic singleton per attribute.** `Column<attribute.class>()` in the line is the singleton made with the
    walked attribute's class, written out as `Column<Position>()` for `position`; its `Vector` attribute is the
    source the item is borrowed from (`'row' borrows items from 'Column<Position>().values' ...`), and the resize
    check follows the singleton's attribute through the call effects as it follows a class's own attribute. A
    singleton made from `attribute.class` is never an attribute, so it may be written where it is used, inline
    included (the D144 and D110 errors leave it out).
  - **The place of the attribute.** `attribute.index` is the attribute's place among those walked, from 0, a
    constant written into the line (`found[attribute.index]` becomes `found[1]`), and it may be read in any
    function a walk of attributes calls. The places are read from a `Vector<Integer>` or `Items<Integer>`, whose
    items are copied (D221), so the vector may be any object's: the runner's own attribute, a walk argument such
    as `matcher.rows`, or `matcher.rows[attribute.index]` read in the line itself. It is read once, when the row is
    made, and no rule of the row reaches it; a runner no longer copies another object's places into its own
    vector first. `benchmarks/matched_rows`: 200 000 entities, two systems, 6.8 ms a tick reading the places
    from the matcher against 8.2 ms copying them into the runner's vector first (best of three rounds of three,
    `clang -O2`). Read elsewhere, it is `'attribute.index' is the attribute's place among
    the attributes walked, so it is read in a function a walk of attributes calls, such as 'fill_attribute(attribute:
    Symbol<Row>, ...)'`. The line's parts are `<value>.attributes[attribute]`, attribute reads, `[ ]`, calls
    (constructions included), the walk's parameters, `attribute.index` and whole numbers; anything else in the line
    of a walk that would borrow is `'row' is a row filled by the walk on the next line, and its line for
    'position' reads 'attribute.index + 1', which a walked row cannot write out: ... so work out 'attribute.index
    + 1' before the walk and pass it in`.
  - **A choice per attribute.** The template's one statement may be an `if` whose conditions are decided while
    compiling for each attribute -- `attribute.class == Entity`, `attribute.class.fits_vector()`,
    `attribute.class.has_function(...)` -- each branch holding one such line; the branch taken for an attribute is
    the line written out for it.
  - **Mixed attributes.** Beside borrowed items, an attribute of a walked row may hold any other value. A
    construction of a class that could be a `Vector` item and holds nothing counted (`Entity(entity)`) is made in
    the frame, never allocated, and is borrowed like the items: it lives exactly as long as the row, and its
    constructor may not use `this` as a value. Any other counted value (a reference read from a reference column,
    a `String`) is counted once when the row is made and let go at the end of the row's block, so a call that
    removes it from its column cannot free it under the row. The resize check covers the borrowed items' vectors
    only. A row written as a literal keeps D206's rule, borrowed items and plain values only.
  - **Cost.** Nothing runs for any of it: the singleton is the one a program would reach anyway, `attribute.index`
    is a constant, the choice is made while compiling, and the frame-made object is a struct in the frame.
    `benchmarks/sparse_rows`: 200 000 entities, two systems, sparse sets for every component, 7.4 ms a tick with
    `Vector` columns and walked rows against 24.0 ms with reference columns and a reused row object (best of five,
    `clang -O2`).
- **Items of an `Items<T>`** (D218, decided by Claude under D205 and D214; the readings below proposed by Claude,
  unconfirmed). An [`Items<T>`](collections.md#itemst--implemented-the-name-provisional) whose `T` fits a
  `Vector` lends its items exactly as a `Vector` does: `items[index]`, `get_at(index)` and the item a template
  visits are borrowed, and every rule above applies to them, rows and walked rows included; `remove_swapping`
  counts as a removal wherever `remove_at` does, on the collection or through the call effects. An `Items<T>`
  whose `T` does not fit lends nothing: its items are counted references, and none of these rules apply to them.
  In a walked row, `Column<attribute.class>().values[found[attribute.index]]` over an `Items` value is therefore a
  borrowed item for a class that fits and a counted value for one that does not, the D217 mix, decided per
  attribute while compiling. Every error about an `Items` item begins with the choice that made it borrowed:
  `'Velocity' fits a Vector, so the items of 'velocities' are borrowed: ...`. Nothing runs for any of it.
- **Arguments a plural fills** (D220, decided by Claude under D205 and D214; the readings below proposed by
  Claude, unconfirmed). A call whose whole argument list is the plural of a template over that function's
  arguments, written as a statement of its own (`system.phase_each(made_arguments(found))`), stands for exactly
  one call, and its results may carry borrowed items into it. For each argument in order, the compiler folds the
  template's body for that argument (an `if` decided while compiling keeps only its branch, as in a walked row)
  and writes it out before the call when what is left is:
  - **one `return` of a walked line**, built as a walked row's line is (`<value>.attributes[...]` aside):
    attribute reads, `[ ]`, calls and constructions, the template's other parameters (read as the plural's
    arguments), `argument.index` and whole numbers, with `Column<argument.class>()` the singleton made with the
    argument's class -- `return Column<argument.class>().values[rows[argument.index]]`. It is a local of the call;
    a borrowed item from a `Vector`, or from an `Items` whose class fits one, is a borrowed name, and the function
    is called through its `___lent_<positions>` copy, in which that parameter is a borrowed name for the call: it
    is not retained or released, and every rule above holds for it (it is not kept, returned, given a second name
    or assigned; it may be lent on to a call, D257). Any other value (`Entity(entity)`, a reference from an `Items` that does not fit) is
    an ordinary local, counted for the call and let go after it.
  - **a walked row declared, filled and returned** -- `var row: $row_type = null`, `fill_attributes(row, rows)`,
    `return row` (or `var row: argument.class = null`) -- which is written out as the walked row it is, under
    every rule above, and passed as a row. Folding `argument.class == $row_type` is what lets a generic
    `Stream<$system_type, $row_type>` fill the one argument of its row type this way
    (`conformance/stage6/streamed_rows`).
  Any other body is the template's ordinary call, as before; a call none of whose arguments borrows is left as
  it was. **The call may not resize what it borrows from**: a call whose call effects may append to or remove
  from a borrowed argument's collection is `'Position' fits a Vector, so the items of 'Column<Position>().values'
  are borrowed: 'position' is borrowed from 'Column<Position>().values' for the one call
  'system.phase_each(made_arguments(found))' stands for, and 'system.update_each()' on line 20 may move the items
  of 'Column<Position>().values': a function handed borrowed arguments may not append to or remove from the
  collections they borrow from, directly or through what it calls` (a row keeps D206's error). **Anywhere else a
  result keeps D204's refusal**: the template called by its single name (`made_position(found)`) is an ordinary
  function, and its `return` of a borrowed item is the error for returning one, which then adds `-- a template
  over the arguments of 'phase_each' may hand back a borrowed item only through its plural, as the whole argument
  list of the one call it fills: 'phase_each(made_arguments(...))' (D220)` (`diagnostics/lent_arguments`).
  **Cost.** The written-out arguments cost what the walked line costs; the template is not called, so it is not
  compiled for that call. `benchmarks/lent_arguments`: 6.9 ms a tick against 34.3 ms copying each argument out and
  back ([optimizations.md](optimizations.md#a-row-of-borrowed-items-lives-in-the-frame);
  `conformance/stage6/lent_arguments`).
- **An item lent to the caller** (D230, decided by Claude under D205; the error texts and the readings below
  proposed by Claude, unconfirmed). A function one of whose `return`s reads an item straight from a singleton's
  storage -- `return column.values[row]` or `return values[index]` (`get_at` too), where the path is attributes
  of the function's class, the object holding the last attribute is a singleton, and that attribute is a `Vector`
  or an `Items` whose class fits one -- **lends its result**: the item is returned uncounted and its caller never
  lets it go. It is decided per class the function is compiled for, so `Lookup<Label>.of` over an `Items` of
  references returns a counted reference as before. **The returns are read per instance with compile-time
  conditions folded** (D259): an `if` whose condition is made only of `$T.fits_vector()`,
  `$T.has_function("name")` with a plain literal name, and a generic parameter compared with a class (`$component_type != Entity`), joined with
  `and`, `or` and `not`, keeps only its taken branch (`function_waits` is left unfolded here, since answering it
  this early would decide it before the functions that ask it are compiled, D209), and a statement after an `if` folded true whose branch returns is dead,
  so it does not count either. `Lookup.of` written as `if $component_type.fits_vector() and $component_type !=
  Entity { return column.values[row] }` followed by `return column.value_at(row)` lends for every instance whose
  class fits and returns an owned value for `Entity` and every class that does not fit, and each caller treats
  the result as its instance does (`conformance/stage6/folded_lends`). At every caller:
  - the result is a borrowed item of `<Singleton>().<attribute>` -- `'layout' is borrowed from
    'Column<Layout>().values'` -- and every rule above applies to it: `var layout = lookup.of(entity)` names it,
    `layout.order = 3` and `lookup.of(entity).order = 3` write the stored item, and it is not kept in an
    attribute, put in a list, given a second name, assigned again, captured in a function value or read past a line whose call effects may append to or remove from that storage, whose error ends
    `... so 'grown' is not read after it: ask for the item again after that line, or keep 'grown.copy()', an
    independent object`;
  - **it is not returned further**: only the read from the singleton's storage lends, so `return lookup.of(0)`, or
    returning a name that holds it, is the error for returning a borrowed item, ending `-- a function lends its
    caller an item only by reading it from a singleton's Items or Vector itself, never one that was lent to it
    (D230)`;
  - a name already used in the block is not reused for it: `'tally' already names a value earlier in this block,
    and an item borrowed from 'Column<Tally>().values' is given a name of its own: call it something new, such as
    'var tally_item = ...'` (this applies to every borrowed item);
  - **narrowing keeps it borrowed** (D259, a bug fix under D244): `if layout { ... }`, `if layout and
    layout.order > 0 { ... }` make it a plain `Layout` inside the block that is still the borrowed item, so passing it to a call lends it (D257) and keeping, listing or returning it is the
    error above. Before, the narrowed name was an ordinary local: a call retained and released the stored item,
    which is a double free under `--debug-memory` and heap corruption in production
    (`conformance/stage6/narrowed_lends`, `diagnostics/lent_escapes`);
  - **it is called by name only** (D259): a lending function is never made a function value (`var find =
    lookup.of`, `entities.map(lookup.of)`), whose caller would let go of an item it does not own -- `'of' lends its
    caller an item of 'Column<Justify>().values' (D230), which only a call written by name can borrow: a function
    value's caller would let go of an item it does not own, so 'of' is not made a function value -- call 'of(...)'
    by name where the item is wanted` -- and its class does not fit a `type` requiring that function: `'Lookup'
    does not fit type 'Finder': its 'of' lends its caller an item of 'Column<Justify>().values' (D230), which only
    a call written by name can borrow: a call through the type would let go of an item it does not own`.
  A function that lends returns only such items or `null`: any other value would be an object the caller never
  lets go, so it is `'at_or_made' lends its caller an item of 'Scores().values', which the caller writes in place
  and never lets go, so each of its returns is an item of 'Scores().values' or null: 'made' is neither -- return
  null for 'none', with a '?' result, or make it a function of its own`. A storage path through a local, a
  parameter or an object that is not a singleton keeps D204's refusal, since that storage may not outlive the
  call. **Cost**: none -- the return is the item's address, with no retain, and the caller releases nothing
  (`conformance/stage6/lent_results`, `diagnostics/lent_results`, `conformance/stage6/folded_lends`,
  `conformance/stage6/narrowed_lends`, `diagnostics/lent_escapes`).
- **An item lent to a call** (D257, decided by Claude under D205 and D244, the argument-side twin of D220 and
  D230; the error texts and the readings below proposed by Claude, unconfirmed). A borrowed item passed as an
  argument -- a borrowed name (`mouse`, including one lent to the function itself), an item read with `[ ]`
  (`mice[0]`) or a row's attribute (`device.mouse`) -- at a place whose parameter is not a union or a variadic
  list, to a function of the program called by name, is **lent** to that call. The compiler writes a second copy
  of the function, `<name>___lent_<positions>` (the D206 mechanism, shared with rows and with D220's arguments), in
  which that parameter is a borrowed name for the call: it is never retained or released, reading and writing it
  reads and writes the caller's item, and passing it on to another call lends it again, to that function's own
  `___lent_` copy. Every caller that passes a class instance keeps the ordinary function.
  - **The function keeps nothing.** Every rule above holds for the lent parameter inside the copy, so each way of
    keeping it is an error in that function naming the parameter and the call that lent it (`'mouse' is lent to
    'keep' for one call (D257), by the call on line 9 of 'LentToCalls': it is the caller's item, read and written
    in place,` followed by the site):
    - kept in an attribute: `... so 'keep' does not keep it in the attribute 'last': keep the values it needs, or
      have the caller pass something 'keep' may keep`;
    - put in a list: `... so 'list' does not put it in a list, which would keep it: keep the values it needs`;
    - returned: `... so 'hand_back' does not return it: return the values the caller needs`;
    - made into a function value: `'mouse.move' would keep 'mouse' in a function value, and 'mouse' is lent to
      'capture' for one call (D257), so 'capture' does not keep it: call 'mouse.move()' directly, or keep the
      values it needs`;
    - given a second name: `... and a lent item has one name: use 'mouse' itself instead of 'alias'`.
    These are the facts escape analysis proves per parameter ([optimizations.md](optimizations.md)) -- the function
    keeps nothing its parameter reaches -- checked here by compiling the function under the borrow rules, which
    names the very line that would keep it.
  - **The call may not resize what the item is borrowed from.** A statement that lends an item is checked with
    the call effects: a call whose `grow:` or `shrink:` facts reach the collection the item is borrowed from (for
    a row's attribute, each collection the row borrows from) is `'mouse' is borrowed from 'mice' and lent to
    'grow' for its call, and 'input.grow()' on line 19 may move the items of 'mice': a function lent a borrowed
    item may not append to or remove from the collection it is borrowed from, directly or through what it calls
    -- change the size of 'mice' before or after the call (D257)`. An item lent on from inside a lent function is
    covered by the check at the call that first lent it, whose call effects include everything the function
    reaches. D220's plural calls keep their own error.
  - **Where it is not lent, the error says why, and never offers a copy the function would write** (D244: a write
    to a copy vanishes silently). Passing to a library function: `'first' is borrowed from 'other', and 'append'
    is a library function, which is never lent an item: pass the attributes it needs, or 'first.copy()' where a
    snapshot is what it wants`; to a union parameter: `... and 'f' takes it as a union, which holds its own values
    and is never lent an item: pass the attributes it needs, or give 'f' a parameter of the item's own class`;
    any other expression (such as a call that lends its result, written inside the argument): `... only a named
    item, an item read with '[ ]' or a row's attribute is lent to a call (D257): name it first, 'var item =
    ...', and pass 'item'`.
  - **Cost.** None: the argument is the item's address, the parameter is never counted, and nothing is copied.
    The `___lent_` copy is compiled only for the positions a program lends, and a program that lends nothing
    carries none (tree-shaken like any function). `conformance/stage6/lent_to_calls` (an input system handing a
    walked row's mouse and keyboard to `apply(event, mouse, keyboard)`, which writes them and passes them on,
    with balanced memory and the writes read back from the vectors), `diagnostics/lent_to_calls`.
- **A walked line names the attribute's class wherever it was declared** (proposed by Claude, unconfirmed; a bug
  fix found making every fitting component inline in SlopEngine). `Column<attribute.class>()` in a walked row or
  a plural's argument is written out with the class's full name, so a row type declared in another namespace
  (`mouse: Component.Mouse` in `Input`) names the same class from the runner's file.

`diagnostics/vector_borrows`, `diagnostics/vector_items`, `diagnostics/vector_rows`, `conformance/stage6/vector_items`,
`conformance/stage6/vector_rows`, `conformance/stage6/walked_rows`, `diagnostics/walked_rows`,
`conformance/stage6/sparse_rows`, `diagnostics/sparse_rows`, `conformance/stage6/items_columns`,
`diagnostics/items_borrows`, `conformance/stage6/lent_arguments`, `conformance/stage6/streamed_rows`,
`diagnostics/lent_arguments`, `conformance/stage6/lent_to_calls`, `diagnostics/lent_to_calls`.
