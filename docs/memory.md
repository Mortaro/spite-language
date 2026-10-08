# Memory

Spite uses **reference counting**, JavaScript-like. A scalar (a number, a `Boolean`, an enum value) is a plain
value, copied wherever it goes. Everything else (a class instance, `List<T>`, `Dictionary<T>`, `String`, a
union, an object literal) is a **reference**: assigning it, passing it, storing it in a field, a list or a
dictionary, and returning it all share the exact same object. There is no reference syntax to write: a reference
is the default. When the last reference to an object goes (a scope ends, a field is overwritten, an element is
removed), the object's `drop()` runs, if it has one, its own references are released, and it is freed. There is
no garbage collector and no pause.

What that costs while the program runs is a count in every object and one addition or subtraction each time a
reference is kept or let go (plain arithmetic, atomic only in a program that starts a thread) and nothing
else: no collector, no runtime to ship. A singleton is not counted at all, and a number, a `Boolean` or an enum
value is never an object.

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

### A name read from a list names the item, until you assign it

`var box = boxes[0]` names the box the list holds, so `box.label = "b"` changes the box in the list. Assigning
`box` is different: `box = Box("c")` makes the name hold another box and leaves the list holding the first one.
When nothing reads `box` after that, the assignment was meant for the list, and the compiler says so:

```gdscript
var boxes = List<Box>()

func relabel(replacement: Box) {
    crash boxes[0]
    var box = boxes[0]
    box.label = "b"
    box = replacement
}
```

```
'box' was read from 'boxes[0]', so assigning it here changes only 'box', and nothing reads 'box' after:
'boxes' still holds the old item. Write 'boxes[0] = replacement' to replace the item, or give the new value a
'var' of its own
```

The same holds for a name answered by a function that returns an item of a list, such as a lookup that answers
`values[row]`: the error names the class holding the list and its function that writes it
([the rules](../specs/memory.md#assigning-a-name-read-from-a-list-changes-only-the-name)).

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

`deep_copy()` copies the shape of what it reaches: an object reached twice in one copy, through a cycle or a
`Weak`, is copied once, so a child's `parent` in the copy is the copied parent, and a `Weak` in the copy holds the
copy of its object when the same `deep_copy()` copied it ([the rules](../specs/memory.md#the-memory-model)).

## A `Vector` lends its items

A `Vector<T>` is the one place a value is not a counted reference: it holds its items inline, one block of their
attributes with no header ([collections.md](collections.md#vectort-items-inline)), and `velocities[index]` is the
item inside that block, **borrowed**. Writing its attributes writes the vector's item; taking it and letting it go
costs nothing. In exchange the compiler never lets it be kept (in an attribute, a list, a returned value, a
function value, or past a line that may grow or shrink the vector), and each of those errors names `copy()`,
which makes an independent object ([the rules](../specs/memory.md#borrowed-items-of-a-vectort)).

Only a class is lent: numbers, `Boolean`s and enums are kept in a `List`, never a `Vector`, and reading one gives
a number of its own. `velocities[index]`, like every `[]`, answers a `T?`, so an item is lent once the read is
narrowed, by `crash velocities[index]` or by a proof such as the loop bound `index < velocities.count()`.

### A row of borrowed items, for one call

An engine keeps each component in its own `Vector` and hands a system one entity's components at a time. The
row is an object literal of borrowed items, and the system takes it as a `type`:

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
    crash positions[0] and velocities[0]
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
([the rules](../specs/memory.md#borrowed-items-of-a-vectort)).

A generic runner does not know the attributes of the `type` it is given, so it cannot write the literal. It fills
the row with a `Symbol` walk instead, and a local of the `type` declared `= null` and filled by the walk on the
next line is a row exactly like the literal:

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
    crash columns.attributes[attribute][index]
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
    crash columns.position[0]
    console.print(columns.position[0].left)
}
```
```output
5
```

Every `[]` answers a `T?` and a row's attributes are items, so the template states each
read with a `crash` line before the fill: nothing the walk knows proves that `index` is inside each column. The
compiler writes the walk out where it is called: for `Runner<Moving>` the two lines become `crash
columns.position[index]`, `crash columns.velocity[index]` and `var row: Moving = {position:
columns.position[index], velocity: columns.velocity[index]}`, where each `crash` line's read is made once and
the literal uses it, so the row costs what the literal costs, follows the same rules, and `fill_attribute` is
never called.

An engine that adds and removes components all the time keeps each one in a **sparse set**: a generic singleton
`Column<Position>` whose `Vector` is packed, so each entity sits at a different place in each column. The runner
works out those places first, one per attribute, and the walk's line reads each attribute from its own column at
its own place: `Column<attribute.class>().values[stored_row]`, after `var stored_row = found[attribute.index]`,
where `attribute.index` is the attribute's place in the `type`, with one `crash` line for the
place and one for the item (an index never holds another `[]` read).
A generic singleton made with no arguments is the same object every time, so `Column<Position>()` starts a path
that `crash` narrows like a name. The places are numbers in a `List`, so they may come from any `List<Integer>`:
the runner's own, or another object's, `fill_attributes(row, matcher.rows)`. A class that cannot be a `Vector`
item stays a reference in a `List`, `attribute.class.fits_vector()` choosing while compiling, and the row may hold
it beside the borrowed items, as it may hold an `Entity` made from the id:

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

var found = List<Integer>()

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

func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, rows: List<Integer>, entity: Integer) {
    if attribute.class == Entity {
        row.attributes[attribute] = Entity(entity)
    } else if attribute.class.fits_vector() {
        crash rows[attribute.index]
        var stored_row = rows[attribute.index]
        crash Column<attribute.class>().values[stored_row]
        row.attributes[attribute] = Column<attribute.class>().values[stored_row]
    } else {
        crash rows[attribute.index]
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
item, and the `Trail` is counted once for the row and let go after the call, unless nothing the call runs can
let go of it, when `at` lends it to the row uncounted ([the rules](../specs/memory.md#borrowed-items-of-a-vectort)). Only `Position`
and `Velocity` are borrowed, so only their vectors are checked for a resize during the call.

Two column classes, and a choice in every line that reaches them, are only there because a `Vector<Trail>` cannot
be made. An [`Items<T>`](collections.md#itemst-the-storage-chosen-for-you) makes that choice itself, inline when
the class fits and by reference when it does not, so one `Column<$component_type>` holding
`var values = Items<$component_type>()` serves every component, and the fill template needs no `fits_vector()`:

```gdscript
func fill_attribute(attribute: Symbol<$row_type>, row: $row_type, rows: List<Integer>, entity: Integer) {
    if attribute.class == Entity {
        row.attributes[attribute] = Entity(entity)
    } else {
        crash rows[attribute.index]
        var stored_row = rows[attribute.index]
        crash Column<attribute.class>().values[stored_row]
        row.attributes[attribute] = Column<attribute.class>().values[stored_row]
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
call's whole argument list ([metaprogramming.md](metaprogramming.md#walking-a-programs-structure)).
The template's line is the walked row's line, read per argument: `argument.index` is the argument's place, so
`Column<argument.class>().values[stored_row]`, after `var stored_row = rows[argument.index]`, is that component's
item. The plural stands for exactly
one call the compiler writes, so its results may be borrowed items for that call and no longer:

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
var found = List<Integer>()

func run(index: Integer) {
    found.clear()
    found.append(index)
    found.append(index)
    run_phases_each()
}

func run_phase_each(phase: Symbol<$system_type.phase_each>) {
    system.phase_each(made_arguments(found))
}

func made_argument(argument: Symbol<$system_type.phase_each>, rows: List<Integer>): argument.class {
    crash rows[argument.index]
    var stored_row = rows[argument.index]
    crash Column<argument.class>().values[stored_row]
    return Column<argument.class>().values[stored_row]
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
    crash positions.values[0]
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
([the rules](../specs/memory.md#borrowed-items-of-a-vectort)).

### An item lent to the caller

A singleton lives until the program ends, so an item of its `Items` or `Vector` can be handed to a caller: a
function that returns `column.values[row]`, read straight from a singleton's storage, **lends** the stored item,
and the caller writes it in place:

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
    crash layouts.values[0]
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

A borrowed item (a name read from a `Vector` or `Items`, `velocities[index]` itself, a row's attribute, an item
lent to the caller) may be passed as an ordinary argument. It is **lent for the call**: the function reads and
writes the caller's own item and may lend it on to the functions it calls, and when the call returns the borrow is
the caller's again:

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
    crash mice[0]
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
copy and vanish. What the function may not do is keep the item past the call: store it in an attribute or a
list, return it, or make a function value of it. Each of those is an error in the function, naming the call
that lent it: `'mouse' is lent to 'keep' for one call, by the call on line 9 of 'LentToCalls': it is the
caller's item, read and written in place, so 'keep' does not keep it in the attribute 'last': keep the values it
needs, ...`. Nothing the call reaches may append to or remove from the collection the item is borrowed from,
since that could move it while the function holds it.

## `drop()` runs once, right before the object is freed

A class may define a zero-argument `func drop() { ... }` for cleanup (closing a handle, clearing a
back-reference). The compiler calls it automatically the moment the last reference goes away, never by name:

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
neither is ever freed. The back reference is written with `Weak<T>` instead: it holds a
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
are done with it ([the rules](../specs/memory.md#the-memory-model)).

## `--debug-memory`

`spite program --debug-memory` builds with an allocation table and prints `allocations: N frees: N` right before
the program exits. A mismatch means something leaked; when the two do not balance, it also prints a
**leaked-object summary by class name**, naming which classes' instances are still alive, which is what makes a
leaked cycle visible instead of an unexplained count. Every program on these pages is run this way, and must
balance. It also fills every byte it hands out that nobody has written yet with the same pattern, so a program that
reads memory it never wrote fails the same way on every run and from every launcher, instead of reading whatever
the heap held. The table exists only in a `--debug-memory` build ([what it records](../specs/memory.md#--debug-memory)).

## `Memory` is the floor, and you can build on it

Memory is its own namespace, because it is the most basic thing a program has and the most dangerous. It holds two kinds of class:

- **`Memory.Address` is a place in memory** (`library/memory/address.spite`). It is a number, eight bytes, kept
  in a register like a `Long` and cast to and from one by the ordinary casting rule, so `address + 16` is the
  address sixteen bytes on. What makes it an address is what it answers: `read_long(offset)`,
  `write_float(offset, value)` and the rest read and write the value at `address + offset`.
- **An allocator is who owns memory**: `Memory.Heap()` is the one every object uses unless told otherwise.
  It hands out addresses and takes them back; `Memory.Arena` is another
  ([below](#choosing-an-allocator-memoryallocator)).

Every type in the standard library is Spite over these two: `String`, `List<T>` and `Dictionary<T>` keep their
bytes in memory the heap hands out, and each number says in its own file how much memory it is. A container of
your own is written over the heap and `TypedMemory<T>` (below), with nothing the compiler does for `List<T>`
that it would not do for yours: read `library/list.spite` for a complete one. The rules of this floor are in
[the rules](../specs/memory.md#the-floor-memoryaddress-memoryheap-and-typedmemoryt).

Bytes that come from outside are not this floor: a file's or a socket's bytes are a `List<Byte>`, which reads the
numbers in it by position, and memory a C library hands out is a `ForeignBytes`, which checks every read and write
against the size it was given ([standard_library.md](standard_library.md#numbers-in-bytes)).

### Where `String` and `Integer` keep their memory

A type's storage is attributes at the top of its file. `library/string.spite` starts with the memory a
`String` is:

```gdscript
var _bytes: Memory.Address = 0
var _length: Long = 0
```

Sixteen bytes, kept wherever the `String` is (in a local, an attribute, a list's element) and never an object
of their own. `_bytes` is where its characters are, with a 0 after the last one for C, and `_length` is how many
there are. Where the characters live is the compiler's choice: text of up to 15 bytes is
kept in those sixteen bytes themselves, so it allocates nothing and is never counted; longer text is one block the
heap hands out, with its reference count and capacity in front of the characters; and a written text (`"hello"`)
points at the characters the program already carries, which are never counted or freed. Everything else is Spite
in the same file: `length()` answers `_length`, `equals` and `less_than` compare `_bytes` with `compare_bytes`, and
`slice` ends in `_bytes.text(length)`. What stays C is what depends on where the characters are: reading one
(`code_at`, which answers the 0 after the last one past the end, and is one load in a loop over the text), making
text from bytes (`Memory.Address.text`), joining two texts (`sum`), counting a block's references, and growing text
in place, which fills the sixteen bytes first and moves the text into a block once it passes 15 bytes, and only
when nothing else holds that block.

`library/integer.spite` starts with the size of an `Integer`:

```gdscript
var _memory = Memory.Bytes(4)
```

Four bytes, and nothing is allocated: `Memory.Bytes(4)` says only how big the value is, which is true wherever it
lives. The compiler places it (a number's memory is a register, or wherever the C compiler keeps an `int32_t`, a
local on the stack, an attribute inside an object, or the value of a `type` beside its class's tag), so there is no
address behind `this` and nothing to free. `Memory.Bytes` is not a class and is not called: it is how a value
class states its size, and it is read only there. Every number file says the same with its own width (`Long` 8, `Short` 2,
`Byte` 1, `Boolean` 1, `Double` 8, `Memory.Address` 8, ...), the compiler checks it against the C type it emits,
and `count.memory.bytes` reads it. Inside a number, `this` is the value itself, not `_memory`.

What a container of your own works with:

- **Allocating:** `heap.allocate(bytes)` returns a `Memory.Address`, `heap.resize(address, bytes)` grows it and
  `heap.free(address)` gives it back, from your `drop()`. The bytes are not cleared, so write before you read.
- **Where it lives is the compiler's choice** ([placement](../specs/memory.md#placement-the-compiler-decides-where-memory-lives)):
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
(`library/spite/memory.spite`) with a read-only `address`, `bytes` and `section`: `'heap'`, `'stack'` or
`'constant'` (the text of a literal, which is part of the program). A class instance or a list answers with
its object; a `String` with its characters; a number held in a local with the local itself. It is built only
where a program reads it, so it costs nothing anywhere else ([the rule](../specs/memory.md#where-a-value-lives-memory)).

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
the object is made:

```gdscript
var spark = Particle("spark", 1.5)
spark.memory.allocator = arena
```

The compiler reads the two lines as one intent: `spark` is made in `arena` from the start. Nothing is allocated
on the heap first and moved, and nothing is looked up while the program runs: the constructor is handed the
arena's memory instead of the heap's. The standard library's allocator besides the heap is
**`Memory.Arena(block_bytes)`**, which hands out memory from blocks of that size, one after another, never gives
any of it back one piece at a time, and frees every block at once when the arena itself goes. An object made in
an arena holds the arena, so the arena cannot go while anything made in it is alive.

- **Only right after it is made.** A constructor, `List<T>()` or `.copy()` on one line, and the allocator on the
  next. To move an object that was already used, copy it and set the copy's allocator, as `kept` does below.
- **Only an object.** A number or a `String` is placed by the compiler, not by an allocator.
- **A list holds references**: giving a `List` an allocator places the list there, and its buffer of
  references too, while each element lives wherever it was made. A `Vector` holds its items inline
  ([collections.md](collections.md#vectort-items-inline)); giving one an allocator places the vector object and
  its block of items.
- **It costs nothing where it is not used**: only a class some line gives an allocator
  grows, by sixteen bytes per object.

The exact rules and the errors are in [the rules](../specs/memory.md#allocators-memoryallocator).

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

### An object reads its own allocator

`value.memory.allocator` answers where an object was made: `Memory.Heap` unless a line said otherwise.
Inside a class, the object a function runs on reads its own as `memory.allocator`, and that is how a `List`
keeps its buffer in the list's arena: `library/list.spite` grows its buffer with
`memory.allocator.allocate(bytes)` and gives the old one back with `memory.allocator.free(address)`. The answer
is one of the program's allocators, so `== Memory.Arena` asks which, as for a
[union](values_and_types.md#unions).

```gdscript title=arena_scores/arena_scores.spite entry
var console = Console()
var heap = Memory.Heap()

func ArenaScores() {
    var arena = Memory.Arena(4096)
    var before = heap.live_allocations()
    var scores = List<Integer>()
    scores.memory.allocator = arena
    var score = 0
    while score < 100 {
        scores.append(score)
        score = score + 1
    }
    var during = heap.live_allocations()
    var in_arena = scores.memory.allocator == Memory.Arena
    console.print("heap allocations:", during - before, "in the arena:", in_arena)
    var plain = List<Integer>()
    plain.append(1)
    var on_heap = plain.memory.allocator == Memory.Heap
    console.print("plain list on the heap:", on_heap)
}
```
```output
heap allocations: 1 in the arena: true
plain list on the heap: true
```

The list grew six times, and every buffer came from the arena's one block. An arena never gives memory back
one piece at a time, so the smaller buffers a list grew out of stay in the arena until the arena goes.

---

Next: [Metaprogramming](metaprogramming.md), writing code that writes code for every attribute of a class.
