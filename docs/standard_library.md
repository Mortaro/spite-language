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

## Walk a directory tree

A `Directory` has a `path`, the way a `File` does, and `entries()` lists what is inside it as a
`List<Directory.Entry>`: each entry is a `Directory` or a `File` whose `path` is already joined to its parent's,
so a `switch` tells them apart and a folder is walked by calling the same function again.

```spite title=directory_walk/directory_walk.spite entry
var console = Console()

func DirectoryWalk() {
    var root = Directory(".spite-cache/documentation_walk")
    var inner = Directory("{root.path}/inner")
    root.create()
    inner.create()
    File("{root.path}/top.txt").write("top")
    File("{inner.path}/deep.txt").write("deep")
    walk(root)
}

func walk(folder: Directory) {
    var entries: List<Directory.Entry> = folder.entries()
    var index = 0
    while index < entries.count() {
        var entry = entries.get_at(index)
        switch entry {
            Directory: {
                console.print("folder", entry.path)
                walk(entry)
            }
            File: console.print("file", entry.path)
        }
        index = index + 1
    }
}
```
```output
folder .spite-cache/documentation_walk/inner
file .spite-cache/documentation_walk/inner/deep.txt
file .spite-cache/documentation_walk/top.txt
```

Folders come first, then files, each sorted by name, and `.` and `..` are never listed.

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
