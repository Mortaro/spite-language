# Lists, dictionaries and member templates

`List<T>` and `Dictionary<Key, Value>` are not built into the compiler. They are ordinary generic classes written in Spite
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
| `list[index] = value` | | calls `set_at(index, value)`; an index out of range halts, naming the line |
| `get_at(index)` | `T?` | what `list[index]` calls, `null` out of range; it is only called through `[]` ([Use the operator](functions_and_operators.md#use-the-operator-not-its-function)) |
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
| `index_of(value)` | `Integer?` | the place of the first element equal to `value`, or `null` when none is; the same elements as `contains` |
| `clear()` / `reverse()` | | in place; `clear()` keeps the buffer's capacity |
| `join(separator)` | `String` | every element becomes text: a `String`, a number, a `Boolean`, an enum value |
| `copy()` / `deep_copy()` | `List<T>` | one level, or all the way down ([memory.md](memory.md)) |
| `to_utf8_text()` | `String?` | a `List<Byte>` only: the text those bytes spell, or `null` when they are not valid UTF-8 ([standard_library.md](standard_library.md#bytes-base64-compression-hashes-and-passwords)) |

`append`, `remove_last`, `list[index]` and their kin take the same time however long the list is; `prepend`,
`insert`, `remove_first` and `remove_at` move every later element, and `contains` looks at each in turn. An empty
list has no buffer; the first element gets one of four slots, and a full buffer is resized to twice its size.

Names say where: `append` and `prepend`, never `add`; `remove_last`, never `pop`
([the rule](../specs/collections.md#listt-additions)).

## `Dictionary<Key, Value>`

A dictionary names two types: what it is keyed by, then what it holds. `Dictionary<String, Integer>` finds a count
by a name, `Dictionary<Integer, Monster>` a monster by its id, and `Dictionary<Player, Party>` a party by the player
itself. Entries keep the order they were inserted in, and `get`, `has`, `set` and `[]` take the same time however
many keys there are, because it is a hash table over two lists. `remove` is the exception: it moves every later
entry down, as `remove_at` does on a list.

| Member | Result | Notes |
|---|---|---|
| `Dictionary<Key, Value>()` | | an empty one, with no table until the first key |
| `dictionary[key] = value` | | replaces the value of a key already there; it calls `set_at(key, value)`, which is only called through `[] =` |
| `dictionary[key]` | `Value?` | `null` when the key is absent; it calls `get_at(key)`, which is only called through `[]` |
| `has(key)` | `Boolean` | |
| `remove(key)` | | nothing happens when the key is absent |
| `count()` | `Integer` | |
| `keys()` / `values()` | `List<Key>` / `List<Value>` | a fresh list, in insertion order |
| `copy()` / `deep_copy()` | `Dictionary<Key, Value>` | |

```gdscript title=dictionary_tasks/dictionary_tasks.spite entry
var console = Console()

func DictionaryTasks() {
    var inventory = Dictionary<String, Integer>()
    inventory["sword"] = 1
    inventory["potion"] = 4
    inventory["potion"] = 6
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

A dictionary whose entries are known when it is written is a **dictionary literal**: each entry a key, a colon and
a value, apart with commas or one per line, as a list literal's items are (the formatter puts a short one on one
line). Its keys are text or whole numbers written out. Stored where a dictionary is declared, it takes the declared
key and value types; otherwise its keys are `String` or `Integer`, as its first key is, and its value type is the
first value's:

```gdscript title=dictionary_literals/dictionary_literals.spite entry
var console = Console()

func DictionaryLiterals() {
    var plurals = {"cactus": "cacti", "foot": "feet"}
    var squares = {1: 1, 2: 4, 3: 9}
    var weights: Dictionary<String, Float> = {"feather": 0.01, "anvil": 50}
    crash plurals["cactus"]
    crash squares[3]
    crash weights["anvil"]
    console.print(plurals["cactus"], squares[3], weights["anvil"])
}
```
```output
cacti 9 50
```

A key written twice is an error (`"cactus" is written twice in this dictionary: keep one entry for each key`), and
an empty dictionary is made with `Dictionary<Key, Value>()`, since `{}` names no types.

### Any type is a key

The key type is what you look up by, and the dictionary finds a key the way that type means it:

- **numbers** by their value: `Dictionary<Integer, String>` stores and hashes the number itself, never text made
  from it, and `keys()` answers the numbers. A fraction is found by its value too, so `0.0` and `-0.0` are one key,
  and so is every `not_a_number`;
- **text** by its bytes, and a `Boolean` by whether it is `true`;
- **an enum value** as itself: `Dictionary<Color, Integer>` holds one entry for `'red'`, whatever its name is
  written as;
- **an object** as that one object: two players with the same name are two keys, and the dictionary holds each
  key, so an object cannot be freed while it is a key of one.

```gdscript title=number_keys_tasks/number_keys_tasks.spite entry
var console = Console()
var names = Dictionary<Integer, String>()

func NumberKeysTasks() {
    names[3] = "fern"
    names[11] = "moss"
    names[7] = "reed"
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

```gdscript title=party_keys/player.spite
var name = ""

func Player(new_name: String) {
    name = new_name
}
```
```gdscript title=party_keys/party_keys.spite entry
var console = Console()

func PartyKeys() {
    var ada = Player("ada")
    var other_ada = Player("ada")
    var parties = Dictionary<Player, String>()
    parties[ada] = "red team"
    crash parties[ada]
    var other_found = parties.has(other_ada)
    console.print(ada.name, "plays for", parties[ada], "and the other ada is found:", other_found)
}
```
```output
ada plays for red team and the other ada is found: false
```

A class that declares its own `equals` says two of its objects can be equal without being one object, and a
dictionary keyed by it would miss the equal one, so it is a compile error: key the dictionary by what `equals`
compares instead, an id or a name. A key given where its type is not wanted is the same mismatch as any argument,
and the [specification](../specs/collections.md#dictionarykey-value) lists every rule and error.

The key and value types are what the program means, not how it is stored: the compiler keeps a dictionary however
is fastest, as long as every lookup answers the same ([write it plainly](write_it_plainly.md#a-class-is-what-you-mean-not-how-it-is-stored)).

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
| `vector[index]` (calls `get_at(index)`) | `T?` | the item itself, borrowed once narrowed; `null` out of range |
| `vector[index] = value` (calls `set_at(index, value)`) | | copies `value` over the item; out of range halts |
| `remove_at(index)` | | moves every later item down; an index out of range halts |
| `remove_where_<member>()` / `truncate(count)` / `swap(first, second)` | | as on a list ([below](#removing-many-at-once)); `remove_where(f)` is a `List`'s only |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `reserve(count)` | | makes room for `count` items in all without making any, so appending up to there never grows the block |
| `copy()` / `deep_copy()` | `Vector<T>` | a new vector with its own copy of every item |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()` | | as on a list; `filter_` gives a `Vector<T>` of copies |
| `find_by_<member>(value)` | `T?` | the first item whose member equals `value`, borrowed once narrowed; `null` when none does |
| `find_index_by_<member>(value)` | `Integer?` | the place of that item, or `null` when none matches |
| `sort_by_<member>()` | `Vector<T>` | a new vector with a copy of every item, sorted ascending and stable, as on a list |
| `parallel_each_<member>()` | | as on a list ([concurrency.md](concurrency.md#parallel_each_-a-member-on-every-element)) |

An item is a class of known size: its attributes are only numbers, `Boolean`, enums and `String`s. A class with a
`List`, a `Dictionary` or another object in an attribute is an error naming the attribute, and it belongs in a
`List` ([the rules](../specs/collections.md#vectort)). A `String` may be an item too, held as a counted reference.

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
[memory.md](../specs/memory.md#borrowed-items-of-a-vectort). Items of several vectors can travel together as
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
| `items[index]` (calls `get_at(index)`) | `T?` | inline: the item, borrowed once narrowed; references: the reference; `null` out of range, either way |
| `items[index] = value` (calls `set_at(index, value)`) | | replaces the item; out of range halts |
| `remove_at(index)` | | moves every later item down; an index out of range halts |
| `remove_swapping(index)` | | moves the last item into `index`; an index out of range halts |
| `remove_where_<member>()` / `truncate(count)` / `swap(first, second)` | | as on a list ([below](#removing-many-at-once)); `remove_where(f)` is a `List`'s only |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `copy()` / `deep_copy()` | `Items<T>` | inline: every item copied; references: one level, or all the way down |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()`, `parallel_each_<member>()` | | as on a vector; `filter_` gives an `Items<T>`, and a chain is one loop |
| `find_by_<member>(value)` | `T?` | the first item whose member equals `value`: inline, borrowed once narrowed; references, the reference; `null` when none does |
| `find_index_by_<member>(value)` | `Integer?` | the place of that item, or `null` when none matches |
| `sort_by_<member>()` | `Items<T>` | sorted ascending and stable: inline, a copy of every item; references, the same references |

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
([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)). The rules are [in the specification](../specs/collections.md#itemst).

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
and the ones removed are released at the end ([its benchmark](../benchmarks/removing_many_at_once/) measures it
against the same removal written in C). What stays keeps its order, which one `remove_swapping` per item does
not. A test that is not a function of the element alone (a mask read by row) is written as the loop above:
`swap` each row that stays down to the next free place, then `truncate`.

`remove_where` changes the collection it is called on, so it ends no chain: `names.filter(is_short).remove_where(is_long)`
is an error naming the fix, one test that says everything to remove. Like `remove_at`, all three may move items,
so an item borrowed from a `Vector` or `Items` is not read after one ([the rules](../specs/collections.md#removing-many-at-once)).

## Member templates: loops you do not write

A list of a class answers a family of functions named after the element's members (a function of your own is
passed instead, [below](#passing-a-function-for-each-element)). `chores.count_done()` counts
the chores whose `done` is true, `items.sum_price()` adds up their prices, and `repositories.map_names()` collects
their names. A **member** is an attribute or a function that takes no arguments (the two are the same to a
template, since reading an attribute already goes through its getter), and a template is compiled only for the
names a program calls. Text has members too: `names.map_upper_cases()` collects every name in capitals,
`names.filter_is_empty()` keeps the empty ones and `names.sort_by_length()` sorts them by length
(`conformance/stage6/text_member_templates`).

| Template | Result | The member must |
|---|---|---|
| `filter_<member>()` | `List<T>`, the elements where it is true | take nothing, return `Boolean` |
| `count_<member>()` | `Integer`, how many are true | take nothing, return `Boolean` |
| `any_<member>()` / `all_<member>()` | `Boolean` | take nothing, return `Boolean` |
| `sum_<member>()` | the member's number type | take nothing, return a number |
| `find_by_<member>(value)` | `T?`, the first element whose member equals `value` | return something comparable to `value` |
| `find_index_by_<member>(value)` | `Integer?`, the place of that element | return something comparable to `value` |
| `sort_by_<member>()` | `List<T>`, sorted ascending, stable, in `n log n` time | return a number or a `String` |
| `map_<members>()` | `List<U>`, one value per element | return a value |
| `each_<member>()` | nothing: calls it on every element | be a function that takes nothing; what it returns is discarded |
| `remove_where_<member>()` | nothing: removes the elements where it is true, keeping the rest in order | take nothing, return `Boolean` |

A member that does not fit is a compile error naming the member, what it is, and what the template needs.
`count_` on a number is one of them, and names `sum_` instead: `count()` is only ever a collection's size.
`each_` on an attribute is another: reading a value only to discard it does nothing. `map_` on a test is a third:
a `Boolean` member is a question about each element, never a value to collect, so `monsters.map_is_alive()` names
`filter_is_alive()`, `count_is_alive()`, `any_is_alive()` and `all_is_alive()` instead.

A value computed from each element is a member too. There is no `map(function)`: give the element's class a
read-only attribute that computes it (a `get_<name>()` with no setter,
[functions_and_operators.md](functions_and_operators.md#settergetter-interception)) and collect it with
`map_<name>()`. The value then has a name, every list of that class can collect it, and a chain fuses it like any
other member:

```gdscript title=computed_member/monster.spite
var health = 0

func Monster(starting_health: Integer) {
    health = starting_health
}

func get_doubled_health(): Integer {
    return health * 2
}
```
```gdscript title=computed_member/computed_member.spite entry
var console = Console()

func ComputedMember() {
    var monsters = [Monster(3), Monster(5)]
    var doubled = monsters.map_doubled_health()
    var joined = doubled.join(", ")
    console.print(joined)
}
```
```output
6, 10
```

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
    var titles = chores.map_titles()
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
once more and not copied. A `Dictionary` answers every template through its values: `inventory.sum_price()` is
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
counter walking a list, a `Vector` or an `Items` from `0` to its `count()`, doing nothing with each element but what one template does
with one of its members, or what `each`, `filter`, `count`, `sum`, `find`, `any` or `all` does with a
function passed the element, on a list of anything, numbers and text included. Loops that need the index, pass
more than the element, stop early for another reason, or walk state keep their `while`. The exact shape the
compiler looks for is in [control_flow.md's rules](../specs/control_flow.md#a-while-that-a-member-template-already-says).

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
    var names = starting_with_a.map_names()
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

```gdscript title=post_stages/stage.spite
enum Stage {
    'draft'
    'published'
    'archived'
}
```
```gdscript title=post_stages/post.spite
var title = ""
var stage: Stage = 'draft'

func Post(new_title: String, new_stage: Stage) {
    title = new_title
    stage = new_stage
}
```
```gdscript title=post_stages/post_stages.spite entry
var console = Console()
var posts = List<Post>()

func PostStages() {
    var hello = Post("hello", 'published')
    posts.append(hello)
    var plans = Post("plans", 'draft')
    posts.append(plans)
    var old = Post("old", 'archived')
    posts.append(old)
    var published = posts.filter_published()
    var titles = published.map_titles()
    var shown = titles.join(", ")
    var drafts = posts.count_draft()
    var anything_archived = posts.any_archived()
    posts.remove_where_archived()
    var left = posts.count()
    console.print(shown, drafts, anything_archived, left)
}
```
```output
hello 1 true 2
```

`posts.filter_published()` means exactly `posts.filter_stage_is_published()` would, had `Post` a
`func stage_is_published(): Boolean { return stage == 'published' }`: one loop, one comparison of two small
integers per element, and a chain of them fuses like any other ([below](#chains-run-as-one-loop)). A directory
lists only its files the other way, by class: `directory.entries()` is a list of the union of `Directory` and `File`,
and `filter_files()` keeps the `File` items as a `List<File>` ([metaprogramming.md](metaprogramming.md#member-templates),
[standard_library.md](standard_library.md#list-a-directory)).

## Passing a function for each element

A template sees only the element and the list: `names.each_say_hello()` looks for a member `say_hello` of each
name, never for a function of the class the call is written in. A function of yours is passed as a value
instead: `names.each(say_hello)` calls `say_hello` once for every name, in order, and there is no
`say_hello_to_everyone` to write. The function is bound to whoever owns it ([functions_and_operators.md](functions_and_operators.md#functions-are-values)):
`say_hello` alone is this instance's, and `people.each(greeter.greet)` calls `greet` on `greeter`. The library's
classes are no different: `keys.filter(counts.has)` asks the dictionary `counts`, and
`words.filter(greeting.contains)` asks the text `greeting`.

`each`, `filter`, `any`, `all`, `count`, `find`, `sort_by` and `sum` each take such a function, which takes
the element as its only argument: `filter`, `any`, `all`, `count` and `find` want one returning `Boolean` (`find`
answers the first element it is true for, or `null`), `sum` one returning a number, `sort_by` one returning a
number or text. There is no `map(function)`: a value collected from each element is a member of the element
([above](#member-templates-loops-you-do-not-write)), named and collected with `map_<member>()`. This works on a list of anything (numbers included, which
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
    var letters = names.filter(is_short).sum(doubled_length)
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

func doubled_length(name: String): Integer {
    return name.length() * 2
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
in a variable (`var quiet = greeter.is_quiet`, then `names.filter(quiet)`) is called through the value. A function that
does not fit is an error naming what the template needs:

```gdscript title=passed_function_mistake/passed_function_mistake.spite entry error
var console = Console()

func PassedFunctionMistake() {
    var names = ["Ada", "Grace"]
    var greetings = names.filter(say_hello)
    console.print(greetings)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}
```
```diagnostic
'filter(say_hello)': 'say_hello' returns nothing, but 'filter' needs it to return Boolean
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

Every template takes what the one before it gives, so they chain: `map_<members>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as (that is what it means), but the compiler runs it as **one loop over
the first list**, with no list in between: `teams.filter_active().map_leads().sum_age()` visits each team once,
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
    var ages = teams.filter_active().map_leads().sum_age()
    var active = teams.filter_active()
    var leads = active.map_leads()
    var ages_step_by_step = leads.sum_age()
    console.print(ages, ages_step_by_step)
}
```
```output
34 34
```

Only a call made directly on another template call is part of the chain: `active.map_leads()` above starts a new
one, because `active` is a list the program named and kept. The steps in the middle are `map_` (to a member that
is a class) and `filter_`; the last call can be any template, and a chain the compiler cannot write as one loop
is run step by step, which means the same. The one visible difference is order: a member function in a fused
chain runs element by element, where the steps written out would run it on every element before the next step
starts. Written as three steps, the chain above would make two lists every time it ran; fused, it makes none
([the rules](../specs/collections.md#standard-library-metaprogramming) name the test that pins the count).

## How the member templates are written

The templates are ordinary Spite in `library/list.spite`, over the list's own buffer on the heap. Each one is a
template ([metaprogramming.md](metaprogramming.md#member-templates)) whose parameter is
`member: Spite.AttributeDeclaration<$element_type>`: it names a member of the *element*, and `item.attributes[member]` reads
it: the field itself, or a call to the zero-argument function. `filter_member` answers `filter_in_stock`,
`filter_is_popular` and every other `filter_<member>` call:

```gdscript
func filter_member(member: Spite.AttributeDeclaration<$element_type>): List<$element_type> {
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

A `Dictionary`'s templates are written the same way, in `library/dictionary.spite`: their parameter is
`member: Spite.AttributeDeclaration<$value_type>`, and each walks a copy of the values, `var listed = values()`,
so `inventory.sum_price()` is `inventory.values().sum_price()` and a member that changes the dictionary while the
template runs changes nothing the walk sees. There is no `remove_where_` among them, since removing from that copy
would remove nothing.

## Write your own member template

A program reopens `List` by putting a `list.spite` in its own folder, and a function there with a
`Spite.AttributeDeclaration<$element_type>` parameter named after a word of its name becomes one more template, exactly like the
library's:

```gdscript title=list_average/list.spite
func average_member(member: Spite.AttributeDeclaration<$element_type>): Float? {
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

A `Vector` and an `Items` are reopened the same way, by a `vector.spite` or an `items.spite` in the program's
folder; their items are read with `values.item_at(items, index)` (an `Items`, with `inline.item_at` or
`references.read_value` under `$element_type.fits_vector()`), and an item read so is borrowed, as everywhere
in the program (`conformance/stage6/own_collection_templates`).

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

---

Next: [JSON and binary](json.md), the classes that turn any value into JSON or bytes and back.
