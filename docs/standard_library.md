# Standard library

The standard library is ordinary Spite in `library/`, and a program reads it the way it reads its own code:
every class a program can name (`String`, `List`, `Integer`, `File`, `Memory.Heap`) is a file there, and
`--final-classes` prints each one as the program uses it. Its values are reference counted like every other
object ([memory.md](memory.md)). When an operation cannot succeed it says so in its type rather than crashing:
an index or a key that is not there reads as `T?`, a file that cannot be read answers `null`, and text that does
not parse as a number reads as `0`.

`library/` holds what every operating system shares; `library/windows/`, `library/linux/` and `library/mac/`
reopen the classes each system does differently, and the launcher loads the one the program is compiled for
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

**Use the library's class instead of making your own.** A class of your own named like one of the library's, a
`Geometry.Vector3` or a `Color` in a folder, is a compile error naming the library's class to use, and what it lacks
is added by reopening it ([A class name means one class](classes_and_files.md#a-class-name-means-one-class)).

**A program carries only the classes it uses.** The library is part of every program's
source, but a production build keeps only the C that `main` can reach: a program that never makes a `FileSystemWatcher`,
a `Socket`, a `Process` or a `ThreadPool` has none of their code, and none of the operating-system functions only
they call is looked up when the program starts ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
What a class costs when it is used is what its Spite does, and each section below says where that is more than a
call. An inspectable build (`--repl`, `--repl-port`, `--hot-reload`, `--development`) keeps everything, so the
REPL can look at any of it.

## What is in it

| Class | What it is | Page |
|---|---|---|
| `String` | immutable text | [below](#string) |
| `Integer`, `Long`, `Float`, `Double`, `Boolean`, ... | numbers, as classes, with their maths (`square_root()`, `sine()`, `Float.pi`, ...) | [values_and_types.md](values_and_types.md#numbers-are-classes), [maths](values_and_types.md#maths-functions) |
| `Nothing`, `Anything` | what a function returns when it returns nothing; the empty `type` every class fits | [functions_and_operators.md](../specs/functions_and_operators.md#calling-one) |
| `Number` | the `type` every number class fits: its operators and `to_long()`, `to_double()` | [values_and_types.md](values_and_types.md#every-number-fits-number) |
| `List<T>`, `Dictionary<Key, Value>` | containers, and the member templates | [collections.md](collections.md) |
| `Console` | the terminal: print, read a line | [below](#console) |
| `File`, `Directory` | files and folders | [below](#read-and-write-a-file) |
| `FileSystemWatcher` | the paths that changed under a file or a folder, told by the operating system | [below](#watch-files-and-folders) |
| `Process` | run another program | [below](#run-a-process) |
| `Program` | this program: exit, sleep, environment variables, its own path | [below](#program) |
| `Clock` | elapsed time for measuring, and the wall clock | [below](#clock) |
| `Instant`, `Duration`, `Date`, `Time`, `DateTime`, `Period`, `TimeZone`, `TimeZones`, `TimeText` | exact time, the calendar, time zones as presentation, ISO 8601 text | [time.md](time.md) |
| `Environment`, `Build`, `Arguments` | settings and the command line | [programs.md](programs.md) |
| `Reload` | how many reloads a `--hot-reload` program has swapped in, and which classes they rebuilt | [repl.md](repl.md#knowing-what-a-reload-rebuilt) |
| `JsonWriter<T>`, `JsonReader<T>` | any value to JSON text and back | [json.md](json.md) |
| `BinaryWriter<T>`, `BinaryReader<T>` | any value to compact bytes (a `List<Byte>`) and back, for Spite programs talking to each other and for files | [json.md](json.md#write-and-read-bytes) |
| `Vector2`, `Vector3`, `Vector4`, `Matrix3`, `Matrix4`, `Quaternion`, `AxisAlignedBox`, `Plane`, `Frustum`, `Ray`, `Color`, `ColorText`, `CubicBezier`, `Easing`, `Noise` | game maths: points, directions, transforms, rotations, culling and picking, colours in the web's formats, curves, easings and noise | [game_maths.md](game_maths.md) |
| `Concurrent`, `Parallel`, `ThreadPool` | run a function while waiting, or on the thread pool; the handle is the value | [concurrency.md](concurrency.md) |
| `ThreadLocal<T>`, `Lock`, `ThreadSlot` | a value per thread, and a lock | [concurrency.md](concurrency.md#a-value-per-thread-and-a-lock) |
| `Atomic<T>` | a whole number or `Boolean` threads share without a lock | [concurrency.md](concurrency.md#a-number-every-thread-shares-atomict) |
| `Socket` | TCP over IPv4 and IPv6: listen, connect, lines and bytes, waiting or not | [below](#socket) |
| `UdpSocket` | UDP datagrams over IPv4 and IPv6 | [below](#udpsocket) |
| `HttpServer`, `HttpClient`, `HttpRequest`, `HttpResponse` | HTTP/1.1, connections kept alive | [below](#http) |
| `WebSocket`, `WebSocketText`, `WebSocketBinary` | WebSocket (RFC 6455) over an HTTP upgrade: text and binary messages both ways, ping and close | [below](#websocket) |
| `Base64`, `Deflate`, `Zlib`, `Gzip` | bytes as base64 and base64url text; bytes compressed as raw DEFLATE, zlib and gzip | [below](#bytes-base64-compression-hashes-and-passwords) |
| `Sha256`, `SecureRandom`, `Argon2` | SHA-256 and HMAC-SHA256; bytes from the operating system's secure random source; password hashes as PHC strings | [below](#bytes-base64-compression-hashes-and-passwords) |
| `Memory.Address`, `Memory.Heap`, `Memory.Arena`, `TypedMemory<T>` | a place in memory, the allocators that own it, and values of any type there: the floor every other type is built on | [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it) |
| `DynamicLibrary` | call a native library | [foreign_libraries.md](foreign_libraries.md) |
| `Spite.Class`, `Spite.Function`, ... | reflection | [reflection.md](reflection.md) |
| `Spite.Memory` | where a value lives, `value.memory` | [memory.md](memory.md#where-a-value-lives-memory) |

The other files of `library/` are the machinery these are built from: `Scheduler` (the event loop under
`Concurrent`), `HotReload`, `ReadEvaluatePrintLoop`, `AllocationTable` (`--debug-memory`'s table), `NumberText`,
`JsonCursor`, `BinaryFormat` and the time-zone readers. They are ordinary classes a program can read and the REPL
can inspect, which a program has no reason to call.

## What belongs in the standard library

Anything that no company owns belongs here: a protocol, a file format, an algorithm, a piece of maths. HTTP, TLS,
SHA-256 and HMAC, Argon2, secure random bytes, base64 and gzip are standard, and the library grows to cover them
as programs need them. A client for a branded product (a database such as MongoDB or Postgres, a vendor's API) is
never standard: it is a package, kept by whoever maintains it, and a program loads it pinned to a commit like any
other repository
([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)). `library/windows/`, `library/linux/` and
`library/mac/` are not brand packages: they are how a standard class is written for one system.

The library has TCP and UDP over IPv4 and IPv6, HTTP/1.1 with kept-alive connections and WebSocket over it
([below](#socket)), SHA-256 and HMAC, Argon2id, secure random bytes, base64 and base64url, and DEFLATE, zlib and
gzip ([below](#bytes-base64-compression-hashes-and-passwords)). TLS belongs to it as well.

## `String`

Immutable. Text of up to 15 bytes is kept inside the `String` value itself and allocates nothing; longer text is
one reference-counted block ([optimizations.md](optimizations.md#short-text-lives-inside-the-string)), and none of
it changes what a program means. A value is placed inside written text, `"hello {name}"`, and two values join
with `+` ([values_and_types.md](values_and_types.md#string)). `==`, `!=`, `<` and `>` compare by content.

No member of a `String` halts: an index past the end answers `""` or `0`, a range is clamped to the text, and a
search that finds nothing answers `-1`. Reading a number is the one that can find nothing to answer, so
`to_integer()` and the other `to_<type>()` readings answer a `T?`: `null` when the text is not a number that fits
the type. Assigning text to a number calls the matching one, so it is allowed only into a `T?`:
`var age: Integer? = "42"` is `42`, and `var age: Integer = "42"` is an error naming `to_integer()`. Every member,
with what it answers at the edges, is in [the specification](../specs/standard_library.md#the-string-class).

```gdscript title=string_members/string_members.spite entry
var console = Console()

func StringMembers() {
    var line = "  ada, grace, linus  "
    var names = line.trim().split(", ")
    var first = names.first()
    crash first
    var shouted = first.upper_case()
    var count = "3".to_integer()
    crash count
    var missing = line.index_of("barbara")
    var tail = first.slice(1, 99)
    var past_end = first.character_at(10)
    console.print(shouted, count, missing, tail, "[{past_end}]")
    var file_lines = "one\ntwo".lines()
    var line_count = file_lines.count()
    var renamed = line.replace("ada", "Ada").trim()
    console.print(line_count, renamed)
}
```
```output
ADA 3 -1 da []
2 Ada, grace, linus
```

## Read and write a file

`File(path)` is a value, a path, and several may exist at once.

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `to_string()` | `String` | the path, so a `File` prints as its path and a crash on one names it ([failure.md](failure.md#what-a-crash-reports)) |
| `read()` | `String?` | `null` when the file cannot be read |
| `write(text)` / `append(text)` | `Boolean` | replaces the content / adds to its end |
| `exists()` / `remove()` | `Boolean` | |
| `move_to(path)` | `Boolean` | renames or moves it ([below](#rename-or-move-a-file-or-folder)) |
| `size()` | `Long?` | how many bytes it holds; `null` when it cannot be opened |
| `modified()` | `Instant?` | when it was last written ([time.md](time.md)); `null` when it does not exist |
| `read_bytes(position, count, address)` | `Long?` | reads up to `count` bytes starting `position` bytes in, into memory at `address`; answers how many it read (`0` at the end), `null` when it cannot be opened |
| `write_bytes(address, count)` | `Boolean` | replaces the content with `count` bytes from memory at `address` |
| `append_bytes(address, count)` | `Long?` | adds `count` bytes to the end; answers the position they start at, `null` when they could not all be written |
| `map()` | `MappedFile?` | the whole file mapped into memory, read-only ([below](#a-file-larger-than-memory-map-it)); `null` when it cannot be opened |

```gdscript title=file_tasks/file_tasks.spite entry
var console = Console()

func FileTasks() {
    var log_file = File(".spite/documentation_demo_log.txt")
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

`read()` is a `String?`: narrow it with `if content { }`, `assert content` (which returns the function's
default) or `crash content` (which halts), exactly like any other `T?` ([failure.md](failure.md)). This entry
constructor uses `crash`, because `assert` is not allowed in a constructor.

**Bytes.** A cache or an asset file is bytes, not text: `read_bytes` reads into memory from any position, which is
what seeking is for, and `append_bytes` says where a record landed, which is what an index needs. The memory is a
`Memory.Address` from `Memory.Heap`, which a program fills and reads through `TypedMemory<T>`
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)). Every call opens and closes the
file, so read a large file in one call and walk it in memory rather than record by record.

```gdscript title=byte_records/byte_records.spite entry
var console = Console()
var heap = Memory.Heap()
var longs = TypedMemory<Long>()

func ByteRecords() {
    var store = File(".spite/documentation_byte_records.bin")
    var record = heap.allocate(8)
    longs.write_value(record, 0, 1111)
    store.write_bytes(record, 8)
    longs.write_value(record, 0, 2222)
    var second_at = store.append_bytes(record, 8)
    crash second_at
    var read_back = heap.allocate(8)
    var got = store.read_bytes(second_at, 8, read_back)
    crash got
    var second = longs.read_value(read_back, 0)
    var size = store.size()
    crash size
    console.print("the second record starts at", second_at, "and holds", second, "of", size, "bytes")
    heap.free(record)
    heap.free(read_back)
}
```
```output
the second record starts at 8 and holds 2222 of 16 bytes
```

### Rename or move a file or folder

`move_to(path)` is one function for both, on a `File` and on a `Directory`: a new name in the same folder renames
it, a path in another folder moves it there. It answers `true` when it moved, and the value then names the new
path, so the same `File` keeps working. It never replaces anything: when a file or folder is already at `path` it
answers `false` and changes nothing, so a move can never lose what was there. It answers `false` too when there is
nothing to move, or when the system cannot move it (a folder to another drive).

```gdscript title=moving_files/moving_files.spite entry
var console = Console()

func MovingFiles() {
    var folder = Directory(".spite/documentation_moving")
    folder.create()
    var draft = File(".spite/documentation_moving/draft.txt")
    draft.write("notes")
    var renamed = draft.move_to(".spite/documentation_moving/final.txt")
    console.print(renamed, draft.name)
    var keeper = File(".spite/documentation_moving/keeper.txt")
    keeper.write("keep me")
    var replaced = draft.move_to(".spite/documentation_moving/keeper.txt")
    var kept = keeper.read()
    crash kept
    console.print(replaced, kept)
    draft.remove()
    keeper.remove()
}
```
```output
true final.txt
false keep me
```

### A file larger than memory: map it

`file.map()` maps the whole file into the program's memory, read-only, without reading it: the operating system
brings each page in the first time it is read and may drop it again, so a file of many gigabytes is walked as if
it were in memory. A `MappedFile` answers `size(): Long`,
and reads by position (`mapped[position]` (a `Byte`), `read_short`, `read_integer`, `read_long`, `read_float`,
`read_double` and `text(position, count)`), each a `T?` that is `null` when the bytes it would read are not all
inside the file, since a position read from a file is data from outside ([failure.md](failure.md)). The mapping
is undone when the `MappedFile` is dropped, and a file that is mapped cannot be removed on Windows until then.

```gdscript title=mapped_records/mapped_records.spite entry
var console = Console()
var heap = Memory.Heap()
var longs = TypedMemory<Long>()

func MappedRecords() {
    var store = File(".spite/documentation_mapped_records.bin")
    var record = heap.allocate(16)
    longs.write_value(record, 0, 1111)
    longs.write_value(record, 1, 2222)
    store.write_bytes(record, 16)
    heap.free(record)
    total_records(store)
    store.remove()
}

func total_records(store: File) {
    var mapped = store.map()
    crash mapped
    var total: Long = 0
    var position: Long = 0
    while position < mapped.size() {
        var value = mapped.read_long(position)
        crash value
        total = total + value
        position = position + 8
    }
    var past_end = mapped.read_long(16)
    console.print("the records add up to", total, "and a read past the end is null:", not past_end)
}
```
```output
the records add up to 3333 and a read past the end is null: true
```

To stream instead (a file read once, front to back, with no mapping), `read_bytes(position, count, address)`
already covers it: each call opens, seeks and closes the file, which measured about 50 microseconds a call on
Windows, so a loop reading a megabyte at a time spends almost nothing on it (512 MB in 169 ms, against 572 ms in
64 KB pieces, warm cache). Read a megabyte or more per call; there is no separate reader to open and close.

**Independent reads overlap.** Two or more untyped `var name = file.read()` in a row (or `socket.read_line()`),
each on a name or an attribute and none naming an earlier one, are started together and all finished before the next
statement: the compiler starts each but the last as a `Concurrent` of its own, so the program waits once for the
slowest instead of once per file. Nothing is written for it, and nothing after the reads can see a difference:
a file written by the next statement is written after every read has finished
([concurrency.md](concurrency.md#reads-in-a-row-overlap)). What it costs is what a `Concurrent` costs, a frame on
the heap for each read but the last and the event loop that finishes them, and only in a program that reads this
way ([optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used)). A read or write of a
`File`, `console.read_line()`, a `Socket`'s `accept_client()`, `read_line()` and `read_bytes()`, and
`program.sleep()` are the library's waits: inside a `Concurrent` each lets other work run. Everything else,
`Process.run()` included, blocks the thread that calls it, except a `Socket`'s `_now` calls, which never wait.

## List a directory

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `name` | `String` | the last piece of the path: `Directory("levels/forest").name` is `forest` |
| `entries()` | `List<Directory.Entry>` | every folder and file inside it, as `Directory` and `File` values, each kind sorted by name with a merge sort (`n log n`), so a folder of thousands of files lists quickly |
| `exists()` / `create()` | `Boolean` | |
| `move_to(path)` | `Boolean` | renames or moves it with everything inside ([below](#rename-or-move-a-file-or-folder)) |

```gdscript title=directory_tasks/directory_tasks.spite entry
var console = Console()

func DirectoryTasks() {
    var target = Directory(".spite/documentation_demo_dir")
    target.create()
    var target_exists = target.exists()
    console.print("exists", target_exists)
    var examples = Directory("examples")
    var has_hello = examples.entries().filter_directories().map_names().contains("hello")
    console.print("has hello", has_hello)
}
```
```output
exists true
has hello true
```

`entries()` answers values you can walk, and its member templates keep one kind:
`folder.entries().filter_files()` is a `List<File>` and `filter_directories()` a `List<Directory>`
([a list of a union](metaprogramming.md#member-templates)).

## Walk a directory tree

A `Directory` has a `path`, the way a `File` does, and `entries()` lists what is inside it as a
`List<Directory.Entry>`: each entry is a `Directory` or a `File` whose `path` is already joined to its parent's,
so a `switch` tells them apart and a folder is walked by calling the same function again.

```gdscript title=directory_walk/directory_walk.spite entry
var console = Console()

func DirectoryWalk() {
    var root = Directory(".spite/documentation_walk")
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
        var entry = entries[index]
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
folder .spite/documentation_walk/inner
file .spite/documentation_walk/inner/deep.txt
file .spite/documentation_walk/top.txt
```

Folders come first, then files, each sorted by name, and `.` and `..` are never listed.

## Watch files and folders

`FileSystemWatcher()` is told by the operating system which paths changed, so nothing reads the disk over and over.
Making one takes no arguments and starts nothing: no thread and no job exist until something is watched. One watcher
watches any number of files and folders, and `watch_for_changes` takes either, so a folder or a file a program
already holds is passed as it is.

| Member | Result | Notes |
|---|---|---|
| `watch_for_changes(target: FileSystemWatcher.Target)` | `Boolean` | `Target` is the union of `Directory` and `File`: a `File`, or a `Directory` with everything below it; `false` when there is nothing there to watch |
| `changes()` | `List<String>` | never waits: the paths changed since the last call, each once, or none |
| `wait_for_changes()` | | blocks this thread until `changes()` has something to answer |

A change is reported once the watcher has seen none for 100 ms, so a burst (a save that writes a file in pieces,
a checkout that touches a hundred) comes back as one list, each path once, in the order they first changed.
Each path is the `path` of the watched `Directory` or `File` joined with what is below it, with `/` between. A
folder is reported when it is created, removed or renamed, not when what is inside it changes: the files inside
are reported instead. The 100 ms are counted from when the watcher sees the change, which is in a call, so a
program that calls `changes()` once a frame sees a save about 100 ms after it happened.

```gdscript title=watch_folder/watch_folder.spite entry
var console = Console()
var program = Program()

func WatchFolder() {
    var folder = Directory(".spite/documentation_watch")
    folder.create()
    var watcher = FileSystemWatcher()
    watcher.watch_for_changes(folder)
    File("{folder.path}/level.txt").write("three goblins")
    var changed = watcher.changes()
    var tries = 0
    while changed.is_empty() and tries < 250 {
        program.sleep(20)
        changed = watcher.changes()
        tries = tries + 1
    }
    var changed_paths = changed.join(", ")
    console.print("changed:", changed_paths)
    var again = watcher.changes()
    var again_count = again.count()
    console.print("changed since:", again_count)
}
```
```output
changed: .spite/documentation_watch/level.txt
changed since: 0
```

`changes()` suits a program that already runs a loop, a game once a frame, a tool between steps. A thread with
nothing else to do calls `wait_for_changes()` and then `changes()`, which is what `--hot-reload` does
([repl.md](repl.md#how-it-works)); on the program's main thread it would stop everything else, `Concurrent` work
included, until something changes.

Each system's folder asks its own kernel: `ReadDirectoryChangesW` on the folder, with its sub-folders, on Windows;
an `inotify` watch on each folder on Linux, adding one for each new folder; and a `kqueue` entry on each file and
folder on macOS, adding new files when their folder changes. A file is watched through its folder on Windows and
Linux, so a save that replaces the file is still seen. Nothing of it is in a program that never calls `FileSystemWatcher()`:
not the code, and not the lookups of the system's functions ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).

## Run a process

```gdscript title=process_tasks/process_tasks.spite entry
var console = Console()

func ProcessTasks() {
    var listing = Process("echo", ["build", "finished"])
    var code = listing.run()
    console.print("exit code", code)
    var trimmed_output = listing.output().trim()
    console.print("output", trimmed_output)
}
```
```output
exit code 0
output build finished
```

| Member | Result | Notes |
|---|---|---|
| `Process(command, arguments)` | | `arguments` is a `List<String>`, each one argument of the child, quoted for you (below) |
| `working_directory` | `String` | the folder the child runs in; `""`, the default, is the program's own |
| `environment_variables` | `Dictionary<String, String>` | variables set for the child alone, on top of the program's own: `process.environment_variables["LOG"] = "1"` |
| `run()` | `Integer` | runs it through the system's shell, waits for it and answers its exit code (`-1` when it could not start; on Linux and macOS, a child a signal ended answers 128 plus the signal's number, as a shell reports it) |
| `output()` | `String` | what it wrote to its standard output, valid after `run()` |
| `run_attached()` | `Integer` | runs it with the program's own terminal, so what it writes and reads is the user's, and answers its exit code, read as `run()` reads it |

Each argument reaches the child whole, as one argument, whatever it holds. On Linux and macOS each is put in single
quotes for the shell, so the child's `argv` holds exactly the text given. On Windows a program reads its own
command line, so an argument is quoted the way Windows splits one back apart (the rules of `CommandLineToArgvW`):
an argument with no space, tab, quote or `& | < > ^` is written as it is; one with them is put in double quotes,
with a `"` inside written `\"` and the backslashes before a quote doubled; and one shaped `key=value` quotes only
its value, so `-script=a b` reaches the child's command line as `-script="a b"` (the form Unreal and other
programs that read `-key=value` themselves expect) and still splits back into `-script=a b`
(`conformance/stage4/process_settings`). A `%` is still read by the Windows shell.

`working_directory` and `environment_variables` are set by the shell line itself, before the command: a
`cd` into the folder and one assignment per variable, so a relative command or path is read from that folder, and
a folder that does not exist fails the run with the shell's exit code. `output()` is read 4 KiB at a time and each
piece appended in place, so reading 50 MB takes as long as the child takes to write it.

`output()` includes the child process's own trailing newline, which is why the example above calls `.trim()`.
What the child writes to its error stream is not captured: it goes to the program's own error stream. `run()`
is not one of the library's waits: it blocks the thread that calls it, and other `Concurrent` work does not run
meanwhile ([which calls wait](#read-and-write-a-file)).

## `Console`

`Console()` is a singleton: the same instance everywhere, so every file holds `var console = Console()`, and
`Console().print(...)` is an error naming it ([classes_and_files.md](classes_and_files.md#singletons)).

| Member | Does |
|---|---|
| `print(...values)` | each value's `to_string()`, separated by a space, then a line break |
| `write(...values)` | the same without the line break |
| `error(...values)` | like `print`, to the error stream |
| `debug(...values)` | each value's `to_debug()`, separated by a space, then a line break |
| `read_line()` | one line of input without its line break, as a `String?`: `null` only at the end of the input |

**You never flush.** The compiler chooses how printed output reaches its destination, and whatever it chooses,
three things hold: a line is never split, nor mixed with a line another thread prints at the same time; everything
printed is out before the program exits and before a crash report; and text written before the program reads
input, a prompt, is visible before the read. `print`, `error` and `debug` end a line and hand it to the operating
system at once, whether the output is a terminal, a file or a pipe, so a server's log redirected to a file shows
each line when it happens rather than when the program ends. `write` leaves its text for the next line end, the
next read of input or the end of the program
([the rule and its measurement](../specs/standard_library.md#system-classes)). There is no `flush()` to call.

`print`, `write` and `error` are ordinary functions in `library/console.spite`, taking
`...values: List<Printable>`, where `Printable` is a `type` that requires `to_string(): String`. Every number,
`Boolean`, `String`, `Symbol`, enum value, `Spite.Class` and `Spite.Namespace` answers it, and a class of yours
prints once it declares `to_string()`. Passing one that does not is a compile error naming `to_string`; a `List`
or `Dictionary` has none either, so show it with `debug` or `join` it first
([the exact errors](../specs/standard_library.md#system-classes)).

```gdscript title=printable_doc/ticket.spite
var code = ""
var seats = 0

func Ticket(starting_code: String, starting_seats: Integer) {
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

`debug` is for reading a program's state rather than for its output. It takes
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
| `executable_path()` | the path of the running executable, as the operating system gives it |
| `live_allocations()` | how many allocations are alive, under `--debug-memory` |

The entry constructor returning normally is exit code `0`.

```gdscript title=program_basics/program_basics.spite entry
var console = Console()
var program = Program()

func ProgramBasics() {
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

`Clock()` is a singleton with two readings:

| Member | Does |
|---|---|
| `elapsed_nanoseconds(): Long` | the monotonic clock, in nanoseconds from an arbitrary start: subtract two readings to measure |
| `elapsed_milliseconds(): Long` | the same, in milliseconds |
| `now(): Instant` | the wall clock, as an exact [`Instant`](time.md): show it through a time zone |

Each system reads its own clock, in `library/windows/clock.spite` (`QueryPerformanceCounter`,
`GetSystemTimeAsFileTime`) and the `linux` and `mac` folders (`clock_gettime` with `CLOCK_MONOTONIC` and
`CLOCK_REALTIME`), through `DynamicLibrary`. A reading allocates nothing: the operating system writes it into a slot
in the calling function's frame ([placement](../specs/memory.md#placement-the-compiler-decides-where-memory-lives)),
and Windows' counter frequency, fixed at boot, is read once when `Clock()` is first made. The unit is the
nanosecond on every system; the resolution is the hardware's (100 ns for Windows' usual 10 MHz counter, 1 ns on
Linux and macOS). A profiler can read it twice per system per frame and measure nothing but the reads
(`conformance/stage6/clock_reads` pins 1 000 readings at no allocation).

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

`Socket()` is a TCP connection over IPv4 or IPv6, the same on Windows (winsock), Linux and macOS. `--repl-port`
and `spite connect` are written with it, and so is a game server. It is public library surface.

| Member | Does |
|---|---|
| `listen_locally(port)`, `listen_everywhere(port)`, `listen_at(host, port)` | listens on `127.0.0.1`, on every interface of both IPv4 and IPv6, or on the one interface a host name or address names (a name with both kinds of address, like `"localhost"`, listens on its IPv4 one); `false` when it cannot |
| `connect_locally(port)`, `connect(host, port)` | connects to `127.0.0.1`, or to a host name (`"example.com"`, `"localhost"`), an IPv4 address (`"192.168.1.20"`) or an IPv6 address (`"::1"`, `"2001:db8::7"`), trying each address a name resolves to in turn, its IPv4 ones first; `false` when the name does not resolve or nobody answers |
| `accept_client(): Socket?` | waits for the next client |
| `read_line(): String?` | waits for a whole line, without its line break |
| `read_bytes(address, count): Integer` | waits until at least one byte has arrived, puts up to `count` at `address`, and answers how many |
| `write_line(text): Boolean`, `write_bytes(address, count): Boolean` | sends all of it, waiting while the system's buffer is full |
| `accept_client_now(): Socket?` | the next client if one is already connecting, otherwise `null` at once |
| `read_line_now(): String?` | a whole line if one has arrived, otherwise `null` at once |
| `read_bytes_now(address, count): Integer` | puts what has already arrived, up to `count`, at `address` and answers how many: `0` is nothing yet |
| `write_bytes_now(address, count): Integer` | hands the system as much as it takes now and answers how many; the rest is the caller's to send later |
| `closed: Boolean` | `true` once the other end has closed the connection (or it broke, or `close()` was called) |
| `close()` | closes it |

**Waiting and not waiting.** The calls without `_now` wait the way a program waits anywhere: inside a
`Concurrent`, `accept_client`, `read_line` and `read_bytes` are points the state machine returns from, so other
work runs meanwhile ([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait)). The `_now` calls never
wait and are plain calls, for a loop that polls every connection once a tick (a game server, which has its own
frame to keep). Resolving a host name and connecting always wait in place.

**A closed peer is an answer, not a failure.** `read_line()` answers `null` when the
connection ended before a whole line; `read_line_now()` answers `null` for that and when no whole line has arrived
yet, and `read_bytes_now` answers `0` both for "nothing yet" and for "closed". `closed` is what tells them apart
(rather than a count of `-1`), so a count stays a count:

```gdscript
func poll(connection: Socket) {
    var received = connection.read_bytes_now(buffer, 4096)
    if received > 0 {
        handle(received)
    } else if connection.closed {
        forget(connection)
    }
}
```

A write to a closed connection sends nothing, answers `false` or `0`, and sets `closed` too; nothing crashes.
Addresses are `Memory.Address`es, so a buffer is `heap.allocate(bytes)` and its bytes are written with
`TypedMemory<Byte>` or a class of the program's own over the heap ([memory.md](memory.md)).
`UdpSocket`, HTTP and WebSocket are [below](#udpsocket).

## `UdpSocket`

`UdpSocket()` sends and receives UDP datagrams over IPv4 and IPv6, the same on Windows, Linux and macOS. A datagram is a `List<Byte>`, and one call is one datagram.

| Member | Does |
|---|---|
| `open()`, `open_locally(port)`, `open_everywhere(port)`, `open_at(host, port)` | opens on a port the system picks (a client), or on `port` of `127.0.0.1`, of every interface of both IPv4 and IPv6, or of the one interface a host name or address names (`"::1"`; a name with both kinds of address opens on its IPv4 one); port `0` is one the system picks; `false` when the port is taken or the name does not resolve |
| `port: Integer` | get-only: the port the socket is open on, the one the system gave it after `open()` (asked of the system with `getsockname`), so a client can tell a server where to answer; reading it from a socket that is not open halts, since it has none, and a program that wants the number after `close()` keeps it in a `var` before closing, which says it means to |
| `send_to(host, port, bytes): Boolean` | sends one datagram to a host name or an IPv4 or IPv6 address; a name with both kinds of address is sent to its IPv4 one; `false` when the name does not resolve to an address the socket can reach (an IPv6 one from `open_locally`) or the system refuses it |
| `receive(): List<Byte>?` | waits for the next datagram; `null` when the socket is closed |
| `receive_now(): List<Byte>?` | the next datagram if one has arrived, otherwise `null` at once |
| `sender_host(): String`, `sender_port(): Integer` | who sent the datagram received last, to answer with `send_to`: an IPv4 address as `"127.0.0.1"`, an IPv6 one in its shortest form (`"::1"`, `"2001:db8::7"`) |
| `close()` | closes it |

A datagram can arrive empty, which is a list of no bytes, not `null`. UDP itself neither orders nor repeats lost
datagrams; reliable channels are the program's.

## HTTP

`HttpServer` and `HttpClient` speak HTTP/1.1 over `Socket`, keeping a connection open for the next exchange.
A message is an `HttpRequest` (`method`, `path`, `version`, `headers`, `body`) or an `HttpResponse` (`status`,
`headers`, `body`); `header(name)` reads a header whatever its case and `set_header(name, value)` writes one, and
`reason()` is the response's reason phrase. Bodies are text.

| Member | Does |
|---|---|
| `HttpServer.listen_locally(port)`, `listen_everywhere(port)` | listens, like `Socket` |
| `HttpServer.next_request(): HttpRequest?` | waits for the next well-formed request, on a new connection or on one kept open; a malformed one is answered `400 Bad Request` and its connection closed, and a body over `largest_body` (1 MiB) counts as malformed; `null` when the listener fails or is closed |
| `HttpServer.respond(request, response)` | writes the response with its `content-length`, and keeps the connection open for the client's next request unless the client asked to close it |
| `HttpServer.close()` | stops listening and closes every connection kept open |
| `HttpClient.send(host, port, request): HttpResponse?` | sends the request with `host` and `content-length` over a connection kept open to that host and port, or a new one, and reads the response, whether its body is sized, chunked or ends when the connection closes; `null` when nobody answers or the response is malformed |
| `HttpClient.close()` | closes the connections it keeps open |

**Connections are kept alive.** HTTP/1.1 keeps a connection open between exchanges, and both sides do: a server
answers `connection: keep-alive` and waits on every open connection and on its listener at once, and a client
keeps the connection after a response and sends its next request to the same host and port over it. A connection
closes when either side says `connection: close` (a request that sets that header gets its answer and then the
close), when an HTTP/1.0 client does not ask for `keep-alive`, when a response's body ends with the connection,
and when it has been idle too long: a server closes one after `idle_limit` milliseconds (5000) without a request,
and keeps at most `largest_waiting` (64) open, closing each further one after its answer; a client keeps at most
`largest_idle` (8). A kept connection the server has closed meanwhile is noticed when the client's next request
gets no answer at all, and that request is sent once more over a new connection. Requests on one connection are
answered in order, and requests a client sent before reading the answers (pipelining) are read one at a time.
A response to `HEAD`, and a `204` or `304`, carries no body. A server still refuses a request with a chunked body.

Inside a `Concurrent`, `next_request` and `send` wait the way `Socket` does, so a server serves while its program
runs ([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait)). A server reads one request at a time:
while a client is slow to send the rest of a request, the other connections wait for it.

## WebSocket

`WebSocket()` keeps one connection open in both directions and carries messages over it, text or bytes, from
either side at any time: what a browser game or a chat speaks once its page has loaded. A connection starts as an
HTTP request, so a server takes it from `HttpServer.next_request()`, and from there both ends are the same class.

```gdscript
var server = HttpServer()

func serve() {
    var request = server.next_request()
    crash request
    var socket = WebSocket()
    assert socket.accept(request)
    var message = socket.receive()
    while message {
        switch message {
            WebSocketText: socket.send_text("you said {message.text}")
            WebSocketBinary: socket.send_bytes(message.bytes)
        }
        message = socket.receive()
    }
}
```

A client asks with an `HttpRequest`, whose `path` and headers (an `origin`, a cookie, the subprotocols it speaks
in `sec-websocket-protocol`) go out with the handshake:

```gdscript
func talk() {
    var socket = WebSocket()
    var request = HttpRequest()
    request.path = "/chat"
    crash socket.connect("127.0.0.1", 8080, request)
    socket.send_text("hello")
    var answer = socket.receive()
    crash answer
    switch answer {
        WebSocketText: console.print(answer.text)
        WebSocketBinary: console.print("{answer.bytes.count()} bytes")
    }
    socket.close()
}
```

| Member | Does |
|---|---|
| `accept(request): Boolean` | answers a request from `HttpServer.next_request()` with `101 Switching Protocols` and keeps its connection; a request that is not a WebSocket handshake is answered `400 Bad Request` (or `426 Upgrade Required` for a version other than 13), its connection closed, and `false` |
| `connect(host, port, request): Boolean` | connects like `Socket.connect`, sends the handshake with the request's path and headers, and checks the server's answer; `false` when nobody answers or the answer is not a WebSocket one |
| `send_text(text): Boolean`, `send_bytes(bytes): Boolean` | sends one message, a text or a `List<Byte>`; `false`, sending nothing, once the connection has ended or `close()` was called |
| `send_ping(bytes): Boolean` | sends a ping carrying up to 125 bytes |
| `receive(): WebSocket.Message?` | waits for the next whole message, a `WebSocketText` (`text`) or a `WebSocketBinary` (`bytes`); `null` once the connection has ended |
| `close()` | starts the closing handshake with code 1000; messages already on their way still arrive through `receive()`, which answers `null` once the other side has answered the close |
| `closed: Boolean` | `true` once the connection has ended, and before `accept` or `connect` has opened it |
| `close_code: Integer?` | how it ended: the code of the closing handshake, `null` while it is open |
| `close_reason: String` | the text the other side's close carried, often empty |
| `last_pong: List<Byte>?` | what the latest pong carried, `null` until one arrives |
| `largest_message: Integer` | the longest message `receive()` takes, 16 MiB unless set |

**What arrives is checked, and a broken connection says so.** Pings are answered with a pong while `receive()`
waits, and a message sent in pieces arrives whole. Anything the protocol forbids ends the connection instead of
being skipped: the side that sees it sends a close with the code that names the problem, and its `receive()`
answers `null` with `close_code` set to that code. The codes are the protocol's own:

| `close_code` | Means |
|---|---|
| `1000` | a normal close, by either side |
| `1001` | the other side went away (a `WebSocket` dropped without `close()` sends it) |
| `1002` | a frame the protocol forbids: one a client did not mask (or a server did), a reserved bit or opcode, a piece of a message that was never started, a ping or close too long or in pieces, a close code nobody may send |
| `1005` | the other side closed without giving a code |
| `1006` | the connection broke without a close at all |
| `1007` | a text message, or a close's reason, that is not valid UTF-8 |
| `1009` | a message longer than `largest_message` |
| `1003`, `1008`, `1010` to `1014`, `3000` to `4999` | whatever the other side closed with |

Inside a `Concurrent`, `receive()`, `accept` and `connect` wait the way `Socket` does
([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait)), so one server talks to many sockets at once,
one `Concurrent` each. `wss`, WebSocket over TLS, belongs here as well.

## Bytes: base64, compression, hashes and passwords

These classes work on `List<Byte>`, and `text.to_bytes()` gives a text's bytes. `bytes.to_utf8_text()` turns
bytes back into text: it answers a `String?`, `null` when the bytes are not valid UTF-8 (a stray continuation
byte, a cut sequence, an overlong form, a surrogate or a value past U+10FFFF), so bytes from outside are never
taken for text they do not spell; on a list of anything but `Byte` it is a compile error
(`conformance/stage6/utf8_text`). Each is written in Spite in `library/`, so a program that uses none of them
carries none of them.

| Class | Member | Does |
|---|---|---|
| `Base64` | `encode(bytes): String`, `encode_url(bytes): String` | RFC 4648 base64 with `=` padding; base64url (`-` and `_`) without it, as JWTs write it |
| | `decode(text): List<Byte>?`, `decode_url(text): List<Byte>?` | the bytes back; `null` for a character outside the alphabet, a length no encoding has, missing padding in `decode`, or set bits after the last byte |
| `Deflate`, `Zlib`, `Gzip` | `compress(bytes): List<Byte>` | raw DEFLATE (RFC 1951), zlib (RFC 1950, Adler-32) or gzip (RFC 1952, CRC-32 and size) |
| | `decompress(bytes): List<Byte>?` | the bytes back, with no length given in advance; `null` for a truncated or malformed stream, a checksum or size that does not match, or bytes after the end |
| `Sha256` | `hash(bytes): List<Byte>` | the 32-byte SHA-256 digest (FIPS 180-4) |
| | `hmac(key, message): List<Byte>` | HMAC-SHA256 (RFC 2104), a key longer than 64 bytes hashed first |
| `SecureRandom` | `bytes(count): List<Byte>` | bytes from the operating system's secure source: `BCryptGenRandom` on Windows, `getrandom` on Linux, `getentropy` on macOS; a failure of the source crashes |
| `Argon2` | `hash(password): String` | a new Argon2id (RFC 9106) hash as a PHC string, `$argon2id$v=19$m=65536,t=3,p=1$<salt>$<tag>`: 64 MiB, 3 passes, 1 lane, a 16-byte random salt and a 32-byte tag |
| | `verify(password, stored): Boolean?` | whether the password is the one `stored` was made from, for `argon2id`, `argon2i` and `argon2d` PHC strings of version 19 with any cost; `null` when `stored` is not such a string, so a damaged record is not taken for a wrong password; the tags are compared in constant time |
| | `derive(password, salt, time_cost, memory_cost, lanes, tag_length): List<Byte>` | the raw Argon2id tag, `memory_cost` in KiB |

A hash takes about 150 ms in an ordinary build and a few seconds in a `--debug-memory` one, whose arithmetic is
checked; it waits in place, so a server runs it on the thread pool. The `Argon2` hash tree-shakes to Blake2b and
the fill of its memory, both private to the class.

---

Next: [Lists, dictionaries and member templates](collections.md), the containers every program uses and how they are extended.
