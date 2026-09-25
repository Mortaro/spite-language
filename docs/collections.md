# Lists, dictionaries and member templates

`List<T>` and `Dictionary<T>` are not built into the compiler. They are ordinary generic classes written in Spite
-- `library/list.spite` and `library/dictionary.spite` -- over `Memory.Heap` and `Memory.Address`, the floor
everything else is built on. The compiler keeps only the syntax: `[1, 2, 3]`, `list[index]`, and `List<T>()`. Read those two files to see
exactly what a list does; write your own container the same way ([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).

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
| `append(value)` / `prepend(value)` | | adds to the end / to the front |
| `insert(index, value)` | | an index out of range is clamped to the nearest end |
| `list[index]` | `T?` | may not be there, so it is narrowed ([failure.md](failure.md#reading-with--answers-t)) |
| `list[index] = value` | | the explicit form is `set_at(index, value)` |
| `get_at(index)` | `T` | the default when out of range |
| `remove_at(index)` | | nothing happens out of range |
| `remove_first()` / `remove_last()` | `T` | removes and returns it; the default when empty |
| `first()` / `last()` | `T` | the default when empty |
| `count()` / `is_empty()` | `Int` / `Bool` | `count()` is only ever the list's size |
| `contains(value)` | `Bool` | |
| `clear()` / `reverse()` | | in place |
| `join(separator)` | `String` | every element becomes text: a `String`, a number, a `Bool`, an enum value |
| `copy()` / `deep_copy()` | `List<T>` | one level, or all the way down ([memory.md](memory.md)) |

Names say where: `append` and `prepend`, never `add`; `remove_last`, never `pop`. The old names are errors that
name the new ones.

## `Dictionary<T>`

Keys are `String`, entries keep the order they were inserted in, and `get`, `has`, `set` and `[]` take the same
time however many keys there are -- it is a hash table over two lists.

| Member | Result | Notes |
|---|---|---|
| `set(key, value)` / `dictionary[key] = value` | | replaces the value of a key already there |
| `get(key)` / `dictionary[key]` | `T?` | `null` when the key is absent |
| `has(key)` | `Bool` | |
| `remove(key)` | | |
| `count()` | `Int` | |
| `keys()` / `values()` | `List<String>` / `List<T>` | a fresh list, in insertion order |
| `copy()` / `deep_copy()` | `Dictionary<T>` | |

```gdscript title=dictionary_tasks/dictionary_tasks.spite entry
var console = Console()

func DictionaryTasks() {
    var inventory = Dictionary<Int>()
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

## Member templates: loops you do not write

A list of a class answers a family of functions named after the element's members (or after a function of your
own, [below](#a-function-of-yours-for-each-element)). `chores.count_done()` counts
the chores whose `done` is true, `items.sum_price()` adds up their prices, and `repositories.map_name()` collects
their names. A **member** is an attribute or a function that takes no arguments -- the two are the same to a
template, since reading an attribute already goes through its getter -- and a template is compiled only for the
names a program calls.

| Template | Result | The member must |
|---|---|---|
| `filter_<member>()` | `List<T>`, the elements where it is true | take nothing, return `Bool` |
| `count_<member>()` | `Int`, how many are true | take nothing, return `Bool` |
| `any_<member>()` / `all_<member>()` | `Bool` | take nothing, return `Bool` |
| `sum_<member>()` | the member's number type | take nothing, return a number |
| `find_by_<member>(value)` | `T?`, the first element whose member equals `value` | return something comparable to `value` |
| `sort_by_<member>()` | `List<T>`, sorted ascending | return a number or a `String` |
| `map_<member>()` | `List<U>`, one value per element | return a value |
| `each_<member>()` | nothing: calls it on every element | be a function |

A member that does not fit is a compile error naming the member, what it is, and what the template needs.
`count_` on a number is one of them, and names `sum_` instead: `count()` is only ever a collection's size.

```gdscript title=list_helpers/chore.spite
var title = ""
var done = false

func Chore(new_title: String, new_done: Bool) {
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
    chores.append(Chore("write docs", false))
    chores.append(Chore("ship release", false))
    chores.append(Chore("rest", true))
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

func Item(new_name: String, new_price: Int, new_in_stock: Bool) {
    name = new_name
    price = new_price
    in_stock = new_in_stock
}
```
```gdscript title=list_query/list_query.spite entry
var console = Console()

func ListQuery() {
    var items = List<Item>()
    items.append(Item("sword", 50, true))
    items.append(Item("shield", 30, false))
    items.append(Item("potion", 10, true))
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
but 'count_' needs it to return Bool (to add up a numeric member use 'sum_stars')
```

## A function of yours for each element

The member a template names can also be a function of the class the call is written in, when it takes the
element and nothing else. With `func say_hello(name: String)` in the class, `names.each_say_hello()` calls
`say_hello` once for every name, in order; there is no `say_hello_to_everyone` to write. This works on a list of
anything -- text and numbers included, which have no members of their own for a template to name -- and every
template takes such a function the way it takes a member: `filter_`, `count_`, `any_` and `all_` a function
returning `Bool`, `sum_` one returning a number, `map_` one returning a value, `find_by_(value)` and `sort_by_`
one returning something comparable.

```gdscript title=caller_function/caller_function.spite entry
var console = Console()

func CallerFunction() {
    var names = ["Ada", "Grace", "Barbara"]
    names.each_say_hello()
    var short_names = names.filter_is_short()
    var joined = short_names.join(", ")
    console.print("short:", joined)
    var letters = names.filter_is_short().map_measure().sum_double_of()
    console.print("letters, doubled:", letters)
}

func say_hello(name: String) {
    console.print("hello, {name}")
}

func is_short(name: String): Bool {
    return name.length() < 6
}

func measure(name: String): Int {
    return name.length()
}

func double_of(value: Int): Int {
    return value * 2
}
```
```output
hello, Ada
hello, Grace
hello, Barbara
short: Ada, Grace
letters, doubled: 16
```

Which function a name means is never a guess. The element's own member is looked for, then the calling class's
function; when both exist, the call is an error asking to rename one, so adding a member to a class can never
quietly change what a template somewhere else calls. The template sees only the element: a function that needs
more, such as `print_statement(statement, depth)`, is still called from a `while`.

A loop written only to do this is an error that names the template: a counter from `0` to `list.count()`, and a
body that passes `list[counter]` -- directly, or through one `var` -- to one function of the class and adds one to
the counter.

```gdscript title=caller_function_loop/caller_function_loop.spite entry error
var console = Console()

func CallerFunctionLoop() {
    var names = ["Ada", "Grace"]
    var index = 0
    while index < names.count() {
        say_hello(names[index])
        index = index + 1
    }
}

func say_hello(name: String) {
    console.print("hello, {name}")
}
```
```diagnostic
this 'while' only calls 'say_hello' with each element of 'names': write 'names.each_say_hello()'
```

## Chains run as one loop

Every template takes what the one before it gives, so they chain: `map_<member>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as -- that is what it means -- but the compiler runs it as **one loop over
the first list**, with no list in between: `teams.filter_active().map_lead().sum_age()` visits each team once,
reads its lead, and adds the age, allocating nothing.

```gdscript title=fused_chain/person.spite
var age = 0

func Person(new_age: Int) {
    age = new_age
}
```
```gdscript title=fused_chain/team.spite
var active = false
var lead = Person(0)

func Team(new_active: Bool, new_lead: Person) {
    active = new_active
    lead = new_lead
}
```
```gdscript title=fused_chain/fused_chain.spite entry
var console = Console()

func FusedChain() {
    var teams = List<Team>()
    var ann = Person(34)
    teams.append(Team(true, ann))
    var bob = Person(67)
    teams.append(Team(false, bob))
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
starts. `conformance/stage6/fused_chain_allocations` runs four chains a thousand times each and pins its
allocation count at 11; written step by step, the same program allocates 16 010 times.

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

The same template answers a function of the calling class. For `names.each_say_hello()` the compiler compiles
`each_member` once more for that class, with the caller passed in beside the list, and
`item.attributes[member]` reads as `say_hello(item)` on it -- so a template a program adds answers both kinds of
member without being written twice. A chain that uses one is still one loop.

`values` is the list's `TypedMemory<$element_type>`: `values.read_value(items, index)` reads the element in slot
`index` of the list's buffer, retained, the same way a container of your own would
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)); everything else is written in the file. A
build with `--repl` or `--repl_port` compiles every template that fits every element class of a list the loop
can reach, so `monsters.sum_health()` can be typed at the prompt.

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

func Score(new_points: Int) {
    points = new_points
}
```
```gdscript title=list_average/list_average.spite entry
var console = Console()

func ListAverage() {
    var scores = List<Score>()
    scores.append(Score(3))
    scores.append(Score(4))
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
func total_member(member: Symbol): Int {
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
    var scores = List<Int>()
    scores.append(3)
    var count = scores.count()
    console.print(count)
}
```
```diagnostic
write 'member: Symbol<$element_type>' to name a member of the element
```
