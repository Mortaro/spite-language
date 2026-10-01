# Lists, dictionaries and member templates

`List<T>` and `Dictionary<T>` are not built into the compiler. They are ordinary generic classes written in Spite
(`library/list.spite` and `library/dictionary.spite`) over `Memory.Heap` and `Memory.Address`, the floor
everything else is built on. The compiler keeps only the syntax: `[1, 2, 3]`, `list[index]`, and `List<T>()`. Read
those two files to see exactly what a list does; write your own container the same way
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).

Being ordinary classes, they cost what their code costs and nothing more: a list is one heap
buffer of its elements, a program compiles only the members it calls, and one that never makes a `Dictionary`
carries none of it. Nothing runs behind them: no collector, no iterator objects, no registry of templates.

## `List<T>`

A list literal is written with commas on one line, or one entry per line with no commas:

```gdscript
var primes = [2, 3, 5, 7]
var names = [
    "ada"
    "grace"
]
var empty = List<String>()
```

| Member | Result | Notes |
|---|---|---|
| `append(value)` / `prepend(value)` | | adds to the end / to the front (index 0) |
| `insert(index, value)` | | an index out of range is clamped to the nearest end |
| `list[index]` | `T?` | may not be there, so it is narrowed ([failure.md](failure.md#reading-with--answers-t)) |
| `list[index] = value` | | the explicit form is `set_at(index, value)`; an index out of range halts, naming the line |
| `get_at(index)` | `T?` | what `list[index]` calls, so it answers the same `T?`, `null` out of range |
| `remove_at(index)` | | an index out of range halts |
| `remove_swapping(index)` | | moves the last element into `index` instead of moving every later one down ; an index out of range halts |
| `remove_where(test)` / `remove_where_<member>()` | | removes every element the test is true for, in one pass, keeping the rest in order ([below](#removing-many-at-once)) |
| `truncate(count)` | | keeps the first `count` elements and releases the rest; nothing happens when `count` is out of range |
| `swap(first, second)` | | exchanges two elements; nothing happens when either is out of range |
| `reserve(count)` | | makes room for `count` elements in all without adding any, so appending up to there never grows the buffer |
| `remove_first()` / `remove_last()` | `T?` | removes and returns it; `null` when empty, like `first()` |
| `first()` / `last()` | `T?` | `null` when empty, like `[]`: narrow it, `crash first` or `if first { }` |
| `count()` / `is_empty()` | `Integer` / `Boolean` | `count()` is only ever the list's size |
| `contains(value)` | `Boolean` | elements that are numbers, `Boolean`, `String` or an enum only |
| `clear()` / `reverse()` | | in place; `clear()` keeps the buffer's capacity |
| `join(separator)` | `String` | every element becomes text: a `String`, a number, a `Boolean`, an enum value |
| `copy()` / `deep_copy()` | `List<T>` | one level, or all the way down ([memory.md](memory.md)) |

`append`, `remove_last`, `list[index]` and their kin take the same time however long the list is; `prepend`,
`insert`, `remove_first` and `remove_at` move every later element, and `contains` looks at each in turn. An empty
list has no buffer; the first element gets one of four slots, and a full buffer is resized to twice its size.

Names say where: `append` and `prepend`, never `add`; `remove_last`, never `pop`
([the rule](#listt-additions)).

## `Dictionary<T>`

Keys are text or whole numbers, entries keep the order they were inserted in, and `get`, `has`, `set` and `[]`
take the same time however many keys there are, because it is a hash table over two lists. `remove` is the exception: it
moves every later entry down, as `remove_at` does on a list. There is nothing to write for the key: a dictionary
given text keys is keyed by text, and one given whole numbers is keyed by numbers ([below](#keyed-by-numbers)).

| Member | Result | Notes |
|---|---|---|
| `Dictionary<T>()` | | an empty one, with no table until the first key |
| `set(key, value)` / `dictionary[key] = value` | | replaces the value of a key already there |
| `get(key)` / `dictionary[key]` | `T?` | `null` when the key is absent |
| `has(key)` | `Boolean` | |
| `remove(key)` | | nothing happens when the key is absent |
| `count()` | `Integer` | |
| `keys()` / `values()` | `List<String>` (or the numbers) / `List<T>` | a fresh list, in insertion order |
| `copy()` / `deep_copy()` | `Dictionary<T>` | |

```gdscript title=dictionary_tasks/dictionary_tasks.spite entry
var console = Console()

func DictionaryTasks() {
    var inventory = Dictionary<Integer>()
    inventory.set("sword", 1)
    inventory.set("potion", 4)
    inventory.set("potion", 6)
    var inventory_count = inventory.count()
    console.print("count", inventory_count)
    var has_shield = inventory.has("shield")
    console.print("has shield", has_shield)
    crash inventory["potion"]
    console.print("potions", inventory["potion"])
    console.print("shields", inventory["shield"] == 0)
    var total = 0
    var values = inventory.values()
    var index = 0
    while index < values.count() {
        total = total + values[index]
        index = index + 1
    }
    console.print("total items", total)
}
```
```output
count 2
has shield false
potions 6
shields false
total items 7
```

`inventory["shield"]` is `null`, and `null` equals nothing, so `inventory["shield"] == 0` is `false`: absent is
not zero. `has(key)` asks the question directly.

### Keyed by numbers

Give a dictionary whole numbers (`Integer`, `Long` or any other whole-number type) and it is keyed by them: it
stores and hashes the number itself, never text made from it, and `keys()` answers the numbers. Which kind a
dictionary is keyed by is worked out while compiling from every key the program gives it, wherever the dictionary
goes: a parameter, an attribute or a generic class that receives it is keyed the same way.

```gdscript title=number_keys_tasks/number_keys_tasks.spite entry
var console = Console()
var names = Dictionary<String>()

func NumberKeysTasks() {
    names[3] = "fern"
    names[11] = "moss"
    names.set(7, "reed")
    names.remove(11)
    crash names[7]
    console.print("seven", names[7])
    var ids = names.keys()
    crash ids[0]
    var first_id = ids[0] + 100
    var joined_ids = ids.join(",")
    console.print("ids", joined_ids, "first plus 100", first_id)
}
```
```output
seven reed
ids 3,7 first plus 100 103
```

One dictionary is keyed by one kind: given a text key in one place and a number in another, even through a
parameter, it is a compile error naming both places. A number in a key is not turned into text; to key by text, give
text.

## `Vector<T>`: items inline

A `List` of objects holds references: each element is an object somewhere on the heap, and walking the list
follows one pointer per element. A `Vector<T>` (`library/vector.spite`) holds objects themselves, one after another
in one block of memory, the way the processor's cache likes to read them. An item has no header and no reference count of its own; appending one copies its
attributes into the block. Numbers, `Boolean`, enums and `Memory.Address` are already flat in a `List`, so a list
of them is always a `List`: `Vector<Integer>` is an error naming `List<Integer>`.

```gdscript title=vector_basics/velocity.spite
var across = 0.0
var down = 0.0
var moving = false

func Velocity(new_across: Float, new_down: Float) {
    across = new_across
    down = new_down
}

func integrate() {
    across = across + down
    moving = true
}
```
```gdscript title=vector_basics/vector_basics.spite entry
var console = Console()

func VectorBasics() {
    var velocities = Vector<Velocity>()
    var slow = Velocity(1.0, 0.5)
    velocities.append(slow)
    var fast = Velocity(2.0, 3.0)
    velocities.append(fast)
    crash velocities[0]
    var first = velocities[0]
    first.down = 2.0
    velocities.each_integrate()
    var total = velocities.sum_across()
    var moving_count = velocities.count_moving()
    console.print(total, moving_count, slow.down)
}
```
```output
8 2 0.5
```

`velocities[0]` answers a `Velocity?`, like every `[]`: `null` when there is no item
there. `crash velocities[0]` narrows it, and then it is not a copy: it is the item inside the vector,
**borrowed**, so `first.down = 2.0` writes the vector's own item. `slow` is still `0.5`, because `append` copied it
in. The member
templates run on the items in place the same way, and a chain of them is one loop, as on a list.

| Member | Result | Notes |
|---|---|---|
| `append(value)` | | copies `value`'s attributes in as a new last item |
| `vector[index]` / `get_at(index)` | `T?` | the item itself, borrowed once narrowed; `null` out of range |
| `vector[index] = value` / `set_at(index, value)` | | copies `value` over the item; out of range halts |
| `remove_at(index)` | | moves every later item down; nothing happens out of range |
| `remove_where_<member>()` / `truncate(count)` / `swap(first, second)` | | as on a list ([below](#removing-many-at-once)); `remove_where(f)` is a `List`'s only |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `reserve(count)` | | makes room for `count` items in all without making any, so appending up to there never grows the block |
| `copy()` / `deep_copy()` | `Vector<T>` | a new vector with its own copy of every item |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()` | | as on a list; `filter_` gives a `Vector<T>` of copies |
| `parallel_each_<member>()` | | as on a list ([concurrency.md](concurrency.md#parallel_each_-a-member-on-every-element)) |

An item is a class of known size: its attributes are only numbers, `Boolean`, enums and `String`s. A class with a
`List`, a `Dictionary` or another object in an attribute is an error naming the attribute, and it belongs in a
`List` ([the rules](#vectort)). A `String` may be an item too, held as a counted reference.

A borrowed item is read and written through its members. It is never kept: not in an attribute, a list, a
returned value or a function value, and not past anything that may change the vector's size or move its block,
since growing the vector (`reserve(count)` too, which adds no item) may move its items. Each of those is an error
that names `copy()`, which makes an independent object:

```gdscript title=vector_borrow_mistake/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=vector_borrow_mistake/vector_borrow_mistake.spite entry error
var console = Console()

func VectorBorrowMistake() {
    var velocities = Vector<Velocity>()
    var slow = Velocity(1.0)
    velocities.append(slow)
    var first = velocities[0]
    velocities.append(slow)
    console.print(first.across)
}
```
```diagnostic
may move the items of 'velocities', so 'first' is not read after it
```

The rules for what a borrowed item may do, and what they cost, are in
[memory.md](memory.md#borrowed-items-of-a-vectort). Items of several vectors can travel together as
a **row**, an object literal a system takes as a `type` for one call, which is how an engine joins its component
columns ([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)).

## `Items<T>`: the storage chosen for you

A generic class that keeps values of a type it does not know (an engine's `Column<$component_type>`) cannot
say `Vector` or `List` without knowing whether the type fits a `Vector`. `Items<T>` (`library/items.spite`)
answers that while compiling: when `T.is_fixed_size` its items are
inline and borrowed, exactly as a `Vector`'s; otherwise they are references, as a `List`'s. The members are the
same either way, so one class serves both:

```gdscript title=items_basics/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=items_basics/trail.spite
var lefts = List<Float>()
var last = 0.0

func mark(left: Float) {
    lefts.append(left)
    last = left
}
```
```gdscript title=items_basics/items_basics.spite entry
var console = Console()

func ItemsBasics() {
    var velocities = Items<Velocity>()
    var slow = Velocity(1.0)
    velocities.append(slow)
    var fast = Velocity(4.0)
    velocities.append(fast)
    crash velocities[0]
    var first = velocities[0]
    first.across = 2.0
    var trails = Items<Trail>()
    var trail = Trail()
    trails.append(trail)
    crash trails[0]
    var kept = trails[0]
    kept.mark(3.5)
    velocities.remove_swapping(0)
    var total = velocities.sum_across()
    var marks = trail.lefts.count()
    console.print(slow.across, total, trail.last, marks)
}
```
```output
1 4 3.5 1
```

`Velocity` fits a `Vector`, so `velocities[0]`, once `crash velocities[0]` has narrowed it, is the item inside
the block, borrowed, and `slow` keeps its own `1.0`. `Trail` holds a `List`, so it does not fit, and `trails[0]` is `trail` itself, a counted reference like a
list's element: writing through it writes `trail`. `remove_swapping(0)` moves the last item into the hole instead
of moving every later one down, which is what a sparse set wants.

| Member | Result | Notes |
|---|---|---|
| `append(value)` | | inline: copies `value`'s attributes in; references: keeps `value` |
| `items[index]` / `get_at(index)` | `T?` | inline: the item, borrowed once narrowed; references: the reference; `null` out of range, either way |
| `items[index] = value` / `set_at(index, value)` | | replaces the item; out of range halts |
| `remove_at(index)` | | moves every later item down; nothing happens out of range |
| `remove_swapping(index)` | | moves the last item into `index`; nothing happens out of range |
| `remove_where_<member>()` / `truncate(count)` / `swap(first, second)` | | as on a list ([below](#removing-many-at-once)); `remove_where(f)` is a `List`'s only |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `copy()` / `deep_copy()` | `Items<T>` | inline: every item copied; references: one level, or all the way down |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()`, `parallel_each_<member>()` | | as on a vector; `filter_` gives an `Items<T>`, and a chain is one loop |

Where the items are inline every rule of a borrowed item applies, and the error says why the item is borrowed,
so a class that starts to fit a `Vector` shows where its old uses keep an item:

```gdscript title=items_borrow_mistake/velocity.spite
var across = 0.0

func Velocity(new_across: Float) {
    across = new_across
}
```
```gdscript title=items_borrow_mistake/items_borrow_mistake.spite entry error
var console = Console()

func ItemsBorrowMistake() {
    var velocities = Items<Velocity>()
    var slow = Velocity(1.0)
    velocities.append(slow)
    var first = velocities[0]
    velocities.remove_swapping(0)
    console.print(first.across)
}
```
```diagnostic
'Velocity' fits a Vector, so the items of 'velocities' are borrowed: 'first' is borrowed from 'velocities'
```

An engine's column holds its values in an `Items`, and the walked row reads them the same way for both kinds
([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)). The rules are [below](#itemst).

## Removing many at once

`remove_where(test)` removes every element a function is true for, and `remove_where_<member>()` every element
whose member is true, in **one pass** that keeps the others in their order: a `List`, a `Vector` and an `Items`
all have both (the passed-function form on a `List` only). `truncate(count)` keeps the first `count` and lets go of
the rest, and `swap(first, second)` exchanges two, which is what a pass of your own is written with when the test
is not a function of the element, such as an engine removing a column's rows by a mask of despawned entities:

```gdscript title=bulk_removal_doc/velocity.spite
var across = 0.0
var stopped = false

func Velocity(new_across: Float) {
    across = new_across
    stopped = new_across == 0.0
}
```
```gdscript title=bulk_removal_doc/bulk_removal_doc.spite entry
var console = Console()
var despawned = [false, true, false, true, false]

func BulkRemovalDoc() {
    var numbers = [4, 7, 10, 13, 16]
    numbers.remove_where(is_odd)
    numbers.truncate(2)
    var velocities = Items<Velocity>()
    var entities = List<Integer>()
    var entity = 0
    while entity < 5 {
        var velocity = Velocity(1.0 * entity)
        velocities.append(velocity)
        entities.append(entity)
        entity = entity + 1
    }
    var moving = velocities.copy()
    moving.remove_where_stopped()
    var kept = 0
    var row = 0
    while row < velocities.count() {
        crash entities[row]
        var owner = entities[row]
        if not is_despawned(owner) {
            velocities.swap(row, kept)
            kept = kept + 1
        }
        row = row + 1
    }
    velocities.truncate(kept)
    entities.remove_where(is_despawned)
    var joined = numbers.join(", ")
    var across = velocities.sum_across()
    var rows = entities.count()
    var moving_count = moving.count()
    console.print(joined, moving_count, across, rows)
}

func is_odd(number: Integer): Boolean {
    return number % 2 == 1
}

func is_despawned(entity: Integer): Boolean {
    crash entity < despawned.count()
    return despawned[entity]
}
```
```output
4, 10 4 6 3
```

Each costs one walk over the collection, however many go: every element that stays is moved down at most once,
and the ones removed are released at the end. Removing half of 200 000 items by a despawn list one
`remove_swapping` at a time took 530 µs, against 340 µs for `remove_where`, and removing nine in ten 930 µs
against 290 µs (`benchmarks/bulk_removal`). What stays keeps its order, which one `remove_swapping` per item does
not. A test that is not a function of the element alone (a mask read by row) is written as the loop above:
`swap` each row that stays down to the next free place, then `truncate`.

`remove_where` changes the collection it is called on, so it ends no chain: `names.filter(is_short).remove_where(is_long)`
is an error naming the fix, one test that says everything to remove. Like `remove_at`, all three may move items,
so an item borrowed from a `Vector` or `Items` is not read after one ([the rules](#removing-many-at-once-1)).

## Member templates: loops you do not write

A list of a class answers a family of functions named after the element's members (a function of your own is
passed instead, [below](#passing-a-function-for-each-element)). `chores.count_done()` counts
the chores whose `done` is true, `items.sum_price()` adds up their prices, and `repositories.map_name()` collects
their names. A **member** is an attribute or a function that takes no arguments (the two are the same to a
template, since reading an attribute already goes through its getter), and a template is compiled only for the
names a program calls.

| Template | Result | The member must |
|---|---|---|
| `filter_<member>()` | `List<T>`, the elements where it is true | take nothing, return `Boolean` |
| `count_<member>()` | `Integer`, how many are true | take nothing, return `Boolean` |
| `any_<member>()` / `all_<member>()` | `Boolean` | take nothing, return `Boolean` |
| `sum_<member>()` | the member's number type | take nothing, return a number |
| `find_by_<member>(value)` | `T?`, the first element whose member equals `value` | return something comparable to `value` |
| `sort_by_<member>()` | `List<T>`, sorted ascending, stable, in `n log n` time | return a number or a `String` |
| `map_<member>()` | `List<U>`, one value per element | return a value |
| `each_<member>()` | nothing: calls it on every element | be a function that takes nothing; what it returns is discarded |
| `remove_where_<member>()` | nothing: removes the elements where it is true, keeping the rest in order | take nothing, return `Boolean` |

A member that does not fit is a compile error naming the member, what it is, and what the template needs.
`count_` on a number is one of them, and names `sum_` instead: `count()` is only ever a collection's size.
`each_` on an attribute is another: reading a value only to discard it does nothing.

A template costs what the loop you would have written costs: each one is a `while` over the list's buffer,
compiled into the program only when something calls it, with no function value or closure in between.
`list.parallel_each_<member>()` is the same walk split across the thread pool
([concurrency.md](concurrency.md#parallel_each_-a-member-on-every-element)).

```gdscript title=list_helpers/chore.spite
var title = ""
var done = false

func Chore(new_title: String, new_done: Boolean) {
    title = new_title
    done = new_done
}

func finish() {
    done = true
}
```
```gdscript title=list_helpers/list_helpers.spite entry
var console = Console()

func ListHelpers() {
    var chores = List<Chore>()
    var write_documentation = Chore("write docs", false)
    chores.append(write_documentation)
    var ship_release = Chore("ship release", false)
    chores.append(ship_release)
    var rest = Chore("rest", true)
    chores.append(rest)
    var done_count = chores.count_done()
    console.print("done count", done_count)
    var any_done = chores.any_done()
    console.print("any done", any_done)
    var all_done = chores.all_done()
    console.print("all done", all_done)
    var titles = chores.map_title()
    crash titles[0]
    console.print("first title", titles[0])
    chores.each_finish()
    all_done = chores.all_done()
    console.print("all done now", all_done)
}
```
```output
done count 1
any done true
all done false
first title write docs
all done now true
```

```gdscript title=list_query/item.spite
var name = ""
var price = 0
var in_stock = false

func Item(new_name: String, new_price: Integer, new_in_stock: Boolean) {
    name = new_name
    price = new_price
    in_stock = new_in_stock
}
```
```gdscript title=list_query/list_query.spite entry
var console = Console()

func ListQuery() {
    var items = List<Item>()
    var sword = Item("sword", 50, true)
    items.append(sword)
    var shield = Item("shield", 30, false)
    items.append(shield)
    var potion = Item("potion", 10, true)
    items.append(potion)
    var in_stock_count = items.filter_in_stock().count()
    console.print("in stock count", in_stock_count)
    var in_stock_price = items.filter_in_stock().sum_price()
    console.print("in stock price total", in_stock_price)
    var found = items.find_by_name("shield")
    if found {
        console.print("found", found.name, found.price)
    }
    var by_price = items.sort_by_price()
    var index = 0
    while index < by_price.count() {
        console.print(by_price[index].name, by_price[index].price)
        index = index + 1
    }
}
```
```output
in stock count 2
in stock price total 60
found shield 30
potion 10
shield 30
sword 50
```

`filter_<member>()` and `sort_by_<member>()` return a new list holding the same elements, each one referenced
once more and not copied. A `Dictionary<T>` answers every template through its values: `inventory.sum_price()` is
`inventory.values().sum_price()`.

```gdscript title=template_mistake/repository.spite
var stars = 0
```
```gdscript title=template_mistake/template_mistake.spite entry error
var console = Console()

func TemplateMistake() {
    var repositories = List<Repository>()
    var starred = repositories.count_stars()
    console.print(starred)
}
```
```diagnostic
but 'count_' needs it to return Boolean (to add up a numeric member use 'sum_stars')
```

**A `while` that only does what a template does is an error naming the template**: a
counter walking a list from `0` to its `count()`, doing nothing with each element but what one template does
with one of its members, or what `each`, `map`, `filter`, `count`, `sum`, `find`, `any` or `all` does with a
function passed the element, on a list of anything, numbers and text included. Loops that need the index, pass
more than the element, stop early for another reason, or walk state keep their `while`. The exact shape the
compiler looks for is in [control_flow.md's rules](control_flow.md#a-while-that-a-member-template-already-says).

```gdscript title=template_loop_error/item.spite
var price = 0

func Item(item_price: Integer) {
    price = item_price
}
```
```gdscript title=template_loop_error/template_loop_error.spite entry error
var console = Console()

func TemplateLoopError() {
    var items = [Item(3), Item(5)]
    var total = 0
    var index = 0
    while index < items.count() {
        total = total + items[index].price
        index = index + 1
    }
    console.print(total)
}
```
```diagnostic
this 'while' walks every element of 'items' only to add up 'price': write 'var total = items.sum_price()'
```

### A member's own function in a template's name

A member template can go one step further and ask each element's member a question of its own:
`filter_<member>_<function>(arguments)` keeps the elements whose `member` answers `function(arguments)`. The
arguments are handed on to that function, so `players.filter_name_starts_with("a")` keeps the players whose
`name` starts with `a`. It works with every member template and every function the member's class has:

```gdscript title=chained_members/player.spite
var name = ""
var score = 0

func Player(starting_name: String, starting_score: Integer) {
    name = starting_name
    score = starting_score
}
```
```gdscript title=chained_members/chained_members.spite entry
var console = Console()

func ChainedMembers() {
    var players = List<Player>()
    var anna = Player("anna", 3)
    players.append(anna)
    var bruno = Player("bruno", 5)
    players.append(bruno)
    var alex = Player("alex", 7)
    players.append(alex)
    var starting_with_a = players.filter_name_starts_with("a")
    var names = starting_with_a.map_name()
    var joined = names.join(", ")
    var with_n = players.count_name_contains("n")
    var best = players.sum_score()
    console.print(joined, with_n, best)
}
```
```output
anna, alex 2 15
```

## Member templates over an enum value

When the element has a member typed with a named enum, the
templates that ask a yes-or-no question also take one of that enum's values as their name: `filter_<value>()` keeps
the elements whose member is that value. A listing does not need one helper per kind of thing it lists; it is
filtered by its kind:

```gdscript
enum Stage {
    'draft'
    'published'
    'archived'
}

var title = ""
var stage: Stage = 'draft'
```

```gdscript
var published = posts.filter_published()
var drafts = posts.count_draft()
var anything_archived = posts.any_archived()
posts.remove_where_archived()
```

`posts.filter_published()` means exactly `posts.filter_stage_is_published()` would, had `Post` a
`func stage_is_published(): Boolean { return stage == 'published' }`: one loop, one comparison of two small
integers per element, and a chain of them fuses like any other ([below](#chains-run-as-one-loop)). This is how a
directory lists only its files: `directory.entries().filter_files()`, where each entry's `kind` is `'files'` or
`'folders'`, and why `Directory` has no `files()` of its own ([standard_library.md](standard_library.md#list-a-directory)).

## Passing a function for each element

A template sees only the element and the list: `names.each_say_hello()` looks for a member `say_hello` of each
name, never for a function of the class the call is written in. A function of yours is passed as a value
instead: `names.each(say_hello)` calls `say_hello` once for every name, in order, and there is no
`say_hello_to_everyone` to write. The function is bound to whoever owns it ([functions_and_operators.md](functions_and_operators.md#functions-are-values)):
`say_hello` alone is this instance's, and `people.each(greeter.greet)` calls `greet` on `greeter`. The library's
classes are no different: `keys.filter(counts.has)` asks the dictionary `counts`, and
`words.filter(greeting.contains)` asks the text `greeting`.

`each`, `map`, `filter`, `any`, `all`, `count`, `find`, `sort_by` and `sum` each take such a function, which takes
the element as its only argument: `filter`, `any`, `all`, `count` and `find` want one returning `Boolean` (`find`
answers the first element it is true for, or `null`), `sum` one returning a number, `sort_by` one returning a
number or text, `map` one returning anything. This works on a list of anything (text and numbers included, which
have no members of their own for a template to name) and on a `Dictionary`, through its values.

```gdscript title=passed_function/greeter.spite
var console = Console()

func greet(name: String) {
    console.print("welcome, {name}")
}
```
```gdscript title=passed_function/passed_function.spite entry
var console = Console()
var greeter = Greeter()

func PassedFunction() {
    var names = ["Ada", "Grace", "Barbara"]
    names.each(say_hello)
    names.filter(is_short).each(greeter.greet)
    var short_names = names.filter(is_short)
    var joined = short_names.join(", ")
    console.print("short:", joined)
    var letters = names.filter(is_short).map(measure).sum(double_of)
    console.print("letters, doubled:", letters)
    var first_long = names.find(is_long)
    crash first_long
    console.print("first long name:", first_long)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}

func is_short(name: String): Boolean {
    return name.length() < 6
}

func is_long(name: String): Boolean {
    return name.length() > 6
}

func measure(name: String): Integer {
    return name.length()
}

func double_of(value: Integer): Integer {
    return value * 2
}
```
```output
hello, Ada
hello, Grace
hello, Barbara
welcome, Ada
welcome, Grace
short: Ada, Grace
letters, doubled: 16
first long name: Barbara
```

A function written by name is compiled straight into the loop, so passing it costs nothing; a function value held
in a variable (`var shout = greeter.shout`, then `names.map(shout)`) is called through the value. A function that
does not fit is an error naming what the template needs:

```gdscript title=passed_function_mistake/passed_function_mistake.spite entry error
var console = Console()

func PassedFunctionMistake() {
    var names = ["Ada", "Grace"]
    var greetings = names.map(say_hello)
    console.print(greetings)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}
```
```diagnostic
'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value (to only call it for each element, write 'each(say_hello)')
```

The template still sees only the element: a function that needs more, such as `print_statement(statement, depth)`,
is called from a `while`.

A list's own functions are passed the same way, so `numbers.each(found.append)` copies every number into `found`,
and a chain of passed functions over a list of numbers is one loop over its buffer:

```gdscript title=plain_vector/plain_vector.spite entry
var console = Console()

func PlainVector() {
    var weights = List<Float>()
    weights.append(1.5)
    weights.append(2.5)
    weights.append(4.0)
    var found = List<Float>()
    weights.each(found.append)
    var heavy = weights.filter(is_heavy).sum(double_of)
    crash weights[0]
    var first = weights[0]
    weights.append(8.0)
    var joined = found.join(", ")
    console.print(joined, heavy, first)
}

func is_heavy(weight: Float): Boolean {
    return weight > 2.0
}

func double_of(weight: Float): Float {
    return weight * 2.0
}
```
```output
1.5, 2.5, 4 13 1.5
```

A `Vector` and an `Items` take no passed function: their items are classes, borrowed and never passed on
(`velocities.each(f)` is an error naming a member template instead), or text (`names.each(f)` on a
`Vector<String>` says to keep the text in a `List<String>`).

## Chains run as one loop

Every template takes what the one before it gives, so they chain: `map_<member>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as (that is what it means), but the compiler runs it as **one loop over
the first list**, with no list in between: `teams.filter_active().map_lead().sum_age()` visits each team once,
reads its lead, and adds the age, allocating nothing.

```gdscript title=fused_chain/person.spite
var age = 0

func Person(new_age: Integer) {
    age = new_age
}
```
```gdscript title=fused_chain/team.spite
var active = false
var lead = Person(0)

func Team(new_active: Boolean, new_lead: Person) {
    active = new_active
    lead = new_lead
}
```
```gdscript title=fused_chain/fused_chain.spite entry
var console = Console()

func FusedChain() {
    var teams = List<Team>()
    var ann = Person(34)
    var ann_team = Team(true, ann)
    teams.append(ann_team)
    var bob = Person(67)
    var bob_team = Team(false, bob)
    teams.append(bob_team)
    var ages = teams.filter_active().map_lead().sum_age()
    var active = teams.filter_active()
    var leads = active.map_lead()
    var ages_step_by_step = leads.sum_age()
    console.print(ages, ages_step_by_step)
}
```
```output
34 34
```

Only a call made directly on another template call is part of the chain: `active.map_lead()` above starts a new
one, because `active` is a list the program named and kept. The steps in the middle are `map_` (to a member that
is a class) and `filter_`; the last call can be any template, and a chain the compiler cannot write as one loop
is run step by step, which means the same. The one visible difference is order: a member function in a fused
chain runs element by element, where the steps written out would run it on every element before the next step
starts. Written as three steps, the chain above would make two lists every time it ran; fused, it makes none
([the rules](#standard-library-metaprogramming) name the test that pins the count).

## How the member templates are written

The templates are ordinary Spite in `library/list.spite`, over the list's own buffer on the heap. Each one is a
template ([metaprogramming.md](metaprogramming.md#member-templates)) whose parameter is
`member: Spite.Attribute<$element_type>`: it names a member of the *element*, and `item.attributes[member]` reads
it: the field itself, or a call to the zero-argument function. `filter_member` answers `filter_in_stock`,
`filter_is_popular` and every other `filter_<member>` call:

```gdscript
func filter_member(member: Spite.Attribute<$element_type>): List<$element_type> {
    var filtered = List<$element_type>()
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        if item.attributes[member] {
            filtered.append(item)
        }
        index = index + 1
    }
    return filtered
}
```

The same template answers a passed function. For `names.each(say_hello)` the compiler compiles `each_member`
once more with the function's owner passed in beside the list, and `item.attributes[member]` reads as
`say_hello(item)` on that owner (or as a call through the value, for a function held in a variable), so
`filter_member` also answers `filter(is_short)` without being written twice. A chain that passes functions is still
one loop.

`values` is the list's `TypedMemory<$element_type>`: `values.read_value(items, index)` reads the element in slot
`index` of the list's buffer, retained, the same way a container of your own would
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)); everything else is written in the file. A
build with `--repl` or `--repl-port` compiles every template that fits every element class of a list the loop
can reach, so `monsters.sum_health()` can be typed at the prompt; that is the one build where templates nobody
calls are in the program. A private member (`_name`) of the element gets none: a template is
`List`'s code, and a private name is read only inside its own class
([classes_and_files.md](classes_and_files.md#private-names)), so `List<Parallel<Integer>>` has no `each__join`
in any build.

## Write your own member template

A program reopens `List` by putting a `list.spite` in its own folder, and a function there with a
`Spite.Attribute<$element_type>` parameter named after a word of its name becomes one more template, exactly like the
library's:

```gdscript title=list_average/list.spite
func average_member(member: Spite.Attribute<$element_type>): Float? {
    assert item_count != 0
    var total = 0.0
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        total = total + item.attributes[member]
        index = index + 1
    }
    return total / item_count
}
```
```gdscript title=list_average/score.spite
var points = 0

func Score(new_points: Integer) {
    points = new_points
}
```
```gdscript title=list_average/list_average.spite entry
var console = Console()

func ListAverage() {
    var scores = List<Score>()
    var three = Score(3)
    scores.append(three)
    var four = Score(4)
    scores.append(four)
    var average = scores.average_points()
    crash average
    console.print(average)
}
```
```output
3.5
```

The `<$element_type>` is what makes it a member of the element. A plain `member: Symbol` would name one of the
list's own attributes, which are its buffer, so it is an error that says what to write:

```gdscript title=plain_symbol_template/list.spite
func total_member(member: Symbol): Integer {
    var total = 0
    var index = 0
    while index < item_count {
        var item = values.read_value(items, index)
        total = total + item.attributes[member]
        index = index + 1
    }
    return total
}
```
```gdscript title=plain_symbol_template/plain_symbol_template.spite entry error
var console = Console()

func PlainSymbolTemplate() {
    var scores = List<Integer>()
    scores.append(3)
    var count = scores.count()
    console.print(count)
}
```
```diagnostic
write 'member: Symbol<$element_type>' to name a member of the element
```

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules
disagree, the rules win.

### Standard library metaprogramming

**Every standard library template names a `<member>`, and a member is a field or a zero-argument function without
distinction.** `map_age` and `map_get_age` are the same call, because reading an attribute already goes through
`get_<attribute>()` ([Operators](functions_and_operators.md#operators)). The compiler may answer the
field case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

The templates, what each answers and what it requires of the member are [the table above](#member-templates-loops-you-do-not-write),
which is normative; a `List<T>` of a class and a `Dictionary<T>` of a class answer all of them. These, together
with `while` and the passed functions below, cover the cases a `for` loop covers elsewhere. This is why a list of
components renders with nothing new in the language: `todos.map_render()` calls `render()` on every element and
collects the results, exactly as `todos.map_title()` collects a field.

A member that does not fit is a compile error naming the member, what it is, and what the template needs. Two
of them name a fix: `count()` is only ever a collection's own size, so `count_<member>()` on a numeric member says
`but 'count_' needs it to return Boolean (to add up a numeric member use 'sum_stars')`; and `each_<member>()` on an
attribute says `'each_size': the member 'size' of 'Thing' is an Integer, but 'each_' needs it to be a function:
reading an attribute and discarding it does nothing`. `sort_by_` is stable (equal keys keep their order) and
is a merge sort, so its time grows as `n log n`: it reads each key once, then merges runs of an index list, and
makes the key list, two index lists and the result (`sort_by(f)` is the same template, and `Directory`'s
`files()` and `folders()` sort through it; `conformance/stage6/sorting_many`).

**How the templates are written.** They are templates in `library/list.spite`, each a `while` over the list's
`Memory` buffer: `func filter_member(member: Spite.Attribute<$element_type>): List<$element_type>` answers every
`filter_<member>` call. The parameter names a member of the *element*, not of the list (whose own attributes are
its buffer), because it says so: `Spite.Attribute<$element_type>` is a [template](metaprogramming.md#templates)'s
`Spite.Attribute<Label>`, one mechanism for both. `item.attributes[member]` reads it: the field, or a call to the
zero-argument function, which is "the value held in that field" applied to members. The generator binds the template to the
element's member and checks [the table](#member-templates-loops-you-do-not-write) before it compiles the body, so a member that does not fit is still
the error naming the member, its type and what the template needs; it writes none of the templates' C. Only the
names a program calls are compiled. A `--repl`/`--repl-port` build compiles every template that fits every
element class of a list the loop can reach, and lists them as that list's functions, so `monsters.sum_health()`
works at the prompt. `Dictionary<T>` answers the same names through its values (`inventory.sum_price()` is
`inventory.values().sum_price()`), and a program's own `list.spite` reopens `List` to add a template of its own.
A template there with a plain `member: Symbol` would range over `List`'s own attributes, its buffer, and answer
nothing, so it is a compile error naming `Symbol<$element_type>` (`diagnostics/plain_symbol_on_list`).

**Chains are one loop.** Each template takes what the previous one returns, and a chain means exactly its steps written out one by one.
The compiler runs a chain as one loop over the first list with no list in between: when a template is called
directly on a `map_`/`filter_` call (on a `map_`/`filter_` call, and so on) of a `List` or `Dictionary`, the
generator writes one Spite function on the first list's class (a `while` over its buffer, an `if` per
`filter_`, a `var` per `map_`, and the last template's step) and calls that instead. A `map_` in the middle
must reach a class; anything the rule cannot write (a nullable member, a union) is compiled step by step, which
means the same. The difference a program can see is only order: a member function in a fused chain runs element
by element. `conformance/stage6/fused_chain_allocations` pins what it saves: four chains run a thousand times
allocate nothing, so the whole program makes 15 allocations (its objects, the list and printing the total)
against more than 16 000 step by step.

**Passing a function for each element.** An iterator sees only the
element and the list, never the class the call is written in: `people.each_say_hello()` names a member
`say_hello` of each `Person`, and a function of the caller never answers
it. When the calling class has a function of that name, the error says to pass it
(`'String' has no attribute or zero argument function 'say_hello' for 'each_say_hello': a template reads a member
of each element, never a function of this class, so pass this class's 'say_hello' instead: 'each(say_hello)'`).
The caller's function is passed as a bound function value, owned by whoever it is bound to:

- **The forms.** `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and
  `sum(f)` on a `List` or a `Dictionary` (through its values), for an element of any type, never on a `Vector` or an `Items`,
  whose items are borrowed or text; `remove_where(f)` on a
  `List`, never on a `Dictionary`. `f` takes the element
  as its only argument, with exactly the element's type; [the table of templates](#member-templates-loops-you-do-not-write) applies to what it returns (`filter`, `any`,
  `all`, `count` and `find` want `Boolean`, `sum` a number, `sort_by` a number or a `String`, `map` a value, `each`
  anything). `find(f)` answers the first element `f` is true for, or `null`, which is the `find_by_` template with
  `true` as its value. `count` with no argument stays the collection's size.
- **The owner.** `say_hello` alone is bound to this instance; `greeter.greet` to `greeter`; a variable holding a
  `Spite.Function<T, R>` is called through the value. `people.map(greeter.label).filter(is_short)` mixes owners.
  A list's own function is bound to the list the same way (`numbers.each(found.append)`); a chain that passes one
  is not fused, and runs step by step. So is every library class's: a `Dictionary`'s (`keys.filter(counts.has)`, `keys.map(counts.get)`), and a
  `String`'s or a number's (`words.filter(greeting.contains)`). A `List`'s or a `Dictionary`'s function may also
  be held as a value, `var lookup = counts.get`, bound to that dictionary; a `String`'s or a number's may only be
  passed to a form, since a value of text is no object a function value can keep: `var check =
  greeting.contains` is "'contains' of a String is passed straight to a form, like 'names.filter(text.contains)',
  and cannot be held as a value yet ...". What the function answers is what the form sees, so `counts.get`
  answers `Integer?`, and `keys.sort_by(counts.get)` is an error naming the fix: a function of your own
  that narrows it (`conformance/stage6/library_functions_passed`, `diagnostics/library_function_mistakes`).
- **How it is written.** No template changes: the same `library/list.spite` template (`each_member(member:
  Spite.Attribute<$element_type>)`) is instantiated once per function and owner class, with a last hidden parameter holding
  the owner (the function's instance, passed at the call site), and `item.attributes[member]` reads as
  `owner.say_hello(item)`, or `owner(item)` when the owner is a held function value. A function written by name
  therefore costs no allocation and no indirect call; only a held value is called through `Spite.Function`. Only
  the forms a program calls are instantiated, and these instances are not listed among the list's `functions`
  nor offered at a `--repl` prompt.
- **Chains.** A passed function chains and fuses with the member templates: `map(f)` and `filter(f)` may
  sit in the middle of a chain and any form may end one, so `people.filter_active().map(greeter.label)` and
  `names.filter(is_short).map(measure).sum(double_of)` are each one loop; the fused function takes each owner as
  a parameter. The element's own members still come only from the element (`people.map(say_hello).filter_active()`
  reads `active` of whatever `say_hello` returns).
- **Mistakes** name the form: `'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value
  (to only call it for each element, write 'each(say_hello)')`, `'each' calls 'greet_twice' with each 'String' as
  its only argument, but 'greet_twice' takes 2`, `'each' calls 'count_to' with each 'String', but 'count_to' takes
  'Integer'`, and an argument that is no function at all.
- **The loop rule.** A `while` that only does what `each(f)` does is under the same rule as the member templates
  ([control_flow.md](control_flow.md#a-while-that-a-member-template-already-says)). A function that needs more than the element (`print_statement(statement, depth)`) keeps its `while`.

`conformance/stage6/passed_functions`, `diagnostics/passed_functions`.

### Member templates over an enum value

**The templates that ask a yes-or-no question of each element also work on a named enum's values.**

- **Which templates.** `filter_`, `count_`, `any_`, `all_` and `remove_where_`, the ones whose member must
  "take nothing, return `Boolean`" in [the table](#member-templates-loops-you-do-not-write). The others already
  take a member and compare or read it (`find_by_kind('files')`, `map_kind()`, `sort_by_kind()`), so an enum value
  adds nothing to them, and `sum_<value>` or `each_<value>` is the ordinary "no member" error.
- **How the name is read.** For `list.filter_<name>()` over elements of class `T`: when `T` has a member named
  `<name>`, it is that member, exactly as for any template. Otherwise the compiler looks at `T`'s members typed with a named
  enum (attributes and functions taking no arguments, non-nullable) and at the values each enum lists. Exactly
  one such member whose enum lists `<name>` makes the template read `element.<member> == '<name>'`, and nothing
  else changes: same result type, same fusion, same order, same cost as a `Boolean` member, one comparison of two
  small integers. A reopened enum's added values count ([packages.md](packages.md#reopening-an-enum-adds-values)).
- **A union element** is read through a member every class of the union answers, with the same enum type; a
  member only some of them answer is not a candidate.
- **Errors, never a guess.** No member and no enum value: the usual error, which then also says no enum of
  `T`'s members lists `<name>`. Two or more members whose enums list `<name>`: "'filter_<name>' could read '<a>' or
  '<b>', whose enums both list '<name>': write the comparison as a Boolean member of <T>, or pass a function". A
  member named `<name>` that is also a value of such an enum: an error naming both, since which one is read must
  not depend on what else the class declares.
- **No helper answers one kind of a listing.** A function that answers the part of a listing of one kind, such as
  `Directory.files()` or `Directory.folders()`, would be a second way to write `entries().filter_<kind>()`, so there
  is none.
- **Tree shaking and run time.** Resolved while compiling; nothing exists at run time that a `Boolean` member's
  template would not have, and an enum no template reads costs nothing.

### List<T> additions

A list's members, their results and their edge cases are [the table under `List<T>`](#listt), which is normative:
`list[index]` and `get_at(index)` are one function and answer `T?`, `null` out of range (`[]` is only
a shortcut for `get_at`, [functions_and_operators.md](functions_and_operators.md#operators); an out of
range read answers `null` and never a default that would read as a real element); `first`, `last`, `remove_first`
and `remove_last` answer `null` on an empty
list (like `[]`), `insert` clamps to the nearest end, and `set_at`, `remove_at` and `remove_swapping` do
nothing. `reserve(count)` grows the buffer to hold `count` elements and adds none. A `List` of numbers, `Boolean`,
enums or `Memory.Address` is the one way to keep a list of them: its buffer holds the values themselves, which is
what a `Vector` of them held (`conformance/stage6/plain_items`). There is
no `for`: a list is walked with a template, a passed function ([above](#standard-library-metaprogramming)),
or a `while` that does more than they do.

`contains(value)` compares with `==`, so it is there only for elements that are numbers, `Boolean`, `String` or an
enum; on a list of a class the call is `List has no method 'contains'`: ask with `any(f)` or `find_by_<member>`.

Names say where: `add` does not, so it is `append` (and `prepend`); `pop()` is `remove_last()`, beside
`remove_first()`. Writing `add` or `pop` is a compile error naming the replacement: `List has no method 'add', which
does not say where: write 'append' to add at the end, or 'prepend' at the start`, and `List has no method 'pop':
write 'remove_last()', or 'remove_first()' to take from the start` (`diagnostics/old_list_names`).

### Dictionary\<T\>

Insertion-ordered, keyed by text or by whole numbers; its members are [the table under
`Dictionary<T>`](#dictionaryt), which is normative. `dictionary[key]` is `get(key)`, a `T?` that is `null` for an
absent key, and `dictionary[key] = value` is `set(key, value)`; there is no `get_at`/`set_at` on a dictionary.
`keys()` and `values()` answer fresh copies, in insertion order.

**The key kind is decided while compiling.** Each dictionary is keyed by text or by whole numbers, never both, and nothing is written for
it: `Dictionary<T>` stays the one spelling.

- The keys the program gives a dictionary decide it: the key of `[]`, `[] =`, `set`, `get`, `has` and
  `remove`. A `String`, a symbol or an enum value is a text key (an enum value becomes its name); a
  `Tiny`, `Byte`, `Short`, `UnsignedShort`, `Integer`, `UnsignedInteger`, `Long` or `UnsignedLong` is a number
  key. A dictionary given no key at all is keyed by text.
- The kind follows the dictionary wherever it goes, like its value type: a local it is assigned to, a parameter it
  is passed to, an attribute that holds it, a function that returns it, a list of dictionaries, and a generic
  class it is handed to (`JsonWriter(scores)`, `BinaryReader<Shelf>`). A key given anywhere along that path
  decides it for all of them.
- One dictionary given a text key and a number key is a compile error at the number key, naming the text key's
  place: `this dictionary is given a whole-number key here and a text key at <file>:<line> (in <class>.<function>):
  a dictionary is keyed by text or by whole numbers, never both, so give every key of it the same kind`, with the
  places that tied the two before the colon when they were given to different dictionaries (below;
  `diagnostics/mixed_dictionary_keys`).
- **Only the program's own flows decide it.** The descriptions
  the compiler writes for `to_debug()` (`Spite.Debug<T>` and `Spite.DebugInstance<T>`, one of each per type for
  the whole program) take a dictionary as the kind it already has and never tie it to another. Otherwise every
  `Dictionary<String>` attribute of every class described would pass through the one `Spite.Debug<Dictionary<String>>`
  and be tied to all the others, so a number key given to one would change the kind of an unrelated one, and
  removing code that made some class be described would change what compiled
  (`conformance/stage6/debug_dictionary_keys`).
  The same holds for the member templates a build compiles only so the prompt can call them (a `--repl`,
  `--repl-port` or `--hot-reload` build, [below](#how-the-member-templates-are-written)): `map_<member>` over a
  dictionary attribute appends it to the one `List<Dictionary<String>>` of the program, so each such template would
  tie every dictionary attribute of every listed class together, and a `--hot-reload` build of a large game would
  fail with a kind its `--optimized` build never had. A template kept for the prompt neither ties dictionaries nor
  gives one a key; one the program calls does both (`conformance/stage6/kept_templates`). The kinds are the same in
  every build of one program.
- **Two kinds that meet are named.** Where a dictionary of one kind is given where one of the other kind is
  wanted, the error says which is which and what decided each, rather than naming two `Dictionary<String>`s:
  `a Dictionary<String> keyed by whole numbers (Long), from the key at engine/columns.spite:47 (in
  Columns.name_of_header) cannot be used where a Dictionary<String> keyed by text, from the key at
  engine/recipes/cache_reader.spite:25 (in Recipes.CacheReader.fingerprint_of) is needed: a dictionary is keyed by
  text or by whole numbers, decided while compiling by the keys it is given, and these two were decided apart --
  give both the same kind of key, or copy the entries across one by one`. A kind nothing decided reads `text,
  since nothing gives it a whole-number key`. **A key that reaches a dictionary through others names the way it
  came**: when the deciding key was given to another dictionary tied to this one,
  the kind (here and in the mixed-keys error above) adds `, which reaches it through <file>:<line> (in
  <class>.<function>), ...`, each place that tied two of them (an assignment, an argument, a return), up to four
  and `and N more`. So a dictionary tied to an unrelated one by code the reader never wrote, such as a template kept
  for the prompt, says how the far key got there.
- The error that a dictionary never settles names the class and function that make it, not whichever function the
  compiler read last.
- **The kinds always settle, or it is an error.** They are found by compiling again with what the last pass
  learned, at most eight times; a program whose kinds are still changing then is a compile error at a dictionary
  that keeps changing, `the dictionary made here never settles on text or whole-number keys: each pass of the
  compiler decides it the other way (last as whole numbers, from the key at ...), since what decides it flows
  back into it; give it a key the program itself writes, of one kind`, never whatever the last pass compiled.
- A number-keyed dictionary's key type is the widest whole-number type any of its keys has (`Integer` keys and one
  `Long` key make a `Long`-keyed dictionary); a narrower key is widened as an argument is. `keys()` answers a
  `List` of that type, and `copy()` and `deep_copy()` are keyed the same way.
- A number key is stored and hashed as the number: no `String` is made for it, in a lookup or in the table.
  Everything else is as for text keys: insertion order, `null` for an absent key, `remove` moving later entries
  down, the member templates through the values (`conformance/stage6/number_keys`).
- Written as JSON, a number key is the number in quotes (`{"7":"SEVEN"}`), and reading JSON into a number-keyed
  dictionary reads each key's text as the number. Binary bytes carry the number as its type is written, and
  `schema()` counts the key's type in, so text-keyed and number-keyed dictionaries have different schemas.

It is a hash table over two ordered lists:
`get`, `has`, `set` and `[]` take the same time however many keys there are, and `remove` takes time in
proportion to the dictionary's size. An empty dictionary allocates no table.

The whole member template family, and every passed-function form, works on a `Dictionary<T>` through its values,
as it does on a `List<T>`; to walk keys and values together, use `while` over `dictionary.keys()` and read
`dictionary[key]`.

`deep_copy()` works for classes, lists and dictionaries. A `String` is shared rather than duplicated
because it is immutable; a union or a `type` shape is shared; a self-referring structure is unsupported ([Memory](memory.md#the-memory-model)).

### Vector\<T\>

A `Vector<T>` holds its items inline, where a `List<T>` holds references, and reading an item gives a borrowed
reference into the vector. Its members are [the table
under `Vector<T>`](#vectort-items-inline), which is normative. The readings:

- **What an item may be.** A `String`, or a class whose attributes are only numbers, `Boolean`s, enums and
  `String`s (a `T?` of one of them, or a singleton such as `Console`, included). A number, a `Boolean`, an enum or a
  `Memory.Address` (or a `T?` of one) is not an item: a `List` holds them flat
  already, so `Vector<Integer>` is `'Vector<Integer>' holds plain values, and a List keeps numbers, Boolean, enums
  and Memory.Address flat just as it did, so a list of them has one spelling: write 'List<Integer>'`
  (`diagnostics/plain_items`), and `Items<Integer>` the same with `Items`. Anything else is an error when the vector
  type is written: `'Holder' cannot be an item of a Vector: its attribute 'scores' is a List<Integer>, and a
  Vector holds each item inline, so every attribute is a number, a Boolean, an enum or a String (keep 'Holder' in
  a 'List<Holder>', or keep 'scores' somewhere else)`; `Vector<List<Integer>>` is `a Vector holds each item
  inline, so an item is a number, a Boolean, an enum, a String or a class made only of those, and a List<Integer>
  is not: keep it in a 'List<List<Integer>>'`. A class with a `drop()` is `'Handle' cannot be an item of a Vector:
  it has a 'drop()', and an item inline in a Vector is never an object of its own to drop (keep 'Handle' in a
  'List<Handle>')`, and a class whose functions use `this` as a value (return it, pass it, keep it, or name one of
  its own functions as a value) is `'Selfish.itself' uses 'this' as a value, and an item of a Vector is borrowed
  from the Vector, never an object to keep or to pass on: read and write its attributes instead`.
- **Reading.** `vector[index]` and `get_at(index)` are one function and answer a `T?`: `null` out of range, and the
  item itself, borrowed, once the read is narrowed (by `crash`, `assert` or `if`, or by a proof the compiler already
  holds), exactly as for a list
  ([failure.md](failure.md#reading-with--answers-t)): `while index < velocities.count()` proves
  `velocities[index]` in the loop, and `crash velocities.count() >= 2` proves `velocities[0]` and `velocities[1]`.
  A proven read writes no check of its own; the read itself still compares the index once. A vector of `String`s
  answers a counted reference, as a list does. What a borrowed item may do, and
  what a row of them passed to a system may do, is
  [memory.md's rule](memory.md#borrowed-items-of-a-vectort).
- **Writing.** `append(value)` and `set_at(index, value)` (`vector[index] = value`) copy the value's attributes
  into the block, counting each `String` attribute once more; the value itself stays an ordinary object. A constructor
  cannot be the argument, so an appended item is made on its own line first: `var slow =
  Velocity(1.0, 0.5)`, then `velocities.append(slow)`.
- **Removing.** `remove_at(index)` releases the item's `String` attributes and moves every later item down;
  `clear()` releases every item's and keeps the capacity; dropping the vector releases them and frees the block.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_` and `sum_<member>()` are in
  `library/vector.spite`, written over the borrowed items; `filter_` answers a `Vector<T>` of copies and `map_` a
  `List` of the members' values. A chain of them is one loop over the block, with `filter_` steps in the
  middle, and `parallel_each_<member>()` splits the walk across the thread pool as it does a list's.
  No passed-function form is offered, since a `List` of numbers
  takes every form. A passed function would take a class item as an argument, which a borrowed item never is, so
  `velocities.each(f)` is `'each' passes each item to a function, and an item of a
  Vector is borrowed from it, never passed on: give the item's class a function and call it with a member
  template, such as 'each_<function>()'`; and a vector of `String`s, whose items are references to text, has
  `names.each(f)` as `'each' passes each item to a function, which a Vector never does: keep the values in a
  'List<String>' to pass them on` (`diagnostics/text_items_passed`).
- **Its allocator.** `velocities.memory.allocator = arena` on the next line places the `Vector` object in the
  arena, as for any object.

`diagnostics/vector_borrows`, `diagnostics/vector_items`, `diagnostics/plain_items`,
`diagnostics/text_items_passed`, `conformance/stage6/vector_items`, `conformance/stage6/plain_items`.

### Items\<T\>

`Items<T>` chooses its storage while compiling, inline like a
`Vector<T>` when `T.is_fixed_size` and references like a `List<T>` otherwise, behind one set of members. Its
members are [the table under `Items<T>`](#itemst-the-storage-chosen-for-you), which is normative. The readings:

- **One class, folded.** `library/items.spite` is one generic class. Its storage is the same for both kinds (a
  heap block, a count and a capacity), and each body that touches an item folds on `$element_type.is_fixed_size`
  ([metaprogramming.md](metaprogramming.md#asking-whether-a-class-fits-a-vector)): the inline branch goes through
  `InlineMemory<T>`, the reference branch through `TypedMemory<T>`, and the branch not taken is not compiled. So a
  `T` that does not fit never makes a `Vector<T>`, never meets the item errors of
  [Vector](#vectort), and never has an item function written for it. Both helpers are singletons
  bound as attributes, so an `Items` object holds one pointer more than a `Vector` or a `List`, one per
  collection and never per item. No syntax is needed: the language already folds a body on `is_fixed_size`, and
  the storage needs no attribute of its own per kind.
- **Which kind.** `T` fits exactly when a `Vector<T>` could be made (a `Vector`'s rule: a `String`, or a class made
  only of numbers, `Boolean`s, enums and `String`s, with no `drop()` and no function that uses `this` as a value).
  A union, a `type`, a `List`, a `Dictionary` or a class holding one is kept by reference. `Items<String>` is a
  plain array of the text's references, as a vector's is. A number, a `Boolean`, an enum or a `Memory.Address` is
  never an item: `Items<Integer>` is the error naming `List<Integer>`, as for a `Vector`.
- **Reading.** `items[index]` and `get_at(index)` are one function and answer a `T?` for both kinds,
  `null` out of range. A read is
  narrowed as a list's is, by `crash`, `assert`, `if` or a proof the compiler holds. Then, inline, the item is
  borrowed, and every rule of [memory.md's borrowed items](memory.md#borrowed-items-of-a-vectort)
  applies to it, rows included. By reference, it is a counted reference like a list element: it may be kept,
  passed, returned and read after the collection changes size. Code reading an item still compiles the same
  whichever kind a class falls into, since both kinds answer `T?`, and a walked row states its reads with
  `crash` lines ([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)).
- **Items of a `T?`.** `Items<String?>`'s `[]` answers the `String?` that was
  stored, and `crash names[0]`, `assert names[0]` or `if names[index] { }` narrows that item in place until the
  collection or the index changes, as a path is narrowed (`conformance/stage6/items_narrowed`). A loop's
  `index < names.count()` proves only that the index is in range, not that the item is there: reading
  `names[index].upper_case()` under it alone is still `this value may be null (it is a String?)`
  (`diagnostics/items_nullable_unproven`).
- **The borrow checks apply only where the storage is inline.** A class that changes kind can meet new errors
  where it is read, and each names the choice first: `'Velocity' fits a Vector, so the items of 'velocities' are
  borrowed: 'stored' is borrowed from 'velocities' and cannot be kept in the attribute 'kept': keep
  'stored.copy()', an independent object`, and the same for a return, an argument, a list, a second name, a
  function value, a row that is kept, and a read after a line that may change the size
  (`diagnostics/items_borrows`). What may change the size is what may change a `Vector`'s, with
  `remove_swapping` beside `remove_at`: called on the collection, or through a function that calls one, where a call
  that swaps an item out counts as a removal.
- **Writing and removing.** `append` and `set_at` copy the value's attributes in when inline (counting each
  `String` attribute once more) and keep the value when by reference; `remove_at`, `remove_swapping`, `clear` and
  dropping the collection release what they remove. `remove_swapping(index)` moves the last item into `index`
  and does nothing out of range; it keeps no order, and costs the same however many items there are.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_` and `sum_<member>()` are in
  `library/items.spite`, each folded the same way; `filter_` answers an `Items<T>` (copies when inline, the same
  references otherwise) and `map_` a `List` of the members' values. A chain is one loop over the block,
  and `parallel_each_<member>()` splits it across the thread pool, as for a vector. The passed-function forms
  (`each(f)`, `map(f)`, ...) are not offered for either kind, since an inline item is never passed on and plain
  values live in a `List`: `'each'
  passes each item to a function, and 'Velocity' fits a Vector, so an item of these Items is borrowed from them,
  never passed on: give the item's class a function and call it with a member template, such as
  'each_<function>()'`; by reference the error says that `Items` does not do it whichever storage it chose.
- **Cost.** Nothing runs to choose: the folds are decided while compiling and the untaken branch is absent from
  the C, so an `Items<Velocity>` compiles to a `Vector<Velocity>`'s code and an `Items<Trail>` to a
  `List<Trail>`'s, reading its elements uncounted in the templates as a list does
  ([optimizations.md](optimizations.md#a-lists-templates-read-its-elements-without-counting-them)). The range
  check of `[]` is one comparison that answers `null`, small enough for the C compiler to inline, and a proven
  read uses the item without testing that answer again. A program that makes no `Items` carries none of it. Measured in
  `benchmarks/items_storage`.

`conformance/stage6/items_columns`, `conformance/stage6/plain_items`, `diagnostics/items_borrows`,
`diagnostics/plain_items`, `diagnostics/text_items_passed`.

### Removing many at once

`List`, `Vector` and `Items` remove many elements in one pass:

- **`remove_where(test)` and `remove_where_<member>()`.** One template, `remove_where_member(member:
  Spite.Attribute<$element_type>)` in each of `library/list.spite`, `vector.spite` and `items.spite`, so it answers both a
  member (`creatures.remove_where_dead()`: the member returns `Boolean`) and a passed function
  (`numbers.remove_where(is_odd)`: on a `List` of anything, never on a `Vector` or `Items`, whose items
  are borrowed or text, and a borrowed item is never passed on). It walks the collection once; an element that stays is
  exchanged with the first place not yet kept, and when the walk ends everything past the kept ones is released.
  The survivors keep their order, and the test sees every element once, first to last.
- **Why exchange, not move.** At every moment of the walk the collection holds each of its original elements
  exactly once, some of them already moved down: no slot is ever empty, a copy, or released while the collection
  still counts it. So whatever the test does to the collection (read it, append to it, even remove from it),
  nothing can be released twice or read after it was released; it only sees the elements in a different order.
  Moving an element down would leave its old slot holding a second copy that a test could release, or a blank that
  is no valid element. The exchange costs one more copy per element that stays, which measured about a third
  slower than moving at half removed and the same at nine in ten (`benchmarks/bulk_removal`).
- **`truncate(count)`** keeps the first `count` elements and releases the rest; a `count` below zero or not below
  the size does nothing. **`swap(first, second)`** exchanges two elements, doing nothing when either index is out
  of range; with `truncate` it is how a pass of the program's own removes by something other than a function of
  the element, such as a mask of rows.
- **Chains.** `remove_where` changes its receiver, so it is not a step of a fused chain, and calling it on
  a chain is an error: `'remove_where' removes from the collection it is called on, and 'names.filter(is_short)'
  makes a new one that nothing keeps: call it on the collection itself, with one test that says everything to
  remove`. A test returning anything but `Boolean` is `'remove_where(measure)': 'measure' is an Integer, but
  'remove_where' needs it to return Boolean`.
- **Borrows and proofs.** All three count as shrinking wherever `remove_at` does: on the collection itself, and
  through the call effects of functions (a function that calls one of them records `shrink:` on the collection). So an item
  borrowed from a `Vector` or `Items` is not read after one (`'first' is borrowed from 'velocities', and
  'velocities.truncate()' on line 34 may move the items of 'velocities', ...`), a row's system may not reach
  one for a vector it borrows from, and a proof about an element is undone by one. `swap` changes no size but
  changes which element an index names, so it counts too.
- **Cost.** Only what a program calls is compiled. `swap` is three copies of the element through a temporary on the
  C stack (`TypedMemory.swap_values`, `InlineMemory.swap_items`, written per element type by the generator), with
  no allocation. Measured at 200 000 `Items<Velocity>` items and a `List<Integer>` beside them: half removed,
  340 µs against 370 µs for a `remove_swapping` per removed row while walking the rows and 530 µs for one per
  entity of a despawn list; nine in ten removed, 290 µs against 490 µs and 930 µs.

`conformance/stage6/bulk_removal`, `diagnostics/bulk_removal`, `benchmarks/bulk_removal`.

### There is no `Heap<T>`

`Heap<T>`, a box for a value, does not exist (it is not `Memory.Heap`, the allocator). References are the default
([Memory](memory.md#the-memory-model)), so a class or union containing itself recursively just declares an
ordinary attribute of type `T`: no indirection is needed, and there is no struct-layout cycle to guard against,
since a class is always a heap object referred to by pointer. A `union Expression` of `NumberExpression` and
`BinaryExpression`, with `var left: Expression? = null` in `binary_expression.spite`, is the whole of it;
[memory.md](memory.md#self-referential-classes-and-unions-just-work) has a tree that runs.

Writing `Heap<T>`/`Heap<T>(value)` is a compile error naming this fix: `there is no 'Heap<T>': a class, a list and
a text are references already, so a class that holds one of its own kind declares an ordinary attribute, like
'var left: Expression? = null' ('Memory.Heap' is the allocator, for containers of your own)`
(`diagnostics/old_list_names`).

---

Next: [JSON and binary](json.md), the classes that turn any value into JSON or bytes and back.
