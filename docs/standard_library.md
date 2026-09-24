# Standard library, task by task

The standard library is ordinary Spite in `library/`, written over `Memory`, and its values are reference
counted like every other object (see [memory.md](memory.md)). When an operation cannot succeed it says so in
its type rather than crashing: an index or a key that is not there reads as `T?`, a file that cannot be read
answers `null`, and text that does not parse as a number reads as `0`.

## Read a file

```spite title=file_tasks/file_tasks.spite entry
var console = Console()

func FileTasks() {
    var log_file = File(".spite-cache/documentation_demo_log.txt")
    log_file.write("first line")
    log_file.append(", second line")
    var log_exists = log_file.exists()
    console.print("exists", log_exists)
    var content = log_file.read()
    crash content
    console.print("content", content)
    var removed = log_file.remove()
    console.print("removed", removed)
    log_exists = log_file.exists()
    console.print("exists after remove", log_exists)
}
```
```output
exists true
content first line, second line
removed true
exists after remove false
```

`read()` is a `String?` (`null` when the file does not exist) -- narrow it with `if ... { }`, with `assert`
(which returns quietly from a function that returns nothing), or with `crash` (which halts), exactly like any
other `T?`. This entry constructor uses `crash`: `assert` is not allowed in a constructor, because a
constructor is setup rather than logic.

## List a directory

```spite title=directory_tasks/directory_tasks.spite entry
var console = Console()

func DirectoryTasks() {
    var target = Directory(".spite-cache/documentation_demo_dir")
    target.create()
    var target_exists = target.exists()
    console.print("exists", target_exists)
    var examples = Directory("examples")
    var has_hello = examples.folders().contains("hello")
    console.print("has hello", has_hello)
}
```
```output
exists true
has hello true
```

`files()`/`folders()` return sorted `List<String>` of names (not full paths).

## Run a process

```spite title=process_tasks/process_tasks.spite entry
var console = Console()

func ProcessTasks() {
    var listing = Process("echo", ["build finished"])
    var code = listing.run()
    console.print("exit code", code)
    var trimmed_output = listing.output().trim()
    console.print("output", trimmed_output)
}
```
```output
exit code 0
output "build finished"
```

`arguments` is a `List<String>`, each shell-quoted for you. `output()` is stdout+stderr merged, valid after
`run()` -- and it includes the child process's own trailing newline, which is why the example above calls
`.trim()`.

## Group things in a `Dictionary`

```spite title=dictionary_tasks/dictionary_tasks.spite entry
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

`set(key, value)` replaces an existing key's value rather than adding a duplicate -- there is exactly one entry
per key, insertion-ordered. `dictionary["missing_key"]` reads as the value type's default (`0` for
`Dictionary<Int>`), never a crash; `get(key)` is the `T?` form when you need to tell "absent" apart
from "present but zero."

## Query a list of classes

```spite title=list_query/item.spite
var name = ""
var price = 0
var in_stock = false

func Item(new_name: String, new_price: Int, new_in_stock: Bool) {
    name = new_name
    price = new_price
    in_stock = new_in_stock
}
```
```spite title=list_query/list_query.spite entry
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

`filter_<member>()` and `sort_by_<member>()` return a new list holding the same elements (each one counted
once more, not copied), so you can chain freely (`items.filter_in_stock().sum_price()`); `find_by_<member>(value)`
returns the element itself or `null`. A member is a field or a function that takes no arguments
(`map_label()` calls `label()` on every element); what each template needs of it is in the manual's section 8.

## How the member templates are written

The templates are ordinary Spite in `library/list.spite`, over the list's own `Memory` buffer. Each one is a
Symbol codegen template (see [metaprogramming.md](metaprogramming.md)) whose `Symbol` parameter is named
`member`; in a list, that symbol names a member of the *element*, and `item.attributes[member]` reads it -- the
field itself, or a call to the zero-argument function. `filter_member` answers `filter_in_stock`,
`filter_is_popular` and every other `filter_<member>` call:

```
func filter_member(member: Symbol): List<$element_type> {
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

`values` is the list's `TypedMemory<$element_type>`: `values.read_value(items, index)` reads the element in slot
`index` of the list's buffer, retained, the same way a container of your own would (see
[memory.md](memory.md)); everything else is written in the file. A template is compiled only for the
names a program calls, so a program that never calls `sum_price()` carries no `sum_price` at all. A build with
`--repl` or `--repl-port` compiles every template that fits every element class of a list the loop can reach,
so `monsters.sum_health()` can be typed at the prompt. `Dictionary<T>` answers the same names through its
values: `inventory.sum_price()` is `inventory.values().sum_price()`.

## Chains run as one loop

Every template takes what the one before it gives, so they chain: `map_<member>()` turns a list of teams into a
list of their leads, `filter_<member>()` keeps some of them, and anything else finishes the chain. Read a chain
as the separate steps it is written as -- that is what it means -- but the compiler runs it as **one loop over
the first list**, with no list in between: `teams.filter_active().map_lead().sum_age()` visits each team once,
reads its lead, and adds the age, allocating nothing.

```spite title=fused_chain/person.spite
var name = ""
var age = 0

func Person(new_name: String, new_age: Int) {
    name = new_name
    age = new_age
}
```
```spite title=fused_chain/team.spite
var active = false
var lead = Person("", 0)

func Team(new_active: Bool, new_lead: Person) {
    active = new_active
    lead = new_lead
}
```
```spite title=fused_chain/fused_chain.spite entry
var console = Console()

func FusedChain() {
    var teams = List<Team>()
    var ann = Person("ann", 34)
    teams.append(Team(true, ann))
    var bob = Person("bob", 67)
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

Only a call made directly on another template call is part of the chain: `active.map_lead()` above starts a
new one, because `active` is a list the program named and kept. The steps in the middle are `map_` (to a member
that is a class) and `filter_`; the last call can be any template. The one visible difference is order: a
member function in a fused chain runs element by element, where the steps written out would run it on every
element before the next step starts. `conformance/stage6/fused_chain_allocations` runs four chains a thousand
times each and pins its allocation count at 11, the lists and objects it builds before the loop and the one
`TypedMemory` its lists share; written step by step, the same program allocates 16 010 times.

## Write your own member template

A program reopens `List` by putting a `list.spite` in its own folder, and a function there with a `Symbol`
parameter named after a segment of its name becomes one more template, exactly like the library's:

```spite title=list_average/list.spite
func average_member(member: Symbol): Float {
    if item_count == 0 {
        return 0.0
    }
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
```spite title=list_average/score.spite
var points = 0

func Score(new_points: Int) {
    points = new_points
}
```
```spite title=list_average/list_average.spite entry
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

## `Console`

`print(...)` (space-separated, trailing newline), `error(...)` (stderr, trailing newline), `write(...)`
(stdout, no trailing newline), `read_line(): String?` (one line from stdin; `null` only at end of
file with nothing read).

## `Program`

`Program().exit(code)` exits the process immediately with `code`; the entry constructor returning normally is
exit code `0`. See [getting_started.md](getting_started.md) for the full method tables (`String`, `List<T>`,
`Dictionary<T>`) if the task you need isn't above.
