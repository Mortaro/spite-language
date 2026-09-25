# Standard library

The standard library is ordinary Spite in `library/`, and a program reads it the way it reads its own code:
every class a program can name -- `String`, `List`, `Int`, `File`, `Memory.Heap` -- is a file there, and
`--final_classes` prints each one as the program uses it. Its values are reference counted like every other
object ([memory.md](memory.md)). When an operation cannot succeed it says so in its type rather than crashing:
an index or a key that is not there reads as `T?`, a file that cannot be read answers `null`, and text that does
not parse as a number reads as `0`.

`library/` holds what every operating system shares; `library/windows/`, `library/linux/` and `library/mac/`
reopen the classes each system does differently, and the launcher loads the one the program is compiled for
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

## What is in it

| Class | What it is | Page |
|---|---|---|
| `String` | immutable text | [below](#string) |
| `Int`, `Long`, `Float`, `Double`, `Bool`, ... | numbers, as classes | [values_and_types.md](values_and_types.md#numbers-are-classes) |
| `List<T>`, `Dictionary<T>` | containers, and the member templates | [collections.md](collections.md) |
| `Console` | the terminal: print, read a line | [below](#console) |
| `File`, `Directory` | files and folders | [below](#read-and-write-a-file) |
| `Process` | run another program | [below](#run-a-process) |
| `Program` | this program: exit, sleep, environment variables | [below](#program) |
| `Clock` | elapsed time for measuring, and the wall clock | [below](#clock) |
| `Instant`, `Duration`, `LocalDate`, `LocalTime`, `LocalDateTime`, `Period`, `TimeZone`, `TimeZones`, `TimeText` | exact time, the calendar, time zones as presentation, ISO 8601 text | [time.md](time.md) |
| `Environment`, `Build`, `Arguments` | settings and the command line | [programs.md](programs.md) |
| `Json<T>` | any value to JSON text and back | [json.md](json.md) |
| `Concurrent`, `Parallel` | run a function while waiting, or on a thread | [concurrency.md](concurrency.md) |
| `Socket` | TCP on `127.0.0.1`, which the remote REPL uses | [below](#socket) |
| `Memory.Address`, `Memory.Heap`, `TypedMemory<T>` | a place in memory, the allocator that owns it, and values of any type there: the floor every other type is built on | [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it) |
| `DynamicLibrary` | call a native library | [foreign_libraries.md](foreign_libraries.md) |
| `Spite.Class`, `Spite.Function`, ... | reflection | [reflection.md](reflection.md) |

## `String`

Immutable and reference counted. A value is placed inside written text, `"hello {name}"`, and two values join
with `+` ([values_and_types.md](values_and_types.md#string)). `==`, `!=`, `<` and `>` compare by content.

| Member | Result | Notes |
|---|---|---|
| `length()` / `is_empty()` | `Int` / `Bool` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty or invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Int` | the byte's value; `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Bool` | |
| `index_of(text)` | `Int` | `-1` when absent |
| `replace(from, to)` | `String` | every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | split on `
` |
| `to_int()`, `to_long()`, `to_double()`, ... | a number | one per number type; `0` when it does not parse |

Assigning text to a number calls the matching `to_<type>()`: `var age: Int = "42"` is `42`.

## Read and write a file

`File(path)` is a value -- a path -- and several may exist at once.

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `read()` | `String?` | `null` when the file cannot be read |
| `write(text)` / `append(text)` | `Bool` | replaces the content / adds to its end |
| `exists()` / `remove()` | `Bool` | |

```gdscript title=file_tasks/file_tasks.spite entry
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

`read()` is a `String?` -- narrow it with `if content { }`, `assert content` (which returns the function's
default) or `crash content` (which halts), exactly like any other `T?` ([failure.md](failure.md)). This entry
constructor uses `crash`, because `assert` is not allowed in a constructor.

## List a directory

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `entries()` | `List<Directory.Entry>` | every folder and file inside it, as `Directory` and `File` values |
| `folders()` / `files()` | `List<String>` | names only, sorted |
| `exists()` / `create()` | `Bool` | |

```gdscript title=directory_tasks/directory_tasks.spite entry
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

`files()` and `folders()` answer names, not paths; `entries()` answers values you can walk.

## Walk a directory tree

A `Directory` has a `path`, the way a `File` does, and `entries()` lists what is inside it as a
`List<Directory.Entry>`: each entry is a `Directory` or a `File` whose `path` is already joined to its parent's,
so a `switch` tells them apart and a folder is walked by calling the same function again.

```gdscript title=directory_walk/directory_walk.spite entry
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

```gdscript title=process_tasks/process_tasks.spite entry
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

## `Console`

`Console()` is a singleton: the same instance everywhere, so every file holds `var console = Console()`, and
`Console().print(...)` is an error naming it ([classes_and_files.md](classes_and_files.md#singletons)).

| Member | Does |
|---|---|
| `print(...values)` | each value's `to_string()`, separated by a space, then a line break |
| `write(...values)` | the same without the line break |
| `error(...values)` | like `print`, to the error stream |
| `debug(...values)` | each value's `to_debug()`, separated by a space, then a line break |
| `flush()` | writes out whatever the output and error streams still hold |
| `read_line()` | one line of input without its line break, as a `String?`: `null` only at the end of the input |

`print`, `write` and `error` are ordinary functions in `library/console.spite`, taking
`...values: List<Printable>`, where `Printable` is a `type` that requires `to_string(): String`. Every number,
`Bool`, `String`, `Symbol`, enum value, `Spite.Class` and `Spite.Namespace` answers it, and a class of yours
prints once it declares `to_string()`. Passing one that does not is a compile error naming `to_string`. Writing
the characters out is the compiler's part (`_write_output`, `_write_error` and `flush` have no body in Spite).

```gdscript title=printable_doc/ticket.spite
var code = ""
var seats = 0

func Ticket(starting_code: String, starting_seats: Int) {
    code = starting_code
    seats = starting_seats
}

func to_string(): String {
    return "{code} for {seats}"
}
```
```gdscript title=printable_doc/printable_doc.spite entry
var console = Console()

func PrintableDoc() {
    var ticket = Ticket("A12", 3)
    var boarding: Symbol = 'boarding'
    console.print("next:", ticket, boarding, true, 2.5, ticket.class)
}
```
```output
next: A12 for 3 boarding true 2.5 Ticket
```

`debug` is for a person or an AI reading a program's state rather than for its output. It takes
`...values: List<Debuggable>` (`to_debug(): String`), and every value answers that without writing anything: a
class shows its name and every attribute, nesting into the attributes that are classes, a `List` shows `[a, b]`, a
`Dictionary` `{"key": value}`, text is quoted, a `Symbol` or enum value is written `'like_this'`, and an absent
`T?` is `null`. An object already being shown further up is written `Name {...}`, so a cycle ends. A class that
declares its own `to_debug()` is shown by it wherever it appears, and a class nothing asks to show gets none:
the automatic one is `Spite.Debug<$value_type>` in `library/spite/debug.spite`, compiled only for the classes a
program debugs.

```gdscript title=debug_doc/crew.spite
var captain = ""
var sailors = List<String>()
var ship: Ship? = null
```
```gdscript title=debug_doc/ship.spite
var name = ""
var crew: Crew? = null

func Ship(starting_name: String) {
    name = starting_name
}
```
```gdscript title=debug_doc/debug_doc.spite entry
var console = Console()

func DebugDoc() {
    var crew = Crew()
    crew.captain = "Ana"
    crew.sailors.append("Bo")
    var ship = Ship("Gull")
    crew.ship = ship
    ship.crew = crew
    console.debug(crew, 3, "done")
    ship.crew = null
}
```
```output
Crew { captain: "Ana", sailors: ["Bo"], ship: Ship { name: "Gull", crew: Crew {...} } } 3 "done"
```

```gdscript title=console_input_doc/console_input_doc.spite entry
var console = Console()

func ConsoleInputDoc() {
    console.write("name? ")
    var name = console.read_line()
    if name {
        console.print("hello", name)
    } else {
        console.print("no input")
    }
}
```
```output
name? no input
```

## `Program`

`Program()` is this running program, a singleton bound once as `var program = Program()`.

| Member | Does |
|---|---|
| `exit(code)` | flushes what was printed and ends the process with `code` |
| `sleep(milliseconds)` | waits; other `Concurrent` work runs meanwhile ([concurrency.md](concurrency.md)) |
| `environment(name)` | the process environment variable, as a `String?` |
| `live_allocations()` | how many allocations are alive, under `--debug_memory` |

The entry constructor returning normally is exit code `0`.

```gdscript title=program_basics/program_basics.spite entry
var console = Console()

func ProgramBasics() {
    var program = Program()
    var missing = program.environment("SPITE_DOCUMENTATION_UNSET_VARIABLE")
    if missing {
        console.print("set to", missing)
    } else {
        console.print("not set")
    }
    program.sleep(1)
    console.print("slept")
}
```
```output
not set
slept
```

## `Clock`

`Clock()` is a singleton with two readings (the names are proposed, not yet confirmed):

| Member | Does |
|---|---|
| `elapsed_nanoseconds(): Long` | a monotonic clock with an arbitrary start: subtract two readings to measure |
| `elapsed_milliseconds(): Long` | the same, in milliseconds |
| `now(): Instant` | the wall clock, as an exact [`Instant`](time.md): show it through a time zone |

Each system reads its own clock, in `library/windows/clock.spite` (`QueryPerformanceCounter`,
`GetSystemTimeAsFileTime`) and the `linux` and `mac` folders (`clock_gettime`), through `DynamicLibrary`.

```gdscript title=clock_basics/clock_basics.spite entry
var console = Console()
var clock = Clock()
var program = Program()

func ClockBasics() {
    var started = clock.elapsed_nanoseconds()
    program.sleep(5)
    var finished = clock.elapsed_nanoseconds()
    var waited = (finished - started) / 1000000
    console.print("waited at least 4 ms:", waited >= 4)
}
```
```output
waited at least 4 ms: true
```

## `Socket`

`Socket()` is a TCP connection on `127.0.0.1` and nowhere else, which is what `--repl_port` and `spite connect`
are written with: `listen_locally(port)`, `accept_client(): Socket?`, `connect_locally(port)`,
`read_line(): String?`, `write_line(text)` and `close()`. A program that uses it waits in `accept_client` and
`read_line` the way it waits anywhere, so other `Concurrent` work runs meanwhile. HTTP is not built.
