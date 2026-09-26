# Lists, dictionaries and member templates

`List<T>` and `Dictionary<T>` are not built into the compiler. They are ordinary generic classes written in Spite
-- `library/list.spite` and `library/dictionary.spite` -- over `Memory.Heap` and `Memory.Address`, the floor
everything else is built on. The compiler keeps only the syntax: `[1, 2, 3]`, `list[index]`, and `List<T>()`. Read
those two files to see exactly what a list does; write your own container the same way
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).

Being ordinary classes, they cost what their code costs and nothing more ([D177](decisions.md)): a list is one heap
buffer of its elements, a program compiles only the members it calls, and one that never makes a `Dictionary`
carries none of it. Nothing runs behind them -- no collector, no iterator objects, no registry of templates.

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
| `list[index] = value` | | the explicit form is `set_at(index, value)`; nothing happens out of range |
| `get_at(index)` | `T` | the default when out of range |
| `remove_at(index)` | | nothing happens out of range |
| `remove_first()` / `remove_last()` | `T?` | removes and returns it; `null` when empty, like `first()` (D211) |
| `first()` / `last()` | `T?` | `null` when empty, like `[]` (proposed by Claude, unconfirmed): narrow it, `crash first` or `if first { }` |
| `count()` / `is_empty()` | `Integer` / `Boolean` | `count()` is only ever the list's size |
| `contains(value)` | `Boolean` | elements that are numbers, `Boolean`, `String` or an enum only |
| `clear()` / `reverse()` | | in place; `clear()` keeps the buffer's capacity |
| `join(separator)` | `String` | every element becomes text: a `String`, a number, a `Boolean`, an enum value |
| `copy()` / `deep_copy()` | `List<T>` | one level, or all the way down ([memory.md](memory.md)) |

`append`, `remove_last`, `list[index]` and their kin take the same time however long the list is; `prepend`,
`insert`, `remove_first` and `remove_at` move every later element, and `contains` looks at each in turn. An empty
list has no buffer; the first element gets one of four slots, and a full buffer is resized to twice its size.

Names say where: `append` and `prepend`, never `add`; `remove_last`, never `pop`
([the rule](#listt-additions--implemented)).

## `Dictionary<T>`

Keys are `String`, entries keep the order they were inserted in, and `get`, `has`, `set` and `[]` take the same
time however many keys there are -- it is a hash table over two lists. `remove` is the exception: it moves every
later entry down, as `remove_at` does on a list.

| Member | Result | Notes |
|---|---|---|
| `Dictionary<T>()` | | an empty one, with no table until the first key |
| `set(key, value)` / `dictionary[key] = value` | | replaces the value of a key already there |
| `get(key)` / `dictionary[key]` | `T?` | `null` when the key is absent |
| `has(key)` | `Boolean` | |
| `remove(key)` | | nothing happens when the key is absent |
| `count()` | `Integer` | |
| `keys()` / `values()` | `List<String>` / `List<T>` | a fresh list, in insertion order |
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

## `Vector<T>`: items inline

A `List` holds references: each element is an object somewhere on the heap, and walking the list follows one
pointer per element. A `Vector<T>` (`library/vector.spite`) holds its items themselves, one after another in one
block of memory, the way the processor's cache likes to read them ([D154](decisions.md), [D203](decisions.md)).
An item has no header and no reference count of its own; appending one copies its attributes into the block.

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

`velocities[0]` is not a copy: it is the item inside the vector, **borrowed** ([D204](decisions.md)), so
`first.down = 2.0` writes the vector's own item. `slow` is still `0.5`, because `append` copied it in. The member
templates run on the items in place the same way, and a chain of them is one loop, as on a list.

| Member | Result | Notes |
|---|---|---|
| `append(value)` | | copies `value`'s attributes in as a new last item |
| `vector[index]` / `get_at(index)` | `T` | the item itself, borrowed; a plain value (a number, `Boolean` or enum) is copied instead; an index out of range halts |
| `vector[index] = value` / `set_at(index, value)` | | copies `value` over the item; out of range halts |
| `remove_at(index)` | | moves every later item down; nothing happens out of range |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `reserve(count)` | | makes room for `count` items in all without making any, so appending up to there never grows the block (proposed by Claude, unconfirmed; D208) |
| `copy()` / `deep_copy()` | `Vector<T>` | a new vector with its own copy of every item |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()` | | as on a list; `filter_` gives a `Vector<T>` of copies |
| `each(f)`, `map(f)`, `filter(f)`, `count(f)`, `any(f)`, `all(f)`, `sum(f)` | | only for plain values, as on a list ([below](#passing-a-function-for-each-element)); `filter(f)` gives a `Vector<T>` |
| `parallel_each_<member>()` | | as on a list ([concurrency.md](concurrency.md#parallel_each_-a-member-on-every-element)) |

An item is of known size: a number, a `Boolean`, an enum, a `String`, or a class whose attributes are only those.
A class with a `List`, a `Dictionary` or another object in an attribute is an error naming the attribute, and it
belongs in a `List` ([the rules](#vectort--implemented)).

Only an item that is a class is borrowed. A plain value -- a number, a `Boolean`, an enum -- is copied as it is
read ([D221](decisions.md)), since copying it costs less than borrowing it, so `var first = numbers[0]` is a number
of its own that may be kept, passed, returned and read after `numbers` grows.

A borrowed item is read and written through its members. It is never kept: not in an attribute, a list, a
returned value or a function value, and not past anything that may change the vector's size or move its block,
since growing the vector -- `reserve(count)` too, which adds no item -- may move its items. Each of those is an error that names `copy()`, which makes an independent object:

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
[memory.md](memory.md#borrowed-items-of-a-vectort--implemented). Items of several vectors can travel together as
a **row**, an object literal a system takes as a `type` for one call, which is how an engine joins its component
columns ([memory.md](memory.md#a-row-of-borrowed-items-for-one-call), [D206](decisions.md)).

## `Items<T>`: the storage chosen for you

A generic class that keeps values of a type it does not know -- an engine's `Column<$component_type>` -- cannot
say `Vector` or `List` without knowing whether the type fits a `Vector`. `Items<T>` (`library/items.spite`, the
name provisional) answers that while compiling ([D218](decisions.md)): when `T.fits_vector()` its items are
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
    var first = velocities[0]
    first.across = 2.0
    var trails = Items<Trail>()
    var trail = Trail()
    trails.append(trail)
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

`Velocity` fits a `Vector`, so `velocities[0]` is the item inside the block, borrowed, and `slow` keeps its own
`1.0`. `Trail` holds a `List`, so it does not fit, and `trails[0]` is `trail` itself, a counted reference like a
list's element: writing through it writes `trail`. `remove_swapping(0)` moves the last item into the hole instead
of moving every later one down, which is what a sparse set wants.

| Member | Result | Notes |
|---|---|---|
| `append(value)` | | inline: copies `value`'s attributes in; references: keeps `value` |
| `items[index]` / `get_at(index)` | `T` | inline: the item, borrowed, or a copy of a plain value; references: the reference; out of range halts, either way |
| `items[index] = value` / `set_at(index, value)` | | replaces the item; out of range halts |
| `remove_at(index)` | | moves every later item down; nothing happens out of range |
| `remove_swapping(index)` | | moves the last item into `index` (name provisional); nothing happens out of range |
| `count()` / `is_empty()` / `clear()` | | `clear()` keeps the block's capacity |
| `copy()` / `deep_copy()` | `Items<T>` | inline: every item copied; references: one level, or all the way down |
| `each_`, `map_`, `filter_`, `count_`, `any_`, `all_`, `sum_<member>()`, `parallel_each_<member>()` | | as on a vector; `filter_` gives an `Items<T>`, and a chain is one loop |
| `each(f)`, `map(f)`, `filter(f)`, `count(f)`, `any(f)`, `all(f)`, `sum(f)` | | only for plain values, as on a vector; `filter(f)` gives an `Items<T>` |

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

An engine's column holds its values in an `Items`, and D217's walked row reads them the same way for both kinds
([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)). The rules are [below](#itemst--implemented-the-name-provisional).

## Member templates: loops you do not write

A list of a class answers a family of functions named after the element's members (a function of your own is
passed instead, [below](#passing-a-function-for-each-element)). `chores.count_done()` counts
the chores whose `done` is true, `items.sum_price()` adds up their prices, and `repositories.map_name()` collects
their names. A **member** is an attribute or a function that takes no arguments -- the two are the same to a
template, since reading an attribute already goes through its getter -- and a template is compiled only for the
names a program calls.

| Template | Result | The member must |
|---|---|---|
| `filter_<member>()` | `List<T>`, the elements where it is true | take nothing, return `Boolean` |
| `count_<member>()` | `Integer`, how many are true | take nothing, return `Boolean` |
| `any_<member>()` / `all_<member>()` | `Boolean` | take nothing, return `Boolean` |
| `sum_<member>()` | the member's number type | take nothing, return a number |
| `find_by_<member>(value)` | `T?`, the first element whose member equals `value` | return something comparable to `value` |
| `sort_by_<member>()` | `List<T>`, sorted ascending | return a number or a `String` |
| `map_<member>()` | `List<U>`, one value per element | return a value |
| `each_<member>()` | nothing: calls it on every element | be a function that takes nothing; what it returns is discarded |

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

`filter_<member>()` and `sort_by_<member>()` return a new list holding the same elements -- each one referenced
once more, not copied. A `Dictionary<T>` answers every template through its values: `inventory.sum_price()` is
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

**A `while` that only does what a template does is an error naming the template** ([D171](decisions.md)): a
counter walking a list from `0` to its `count()`, doing nothing with each element but what one template does
with one of its members, or what `each`, `map`, `filter`, `count`, `sum`, `find`, `any` or `all` does with a
function passed the element -- on a list of anything, numbers and text included. Loops that need the index, pass
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

## Passing a function for each element

A template sees only the element and the list: `names.each_say_hello()` looks for a member `say_hello` of each
name, never for a function of the class the call is written in. A function of yours is passed as a value
instead: `names.each(say_hello)` calls `say_hello` once for every name, in order, and there is no
`say_hello_to_everyone` to write. The function is bound to whoever owns it ([functions_and_operators.md](functions_and_operators.md#functions-are-values)):
`say_hello` alone is this instance's, and `people.each(greeter.greet)` calls `greet` on `greeter`.

`each`, `map`, `filter`, `any`, `all`, `count`, `find`, `sort_by` and `sum` each take such a function, which takes
the element as its only argument: `filter`, `any`, `all`, `count` and `find` want one returning `Boolean` (`find`
answers the first element it is true for, or `null`), `sum` one returning a number, `sort_by` one returning a
number or text, `map` one returning anything. This works on a list of anything -- text and numbers included, which
have no members of their own for a template to name -- and on a `Dictionary`, through its values.

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

A list's own functions are passed the same way, so `numbers.each(found.append)` copies every number into `found`.
A `Vector` or an `Items` of plain values -- numbers, `Boolean`, enums -- takes every form but `find` and `sort_by`,
because its items are copied as they are read and there is nothing borrowed to pass on
([D221](decisions.md)); a chain of them is one loop over the block:

```gdscript title=plain_vector/plain_vector.spite entry
var console = Console()

func PlainVector() {
    var weights = Vector<Float>()
    weights.append(1.5)
    weights.append(2.5)
    weights.append(4.0)
    var found = List<Float>()
    weights.each(found.append)
    var heavy = weights.filter(is_heavy).sum(double_of)
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

An item that is a class is borrowed and never passed on, and a `String` item is not a plain value here
(`velocities.each(f)` is an error naming a member template instead; `names.each(f)` on a `Vector<String>` says to
keep the text in a `List<String>`).

## Chains run as one loop

Every template takes what the one before it gives, so they chain: `map_<member>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as -- that is what it means -- but the compiler runs it as **one loop over
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
([the rules](#standard-library-metaprogramming--partial) name the test that pins the count).

## How the member templates are written

The templates are ordinary Spite in `library/list.spite`, over the list's own buffer on the heap. Each one is a
Symbol codegen template ([metaprogramming.md](metaprogramming.md)) whose parameter is
`member: Symbol<$element_type>`: the symbol names a member of the *element*, and `item.attributes[member]` reads
it -- the field itself, or a call to the zero-argument function. `filter_member` answers `filter_in_stock`,
`filter_is_popular` and every other `filter_<member>` call:

```gdscript
func filter_member(member: Symbol<$element_type>): List<$element_type> {
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
`say_hello(item)` on that owner (or as a call through the value, for a function held in a variable) -- so
`filter_member` also answers `filter(is_short)` without being written twice. A chain that passes functions is still
one loop.

`values` is the list's `TypedMemory<$element_type>`: `values.read_value(items, index)` reads the element in slot
`index` of the list's buffer, retained, the same way a container of your own would
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)); everything else is written in the file. A
build with `--repl` or `--repl-port` compiles every template that fits every element class of a list the loop
can reach, so `monsters.sum_health()` can be typed at the prompt; that is the one build where templates nobody
calls are in the program ([D143](decisions.md)).

## Write your own member template

A program reopens `List` by putting a `list.spite` in its own folder, and a function there with a
`Symbol<$element_type>` parameter named after a segment of its name becomes one more template, exactly like the
library's:

```gdscript title=list_average/list.spite
func average_member(member: Symbol<$element_type>): Float {
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
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Standard library metaprogramming  **[partial]**

D15 (decided by Mortaro, 2026-09-19): **every standard library template names a `<member>`, and a member is a
field or a zero-argument function without distinction.** `map_age` and `map_get_age` are the same call, because
reading an attribute already goes through `get_<attribute>()` ([Operators](functions_and_operators.md#operators--implemented)) -- the compiler may answer the field
case with a direct read, but that is an optimisation, not something the writer thinks about. What a template
requires of a member is its **arity and its return type**, never whether it is stored or computed.

The templates, what each answers and what it requires of the member are [the table above](#member-templates-loops-you-do-not-write),
which is normative; a `List<T>` of a class and a `Dictionary<T>` of a class answer all of them. These, together
with `while` and the passed functions below, cover the cases that used to reach for `for`. This is why a list of
components renders with nothing new in the language: `todos.map_render()` calls `render()` on every element and
collects the results, exactly as `todos.map_title()` collects a field.

A member that does not fit is a compile error naming the member, what it is, and what the template needs. Two
of them name a fix: `count()` is only ever a collection's own size, so `count_<member>()` on a numeric member says
`but 'count_' needs it to return Boolean (to add up a numeric member use 'sum_stars')`; and `each_<member>()` on an
attribute says `'each_size': the member 'size' of 'Thing' is an Integer, but 'each_' needs it to be a function:
reading an attribute and discarding it does nothing`. `sort_by_` is stable (equal keys keep their order) and
sorts by insertion, so it suits short lists: its time grows with the square of the length.

**How the templates are written** (D91, decided by Mortaro; the binding rule is proposed by Claude,
unconfirmed). They are Symbol codegen templates in `library/list.spite`, each a `while` over the list's
`Memory` buffer: `func filter_member(member: Symbol<$element_type>): List<$element_type>` answers every
`filter_<member>` call. The symbol names a member of the *element*, not of the list (whose own attributes are
its buffer), because it says so -- `Symbol<$element_type>` is [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s `Symbol<Label>`, one mechanism for both
-- and `item.attributes[member]` reads it: the field, or a call to the zero-argument function -- the
D11 reading, "the value held in that field", applied to D15's members. The generator binds the template to the
element's member and checks [the table](#member-templates-loops-you-do-not-write) before it compiles the body, so a member that does not fit is still
the error naming the member, its type and what the template needs; it writes none of the templates' C. Only the
names a program calls are compiled. A `--repl`/`--repl-port` build compiles every template that fits every
element class of a list the loop can reach, and lists them as that list's functions, so `monsters.sum_health()`
works at the prompt. `Dictionary<T>` answers the same names through its values (`inventory.sum_price()` is
`inventory.values().sum_price()`), and a program's own `list.spite` reopens `List` to add a template of its own.
A template there with a plain `member: Symbol` would range over `List`'s own attributes, its buffer, and answer
nothing, so it is a compile error naming `Symbol<$element_type>` (proposed by Claude, unconfirmed;
`diagnostics/plain_symbol_on_list`).

**Chains are one loop** (D105, decided by Mortaro; the form below is proposed by Claude, unconfirmed).
Each template takes what the previous one returns, and a chain means exactly its steps written out one by one.
The compiler runs a chain as one loop over the first list with no list in between: when a template is called
directly on a `map_`/`filter_` call (on a `map_`/`filter_` call, and so on) of a `List` or `Dictionary`, the
generator writes one Spite function on the first list's class -- a `while` over its buffer, an `if` per
`filter_`, a `var` per `map_`, and the last template's step -- and calls that instead. A `map_` in the middle
must reach a class; anything the rule cannot write (a nullable member, a union) is compiled step by step, which
means the same. The difference a program can see is only order: a member function in a fused chain runs element
by element. `conformance/stage6/fused_chain_allocations` pins what it saves: four chains run a thousand times
allocate nothing, so the whole program makes 15 allocations (its objects, the list and printing the total)
against more than 16 000 step by step.

**Passing a function for each element** (D148, decided by Mortaro, superseding D113's caller-function templates;
the forms' details below are proposed by Claude, unconfirmed).  **[implemented]** An iterator sees only the
element and the list, never the class the call is written in: `people.each_say_hello()` names a member
`say_hello` of each `Person`, and a function of the caller never answers
it. When the calling class has a function of that name, the error says to pass it
(`'String' has no attribute or zero argument function 'say_hello' for 'each_say_hello': a template reads a member
of each element, never a function of this class, so pass this class's 'say_hello' instead: 'each(say_hello)'`).
The caller's function is passed as a bound function value (D17/D39), owned by whoever it is bound to:

- **The forms.** `each(f)`, `map(f)`, `filter(f)`, `any(f)`, `all(f)`, `count(f)`, `find(f)`, `sort_by(f)` and
  `sum(f)` on a `List` or a `Dictionary` (through its values), for an element of any type, and all but `find` and
  `sort_by` on a `Vector` or an `Items` of plain values (D221, below). `f` takes the element
  as its only argument, with exactly the element's type; D15's table applies to what it returns (`filter`, `any`,
  `all`, `count` and `find` want `Boolean`, `sum` a number, `sort_by` a number or a `String`, `map` a value, `each`
  anything). `find(f)` answers the first element `f` is true for, or `null` -- the `find_by_` template with
  `true` as its value. `count` with no argument stays the collection's size.
- **The owner.** `say_hello` alone is bound to this instance; `greeter.greet` to `greeter`; a variable holding a
  `Spite.Function<T, R>` is called through the value. `people.map(greeter.label).filter(is_short)` mixes owners.
  A list's own function is bound to the list the same way (`numbers.each(found.append)`, proposed by Claude,
  unconfirmed); a chain that passes one is not fused, and runs step by step.
- **How it is written.** No template changes: the same `library/list.spite` template (`each_member(member:
  Symbol<$element_type>)`) is instantiated once per function and owner class, with a last hidden parameter holding
  the owner (the function's instance, passed at the call site), and `item.attributes[member]` reads as
  `owner.say_hello(item)` -- or `owner(item)` when the owner is a held function value. A function written by name
  therefore costs no allocation and no indirect call; only a held value is called through `Spite.Function`. Only
  the forms a program calls are instantiated, and these instances are not listed among the list's `functions`
  nor offered at a `--repl` prompt.
- **Chains.** A passed function chains and fuses with the member templates (D105): `map(f)` and `filter(f)` may
  sit in the middle of a chain and any form may end one, so `people.filter_active().map(greeter.label)` and
  `names.filter(is_short).map(measure).sum(double_of)` are each one loop; the fused function takes each owner as
  a parameter. The element's own members still come only from the element (`people.map(say_hello).filter_active()`
  reads `active` of whatever `say_hello` returns).
- **Mistakes** name the form: `'map(say_hello)': 'say_hello' returns nothing, but 'map' needs it to return a value
  (to only call it for each element, write 'each(say_hello)')`, `'each' calls 'greet_twice' with each 'String' as
  its only argument, but 'greet_twice' takes 2`, `'each' calls 'count_to' with each 'String', but 'count_to' takes
  'Integer'`, and an argument that is no function at all.
- **The loop rule.** D113's error for a `while` that only passes each element to a caller function went with
  it; D171 ([control_flow.md](control_flow.md#a-while-that-a-member-template-already-says)) puts a `while` that only does what `each(f)` does under the same rule as the member
  templates. A function that needs more than the element (`print_statement(statement, depth)`) keeps its `while`.

`conformance/stage6/passed_functions`, `diagnostics/passed_functions`.

### List<T> additions  **[implemented]**

A list's members, their results and their edge cases are [the table under `List<T>`](#listt), which is normative:
out of range, `get_at` answers the element type's default; `first`, `last`, `remove_first` and `remove_last` answer
`null` on an empty list (D211, like `[]`),
`insert` clamps to the nearest end, and `set_at` and `remove_at` do nothing; `list[index]` answers `T?`. There is
no `for`: a list is walked with a template, a passed function ([above](#standard-library-metaprogramming--partial)),
or a `while` that does more than they do.

`contains(value)` compares with `==`, so it is there only for elements that are numbers, `Boolean`, `String` or an
enum; on a list of a class the call is `List has no method 'contains'` -- ask with `any(f)` or `find_by_<member>`.

Names say where: `add` does not, so it is `append` (and `prepend`); `pop()` is `remove_last()`, beside
`remove_first()`. Writing an old name is a compile error naming the replacement: `List has no method 'add', which
does not say where: write 'append' to add at the end, or 'prepend' at the start`, and `List has no method 'pop':
write 'remove_last()', or 'remove_first()' to take from the start` (`diagnostics/old_list_names`).

### Dictionary\<T\>  **[implemented]**

String-keyed and insertion-ordered; its members are [the table under `Dictionary<T>`](#dictionaryt), which is
normative. `dictionary[key]` is `get(key)`, a `T?` that is `null` for an absent key, and `dictionary[key] = value`
is `set(key, value)`; there is no `get_at`/`set_at` on a dictionary. `keys()` and `values()` answer fresh copies,
in insertion order.

It is a hash table over two ordered lists (the decision log's hash-table row, proposed by Claude, unconfirmed):
`get`, `has`, `set` and `[]` take the same time however many keys there are, and `remove` takes time in
proportion to the dictionary's size. An empty dictionary allocates no table.

The whole member template family, and every passed-function form, works on a `Dictionary<T>` through its values,
as it does on a `List<T>`; to walk keys and values together, use `while` over `dictionary.keys()` and read
`dictionary[key]`.

`deep_copy()` is implemented for classes, lists and dictionaries. A `String` is shared rather than duplicated
because it is immutable; a union or a `type` shape is shared for now; a self-referring structure is still
unsupported ([Memory](memory.md#memory--implemented)).

### Vector\<T\>  **[implemented]**

D154 (decided by Mortaro): a `Vector<T>` holds its items inline, where a `List<T>` holds references; D204
(decided by Mortaro): reading an item gives a borrowed reference into the vector. Its members are [the table
under `Vector<T>`](#vectort-items-inline), which is normative. The readings below are proposed by Claude,
unconfirmed:

- **The name** is `Vector<T>` (`mortaros_missing_decisions.md` item 175); the classes that were called `Vector` in
  `examples/vectors`, `conformance/stage6/operators` and `benchmarks/small_allocations` are now `Displacement`.
- **What an item may be.** A number, a `Boolean`, an enum, a `String`, or a class whose attributes are only those
  (a `T?` of one of them, or a singleton such as `Console`, included). Anything else is an error when the vector
  type is written: `'Holder' cannot be an item of a Vector: its attribute 'scores' is a List<Integer>, and a
  Vector holds each item inline, so every attribute is a number, a Boolean, an enum or a String (keep 'Holder' in
  a 'List<Holder>', or keep 'scores' somewhere else)`; `Vector<List<Integer>>` is `a Vector holds each item
  inline, so an item is a number, a Boolean, an enum, a String or a class made only of those, and a List<Integer>
  is not: keep it in a 'List<List<Integer>>'`. A class with a `drop()` is `'Handle' cannot be an item of a Vector:
  it has a 'drop()', and an item inline in a Vector is never an object of its own to drop (keep 'Handle' in a
  'List<Handle>')`, and a class whose functions use `this` as a value (return it, pass it, keep it, or name one of
  its own functions as a value) is `'Selfish.itself' uses 'this' as a value, and an item of a Vector is borrowed
  from the Vector, never an object to keep or to pass on: read and write its attributes instead`.
- **Reading.** `vector[index]` and `get_at(index)` answer the item itself, a `T` and never a `T?`: an index out of
  range halts with the crash report of `library/vector.spite`'s `crash index >= 0 and index < item_count`. A vector
  of numbers, `Boolean`s, enums or `String`s answers the value, as a list does: a plain value -- a number, a
  `Boolean`, an enum, `Memory.Address` included -- is copied as it is read and never borrowed (D221, decided by
  Claude under D205), so none of the borrow rules applies to it; a `String` is answered as a counted reference,
  as a list's is. What a borrowed item may do, and
  what a row of them passed to a system may do (D206), is
  [memory.md's rule](memory.md#borrowed-items-of-a-vectort--implemented).
- **Writing.** `append(value)` and `set_at(index, value)` (`vector[index] = value`) copy the value's attributes
  into the block, counting each `String` attribute once more; the value itself stays an ordinary object. D202
  keeps a constructor out of the argument, so an appended item is made on its own line first: `var slow =
  Velocity(1.0, 0.5)`, then `velocities.append(slow)`.
- **Removing.** `remove_at(index)` releases the item's `String` attributes and moves every later item down;
  `clear()` releases every item's and keeps the capacity; dropping the vector releases them and frees the block.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_` and `sum_<member>()` are in
  `library/vector.spite`, written over the borrowed items; `filter_` answers a `Vector<T>` of copies and `map_` a
  `List` of the members' values. A chain of them is one loop over the block (D105), with `filter_` steps in the
  middle, and `parallel_each_<member>()` splits the walk across the thread pool as it does a list's.
  `sort_by_`, `find_by_` and a program's own templates are not built for a vector. The passed-function forms
  `each`, `map`, `filter`, `count`, `any`, `all` and `sum` are, for a vector of plain values only (D221): the
  same templates, instantiated per function as for a list, reading each item as a copy, and fusing in a chain;
  `filter(f)` answers a `Vector<T>`. `find(f)` and `sort_by(f)` are `'find' takes a function on a List, and a
  Vector of plain values takes one only in 'each', 'map', 'filter', 'any', 'all', 'count' and 'sum': keep the
  values in a 'List<Integer>' to use 'find'`. A passed function would take a class item as an argument, which a
  borrowed item never is, so `velocities.each(f)` is `'each' passes each item to a function, and an item of a
  Vector is borrowed from it, never passed on: give the item's class a function and call it with a member
  template, such as 'each_<function>()'`; a vector of `String`s is not counted as plain (it holds references to
  text, and D221 decided the rule conservatively), so `names.each(f)` is `'each' passes each item to a function,
  which a Vector does only for plain values -- numbers, Boolean and enums, copied as they are read -- and a String
  is not one: keep the values in a 'List<String>' to pass them on` (`diagnostics/plain_items`). The D171 rule for
  a `while` a template already says does not look at a vector's loops yet.
- **Its allocator.** `velocities.memory.allocator = arena` on the next line places the `Vector` object in the
  arena, as for any object (D152). **Not built:** its block of items following it there; the block is on the heap,
  because a class cannot read its own `.memory` yet (item 175's proposal, `memory.allocator` read inside a class,
  is still open).

`diagnostics/vector_borrows`, `diagnostics/vector_items`, `diagnostics/plain_items`,
`conformance/stage6/vector_items`, `conformance/stage6/plain_items`.

### Items\<T\>  **[implemented; the name provisional]**

D218 (decided by Claude under D205 and D214): `Items<T>` chooses its storage while compiling, inline like a
`Vector<T>` when `T.fits_vector()` and references like a `List<T>` otherwise, behind one set of members. Its
members are [the table under `Items<T>`](#itemst-the-storage-chosen-for-you), which is normative. The readings
below, and the names `Items` and `remove_swapping`, are proposed by Claude, unconfirmed:

- **One class, folded.** `library/items.spite` is one generic class. Its storage is the same for both kinds -- a
  heap block, a count and a capacity -- and each body that touches an item folds on `$element_type.fits_vector()`
  ([metaprogramming.md](metaprogramming.md#asking-whether-a-class-fits-a-vector)): the inline branch goes through
  `InlineMemory<T>`, the reference branch through `TypedMemory<T>`, and the branch not taken is not compiled. So a
  `T` that does not fit never makes a `Vector<T>`, never meets the item errors of
  [Vector](#vectort--implemented), and never has an item function written for it. Both helpers are singletons
  bound as attributes (D144), so an `Items` object holds one pointer more than a `Vector` or a `List`, one per
  collection and never per item. No syntax was added: the language already folds a body on `fits_vector()`, and
  the storage did not need an attribute of its own per kind.
- **Which kind.** `T` fits exactly when a `Vector<T>` could be made (D204's rule: a number, a `Boolean`, an enum, a
  `String`, or a class made only of those, with no `drop()` and no function that uses `this` as a value). A
  union, a `type`, a `List`, a `Dictionary` or a class holding one is kept by reference. `Items<Integer>` and
  `Items<String>` are plain arrays of the values, as a vector's are.
- **Reading.** `items[index]` and `get_at(index)` answer a `T`, never a `T?`, for both kinds, and an index out of
  range halts with the crash report of `library/items.spite`'s `_out_of_range`. A plain value (a number, a
  `Boolean`, an enum) is copied, never borrowed (D221). Otherwise, inline, the item is borrowed, and
  every rule of [memory.md's borrowed items](memory.md#borrowed-items-of-a-vectort--implemented) applies to it,
  rows included. By reference, it is a counted reference like a list element read with `get_at`: it may be kept,
  passed, returned and read after the collection changes size. `List`'s `[]` answers `T?` instead; `Items` does
  not, so that code reading an item compiles the same whichever kind a class falls into, and so D217's walked row
  can take the item as it is.
- **Items of a `T?`** (proposed by Claude, unconfirmed). `Items<String?>`'s `[]` answers the `String?` that was
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
  `remove_swapping` beside `remove_at`: called on the collection, or through D169's call effects, where a call
  that swaps an item out counts as a removal.
- **Writing and removing.** `append` and `set_at` copy the value's attributes in when inline (counting each
  `String` attribute once more) and keep the value when by reference; `remove_at`, `remove_swapping`, `clear` and
  dropping the collection release what they remove. `remove_swapping(index)` moves the last item into `index`
  and does nothing out of range; it keeps no order, and costs the same however many items there are.
- **Templates.** `each_`, `map_`, `filter_`, `count_`, `any_`, `all_` and `sum_<member>()` are in
  `library/items.spite`, each folded the same way; `filter_` answers an `Items<T>` (copies when inline, the same
  references otherwise) and `map_` a `List` of the members' values. A chain is one loop over the block (D105),
  and `parallel_each_<member>()` splits it across the thread pool, as for a vector. The passed-function forms
  (`each(f)`, `map(f)`, ...) are offered, as on a vector, only for plain values (D221), whose items are copies;
  `filter(f)` answers an `Items<T>`. For any other item they are not offered for either kind, since an inline
  item is never passed on: `'each'
  passes each item to a function, and 'Velocity' fits a Vector, so an item of these Items is borrowed from them,
  never passed on: give the item's class a function and call it with a member template, such as
  'each_<function>()'`; by reference the error says that `Items` does not do it whichever storage it chose.
- **Cost.** Nothing runs to choose: the folds are decided while compiling and the untaken branch is absent from
  the C, so an `Items<Velocity>` compiles to a `Vector<Velocity>`'s code and an `Items<Trail>` to a
  `List<Trail>`'s, reading its elements uncounted in the templates as a list does
  ([optimizations.md](optimizations.md#a-lists-templates-read-its-elements-without-counting-them)). The range
  check of `[]` is one comparison, and its crash report sits in a function of its own so that the read is small
  enough for the C compiler to inline. A program that makes no `Items` carries none of it. Measured in
  `benchmarks/items_storage`.

`conformance/stage6/items_columns`, `conformance/stage6/plain_items`, `diagnostics/items_borrows`,
`diagnostics/plain_items`.

### Heap\<T\> -- removed (D1, decided by Mortaro, 2026-09-19)

`Heap<T>`, a box for a value, is gone (it is not `Memory.Heap`, the allocator). References are the default
([Memory](memory.md#memory--implemented)), so a class or union containing itself recursively just declares an
ordinary attribute of type `T` -- no indirection needed, and no struct-layout cycle to guard against, since a
class is always a heap object referred to by pointer. A `union Expression` of `NumberExpression` and
`BinaryExpression`, with `var left: Expression? = null` in `binary_expression.spite`, is the whole of it;
[memory.md](memory.md#self-referential-classes-and-unions-just-work) has a tree that runs.

Writing `Heap<T>`/`Heap<T>(value)` is a compile error naming this fix: `there is no 'Heap<T>': a class, a list and
a text are references already, so a class that holds one of its own kind declares an ordinary attribute, like
'var left: Expression? = null' ('Memory.Heap' is the allocator, for containers of your own)`
(`diagnostics/old_list_names`).
