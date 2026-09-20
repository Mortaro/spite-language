# Standard library, task by task

Nothing in the standard library uses true reference counting; ownership follows the same single-owner rule as
everywhere else (see [memory.md](memory.md)). Every method below returns a default instead of crashing when
the operation cannot succeed (an out-of-range index, a key that is not present, a `String` that does not
parse).

## Read a file

```spite title=file_tasks/file_tasks.spite entry
var console = Console()

func FileTasks() {
    var log_file = File(".spite-cache/documentation_demo_log.txt")
    log_file.write("first line")
    log_file.append(", second line")
    console.print("exists", log_file.exists())
    var content = log_file.read()
    crash content
    console.print("content", content)
    console.print("removed", log_file.remove())
    console.print("exists after remove", log_file.exists())
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
    console.print("exists", target.exists())
    var examples = Directory("examples")
    console.print("has hello", examples.folders().contains("hello"))
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
    console.print("output", listing.output().trim())
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
    console.print("count", inventory.count())
    console.print("has shield", inventory.has("shield"))
    console.print("potions", inventory["potion"])
    console.print("shields", inventory["shield"])
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
shields 0
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
    console.print("in stock count", items.filter_in_stock().count())
    console.print("in stock price total", items.filter_in_stock().sum_price())
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

`filter_<attribute>()`, `find_by_<attribute>(value)`, and `sort_by_<attribute>()` all return **views**:
shallow-copied elements the source `List<T>` still owns. Only the view's own wrapper is ever dropped, never the
elements inside it -- you can chain freely (`items.filter_in_stock().sum_price()`) without worrying about a
double free.

## `Console`

`print(...)` (space-separated, trailing newline), `error(...)` (stderr, trailing newline), `write(...)`
(stdout, no trailing newline), `read_line(): String?` (one line from stdin; `null` only at end of
file with nothing read).

## `Program`

`Program().exit(code)` exits the process immediately with `code`; the entry constructor returning normally is
exit code `0`. See [getting_started.md](getting_started.md) for the full method tables (`String`, `List<T>`,
`Dictionary<T>`) if the task you need isn't above.
