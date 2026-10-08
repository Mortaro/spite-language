# Foreign libraries and operating systems

A native library is a class, not a keyword. `DynamicLibrary(file, naming, header)` opens a `.dll`, `.so` or
`.dylib`, and every function it exports is called as if it were a member: there is no `extern` declaration, no
binding file and no generated shim. The standard library reaches the operating system this way: `File`,
`Directory`, `Process`, `Console` and the rest are Spite over the C runtime, reached through `DynamicLibrary`, so
what a program can do with it, the library already does.

## Calling a function

```gdscript title=foreign_call/foreign_call.spite entry system=windows
var console = Console()
var c_runtime = DynamicLibrary("ucrtbase.dll", 'identity', "")

func ForeignCall() {
    var text_length = c_runtime.strlen("spite")
    console.print(text_length)
}
```
```output
5
```

`ucrtbase.dll` is Windows' C runtime. A library is a singleton, so it is bound once beside the attributes like any
other ([classes_and_files.md](classes_and_files.md#singletons)); a program for several systems names each
system's file in a class that each system's folder reopens, as the standard library does
([below](#each-operating-system-reopens-what-it-changes)).

- **The arguments are literals**, because the compiler reads them while compiling. The file is named exactly as
  it is on disk, extension included: a name without one is an error.
- **One instance per file, naming rule and header.** `DynamicLibrary` follows the rule every singleton does
  ([classes_and_files.md](classes_and_files.md#one-instance-per-argument-values)): a singleton constructed
  with different argument values is a different instance. Every class asking for
  `DynamicLibrary("ucrtbase.dll", 'identity', "")` shares one library and one table of symbols; the same file
  asked for with a header, or another file, is a second instance with its own table. It is not a special case.
- **Only what is called is bound.** The symbols a program calls are looked up once, when the library opens, so
  every later call is one indirect call. A symbol named only by code the program never reaches is not looked up
  at all ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)), except in an inspectable build
  (`--development`, `--repl`, `--repl-port`, `--hot-reload`), which keeps everything. A missing library or a
  missing symbol stops the program at that point, naming the file, or the symbol and the Spite function that
  wanted it.
- **A foreign call goes through the attribute or variable holding the library**, so the compiler knows which
  table binds it.

That is the whole run-time cost: opening each library once, on first use, one lookup per called symbol, and one
indirect call per call, with arguments passed as they are ([what crosses](#what-crosses)). There is no marshalling
layer, and a symbol no surviving code calls costs nothing.

## Naming rules

Spite's own names are snake_case and never abbreviated, and C's are neither, so the second argument says how a
Spite name becomes the C one:

| Rule | `set_cursor_position` becomes | For |
|---|---|---|
| `'identity'` | `set_cursor_position` | C libraries named in snake_case, or a name written exactly as C spells it |
| `'windows'` | `SetCursorPos` | Win32: abbreviated by the linter's own table read backwards, then PascalCased |
| `'camel_case'` | `setCursorPosition` | libraries named in camelCase |

With `'identity'` a name may be written exactly as C spells it, as in `kernel.GetFileAttributesA(path)`, which is
what the standard library does where Win32's own abbreviations follow no rule. The words of the linter's table
that Win32 spells out in full (`count`, `buffer`, `window`, `size`, the second table in
[Style](../specs/style.md#naming-and-abbreviations-compile-errors-not-auto-fixed)) are not read backwards, so
`set_cursor_position` is still `SetCursorPos` but `get_tick_count` stays `GetTickCount`.

## What crosses

| Spite | C | Cost |
|---|---|---|
| every number type, `Boolean` | the same C type: a number already is one, and see the widths below | none |
| `String`, as an argument | `const char*` | none: every `String` buffer already ends in a zero |
| `String`, as a result (`_as_text`) | `const char*` | one copy; Spite never frees a buffer C owns |
| an enum value | its integer | none |
| `List<T>` of numbers | its element array, by address; C's writes come back into the list | none |
| a `type` whose attributes are all numbers | a C struct in declaration order, by address; C's writes come back | none |
| a function value, as an argument | a C function pointer for that call ([callbacks](#calling-back-into-spite)) | a thread-local store |
| a class instance | never: its layout is the compiler's business | |

A call returns an `Integer`, and the ordinary right-to-left cast takes it from there. A result that is wider or not an
integer is asked for by suffix: `strlen_as_long(...)` for a 64-bit integer or a pointer, `half_of_as_double(...)`
for a `double`, and `greeting_as_text()` for a `const char*` copied into a `String`. An address is a
`Memory.Address`, a number of 64 bits that casts to and from a `Long`; Spite has no pointer type. Text that C
leaves at an address (a name inside a struct C filled in, or a `_as_long` result that may be 0) is read with
`address.terminated_text()`, which copies up to the terminating zero.

With a header as the third argument, a snake_case read is a constant: `user32.mouseeventf_leftdown` is
`MOUSEEVENTF_LEFTDOWN`, an `Integer`. Under the `'windows'` rule the compiler also checks that a `type` passed as a
struct has the size of the header's struct of the derived name (`PointPair` against `POINT_PAIR`), so a missing
padding field fails the build at the Spite line. A function that wants the struct's size, as `SendInput` does, is
given `PointPair.size`: the bytes of the struct the call passes, a `Long` the compiler knows while it compiles. The
reflection members every value has (`.class`,
`.attributes`, `.functions`, `.memory`) and `DynamicLibrary`'s own members (`file_name`, `handle`) are never read
as foreign symbols ([what a member of a library means](../specs/foreign_libraries.md#what-a-member-of-a-library-means)).

### Argument widths

**With a header**, the call goes through the header's own prototype: the function has to be declared there, the
number of arguments is checked, and each argument is converted to its parameter's type the way C converts one --
an `Integer` literal where C wants a 64-bit `VkDeviceSize`, a `Double` where it wants a `float`. A `Long` handle goes
where C wants a pointer, since Spite has no pointer type; that integer-to-pointer conversion is the one C
complaint the call silences. A C function that returns `void` is called the same way, and its call reads as `0`.

**Without a header**, the compiler knows only the Spite types, so it passes what the platform's calling convention
passes anyway:

| Spite argument | Passed as |
|---|---|
| `Tiny`, `Short`, `Integer`, `Long`, `Boolean`, an enum value | `int64_t`, sign-extended |
| `Byte`, `UnsignedShort`, `UnsignedInteger`, `UnsignedLong` | `uint64_t`, zero-extended |
| `Float` | `float` |
| `Double` | `double` |
| `String`, `List<T>` of numbers, a number-only `type` | an address |

On x64 (Windows and System V) and on 64-bit ARM, every integer argument travels in a 64-bit register or an 8-byte
stack slot, and a C function reads the low bits of the width it declared, so a literal `0` reaches a `uint64_t`
parameter as a clean zero and a `-1` reaches an `int64_t` one as -1. What a missing header cannot fix: a `Double`
where C wants a `float` (write a `Float`), and on macOS for ARM, integer arguments after the eighth, which Apple
passes on the stack at their own width; name the header there.

`conformance/stage6/foreign_library` calls a library of every kind above, built from its own `fixture.c`, with and
without a header.

## Calling back into Spite

Some C functions take a function: `qsort` compares with one, Windows sends a window its messages through one,
XAudio2 tells a voice its buffer ended through a table of them, and Vulkan reports what its validation found
through one. A Spite function reaches C in one of two ways, and in both the compiler writes the small C function C
actually calls (a trampoline), which turns C's arguments into Spite's and calls the function on its owner. No
program writes C, and there is no new syntax.

### For the length of one call

A function value given as an argument of a foreign call is a C function pointer for that call:

```gdscript title=sorted_by_c/sorted_by_c.spite entry system=windows
var console = Console()
var c_runtime = DynamicLibrary("ucrtbase.dll", 'identity', "")
var numbers = TypedMemory<Integer>()

func SortedByC() {
    var values = [5, 3, 9, 1]
    var values_count = values.count()
    c_runtime.qsort(values, values_count, 4, ascending)
    var ascending_values = values.join(",")
    var order = Order(true)
    var comparing = ForeignCallback(order.compare, 'context_first')
    values_count = values.count()
    c_runtime.qsort_s(values, values_count, 4, comparing.address, comparing.context)
    var descending_values = values.join(",")
    console.print(ascending_values, descending_values)
}

func ascending(left: Memory.Address, right: Memory.Address): Integer {
    return numbers.read_value(left, 0) - numbers.read_value(right, 0)
}
```
```gdscript title=sorted_by_c/order.spite
var numbers = TypedMemory<Integer>()
var descending = false

func Order(starting_descending: Boolean) {
    descending = starting_descending
}

func compare(left: Memory.Address, right: Memory.Address): Integer {
    var difference = numbers.read_value(left, 0) - numbers.read_value(right, 0)
    if descending {
        return -difference
    }
    return difference
}
```
```output
1,3,5,9 9,5,3,1
```

`qsort` calls `ascending` while it sorts, and that is the whole contract of the first form: C may call the function
while the foreign call runs, on the thread that made the call, as often as it likes. Any function value works, bound
to any object or held in a variable, and the call keeps it, and so its owner, alive until it returns. A C
library that keeps the pointer and calls it later, or calls it from a thread of its own, stops the program with a
message naming the function and the line that handed it over; that is what the second form is for.

### For as long as the program keeps it: `ForeignCallback`

`ForeignCallback(function, where_context)` hands C a function it keeps: `.address` is the C function pointer, and
it stays valid for as long as the program keeps the `ForeignCallback`. The function is named directly
(`order.compare`, or `compare` inside the class), and the second argument, a literal the compiler reads like a
library's naming rule, says how C gets the owner back to the trampoline:

| `where_context` | C calls | The owner is found | For |
|---|---|---|---|
| `'context_first'` | `function(context, ...)` | from `.context`, which the program gives C as its user data | `qsort_s`, a thread's start |
| `'context_last'` | `function(..., context)` | the same | `EnumWindows`, Vulkan's debug messenger |
| `'no_context'` | `function(...)` | because the function belongs to a singleton | a window procedure, a table of functions |

The Spite function never takes the context itself: `order.compare` above takes the two addresses `qsort_s`
compares, and the trampoline turns the context it is handed first back into `order`.

**With no context, the function belongs to a singleton.** A window procedure gets the window's handle and the
message, and nothing that says which Spite object wanted it, so the compiler only accepts a function of a
singleton there, which has one owner by construction, and the singleton tells its windows apart by the handle C
passes, the way Win32 programs use `GWLP_USERDATA`:

```gdscript
singleton

var user32 = DynamicLibrary("user32.dll", 'identity', "")
var windows = Dictionary<Long, Window>()
var procedure: ForeignCallback? = null

func register(): Long {
    var handling = ForeignCallback(handle_message, 'no_context')
    procedure = handling
    return handling.address
}

func handle_message(window: Long, message: UnsignedInteger, word: UnsignedLong, long_word: Long): Long {
    var found = windows[window]
    if found {
        return found.handle_message(message, word, long_word)
    }
    return user32.DefWindowProcW_as_long(window, message, word, long_word)
}
```

`register()`'s address goes into the `procedure` attribute of the `type` passed to `RegisterClassExW`, and every
message of every window (resize, fullscreen, IME composition, the messages Windows sends rather than posts)
arrives at `handle_message`. There is one `ForeignCallback` per such function at a time: making a second while the
first is alive stops the program, since both would be the same C function.

**A table of functions is memory the program fills.** A COM-style interface such as XAudio2's
`IXAudio2VoiceCallback` is an object whose first word points at a table of functions, each called with the object
first. The program allocates both, writes each `ForeignCallback`'s address into the table in the interface's order,
and hands C the object; the object C passes back tells the singleton which voice it was:

```gdscript
singleton

var console = Console()
var heap = Memory.Heap()
var words = TypedMemory<Long>()
var started: ForeignCallback? = null
var ended: ForeignCallback? = null
var table = heap.allocate(16)
var listener = heap.allocate(8)
var buffers_ended = 0

func Mixer() {
    var starting = ForeignCallback(voice_started, 'no_context')
    var ending = ForeignCallback(voice_ended, 'no_context')
    words.write_value(table, 0, starting.address)
    words.write_value(table, 1, ending.address)
    words.write_value(listener, 0, table)
    started = starting
    ended = ending
}

func voice_started(listening: Memory.Address, voice: Integer) {
    console.print("voice", voice, "started", listening == listener)
}

func voice_ended(_listening: Memory.Address, voice: Integer, buffer: Long) {
    buffers_ended = buffers_ended + 1
    console.print("voice", voice, "ended with buffer", buffer)
}

func drop() {
    heap.free(listener)
    heap.free(table)
}
```

`IXAudio2VoiceCallback` has seven entries, so its table is 56 bytes; an interface that derives from `IUnknown`
starts with `QueryInterface`, `AddRef` and `Release`, which are three more functions of the same singleton.
`conformance/stage6/foreign_callbacks` builds a two-entry table that its fixture calls from a thread of its own.

**Threads.** XAudio2 calls from its own thread, so a function handed over with `ForeignCallback` may run on a
thread C made, and it is held to what a `Parallel`'s work is held to ([concurrency.md](concurrency.md)): a function
of an ordinary object may reach only the attributes of its own that hold values, its locals and singletons, and a
singleton it reaches (the owner of a `'no_context'` function included) is made safe for threads by the compiler, a
lock or atomics as its state needs. A program that makes a `ForeignCallback` counts references
atomically. A callback never runs the scheduler: a wait on a thread that is not the program's own makes the plain
blocking call. A function value passed for one call is not checked, since it runs on the calling thread.

**Lifetime.** A `ForeignCallback` keeps its function and the function's owner alive, and C may call it for exactly
as long as the program keeps it. Keep it in the object that owns what C was given (the window class, the voice,
the debug messenger) and tell C to stop in that object's `drop()`: attributes are released after `drop()` runs,
so C has let go before the `ForeignCallback` goes. An attribute holding a `ForeignCallback` counts as read, since
keeping it is what it is for. After one is dropped, a late call from C stops the program with a message naming the
function and the line that made the `ForeignCallback`, so the owner's `drop()` must still tell C to stop first: the
check only makes forgetting loud.

**C types.** A trampoline's C signature is the Spite function's, one parameter at a time, each at its width
([what crosses](#what-crosses)): numbers as their C types, a `Memory.Address` as a 64-bit address, and a
`Boolean` as a 32-bit integer (`BOOL`, `VkBool32`, `int`; C's one-byte `bool` is taken as a `Byte`). A callback
takes only these and returns one of them or nothing, and text or a struct C passes arrives as its
`Memory.Address`, read with `terminated_text()` or `TypedMemory<T>`. The header does not check the trampoline
against C's declaration (C cannot name a parameter's type), so the widths are the program's to get right, the
same bargain a struct's layout is. On every 64-bit target C has one calling convention, so `CALLBACK`, `WINAPI`,
`VKAPI_PTR` and `STDMETHODCALLTYPE` need nothing.

**Cost.** A trampoline for one call is a thread-local load and an indirect call; one with a context, an indirect
call; one with no context, an atomic load and a direct call. Making a `ForeignCallback` is one allocation beside the
function value. A program that passes no function to C and makes no `ForeignCallback` carries none of it: no
trampoline, no class, no atomic counts.

## Each operating system reopens what it changes

There is no platform layer. `library/` holds what every system shares, and `library/windows/`, `library/linux/`
and `library/mac/` hold only what differs: each reopens the classes it changes, with a `DynamicLibrary` of its own
naming the system's real file. `library/windows/program.spite` is all of `Program`'s Windows side:

```gdscript
var library = DynamicLibrary("ucrtbase.dll", 'identity', "")
var kernel = DynamicLibrary("kernel32.dll", 'identity', "")

func sleep(milliseconds: Integer) {
    kernel.Sleep(milliseconds)
}

func exit_process(code: Integer) {
    library.exit(code)
}

func environment_address(name: String): Long {
    return library.getenv_as_long(name)
}
```

and `library/program.spite` calls `exit_process` and `environment_address` without declaring them, because
every system's folder defines them with the same signature. The launcher loads `library/` and then exactly one of
these folders, named by `build.target_operating_system` ([programs.md](programs.md#how-a-program-is-loaded)), so
their functions replace or add members by the ordinary reopening rule ([packages.md](packages.md#monkey-patching-mods)).
`--final-classes` prints each class as it came out, its system's functions included.

## Libraries written in other languages

`DynamicLibrary` does not care what a library was written in. It calls functions through the C calling convention
(the C ABI), and every systems language can export functions that way, so a mature library in C, C++, Rust, Zig or
Go is one binding away. You do not need a big ecosystem on day one: you need the library's dynamic library file
and a small Spite class around it.

| The library is written in | It exports a function to Spite with | It is built as |
|---|---|---|
| C | nothing: every function the library exports is already callable | a `.dll`, `.so` or `.dylib` |
| C++ | `extern "C"` on each function, a free function taking the object's address for each method | a shared library |
| Rust | `#[no_mangle] pub extern "C" fn` | `crate-type = ["cdylib"]` |
| Zig | `export fn` | a dynamic library (`zig build-lib -dynamic`) |
| Go | `//export` above the function, with `import "C"` | `go build -buildmode=c-shared` |

What a library hands across is what [crosses](#what-crosses): numbers, `Boolean`, text as a zero-terminated
`const char*`, arrays of numbers and number-only structs by address, and a handle as a `Long`. A Rust function
that sums an array, exported from a library built as `libmeasure.so`:

```rust
#[no_mangle]
pub extern "C" fn sum_of(values: *const i32, count: i64) -> i64 {
    let values = unsafe { std::slice::from_raw_parts(values, count as usize) };
    values.iter().map(|value| *value as i64).sum()
}
```

is called from Spite exactly as a C function is, its name already snake_case:

```gdscript
var measure = DynamicLibrary("libmeasure.so", 'identity', "")

func total(values: List<Integer>): Long {
    var values_count = values.count()
    return measure.sum_of_as_long(values, values_count)
}
```

A C++ class, a Rust struct or a Go value never crosses as itself. The library hands out an opaque handle (a
pointer, which Spite holds as a `Long`) and a function per operation that takes it, `extern "C"` in C++,
`extern "C" fn` taking `*mut Thing` in Rust, and a `cgo.Handle` in Go. That is the shape every C library already has,
and it is the shape the binding below turns back into an object.

## A binding speaks Spite

You should not have to think about C, or any other language, to use a library. So a binding wraps the library in a
class of its own, and nothing but that class touches the `DynamicLibrary`: only Spite shapes come out of it.

- **Spite names.** The functions the rest of the program calls are full words in snake_case (`open_database`, not
  `db_open_v2`), whatever the library calls them.
- **A status is a Spite enum.** A C function that answers a C enum (or a status code) answers, through the binding,
  an enum the binding writes in Spite, with Spite names, each value given its C number with `=` (`'busy' = 5`,
  [values_and_types.md](values_and_types.md#numbering-an-enums-values)). A number the enum does
  not list crashes at the boundary, so a library that grows a new value is caught where it enters, never carried
  along as a wrong answer.
- **Absence is a `T?`.** A null handle or a "not found" code becomes `null` through an `assert` in the binding, never
  a `0` the caller has to know to compare with.
- **A handle is an object.** The binding's class holds the handle, and its `drop()` gives it back to the library, so
  the library's memory is released when the last reference goes, like every other object's.
- **Text is a `String`**, read with `_as_text` or `terminated_text()`, never an address the caller has to read.

A binding for a library that compresses blocks, whose C side answers `0` for success, `1` when the output is too
small and `2` when the input is corrupt:

```gdscript
enum Packing {
    'packed'
    'output_too_small'
    'input_corrupt'
}

var library = DynamicLibrary("libpack.so", 'identity', "")
var handle: Long = 0

func Packer(level: Integer) {
    handle = library.pack_create_as_long(level)
    crash handle != 0
}

func pack(input: List<Byte>, output: List<Byte>): Packing {
    var input_count = input.count()
    var output_count = output.count()
    var code = library.pack_block(handle, input, input_count, output, output_count)
    return packing_of(code)
}

func packing_of(code: Integer): Packing {
    switch code {
        0: return 'packed'
        1: return 'output_too_small'
        2: return 'input_corrupt'
        _: crash
    }
}

func drop() {
    library.pack_destroy(handle)
}
```

The program sees a `Packer` with a `pack` that answers a `Packing`, which a `switch` reads with every value
written; `pack_create`, the numbers `0` to `2` and the handle never leave the file. Write a binding when a mature
library already does the job (compression, a database, a codec, a physics engine, a graphics API) and rewriting
it would take months; write it in Spite when it is small, or when the work is what your program is about.

## What other languages cannot hand over

Some things have no shape in the C ABI, so no binding can carry them as they are:

- **Exceptions.** A C++ exception or a Rust panic that unwinds out of an exported function is undefined behaviour
  in C, and Spite has no exceptions to receive it. The library must catch it on its side and answer a status.
- **Generics and templates.** Only a concrete function has an address: export one instantiation per type.
- **A struct returned by value**, a C union past its first member, variadic functions (`printf`) and macros that
  look like functions. Wrap them in a small exported function on the library's side.
- **Classes, traits, interfaces and closures as themselves.** They cross as a handle and functions, as above, and a
  function the library calls back is a [`ForeignCallback`](#calling-back-into-spite).
- **Another runtime's memory.** Spite never frees what the library allocated: memory the library hands out goes
  back through the library's own function, which is what a binding's `drop()` is for. A Go library brings Go's
  runtime and collector into the process with it, and a C# or Java library needs its runtime hosted, which no
  binding does for you.
- **Static libraries.** A library is loaded from its `.dll`, `.so` or `.dylib` while the program runs; a `.a` or
  `.lib` is not linked into the executable.

## When a foreign call faults

C can do what Spite cannot: read through a null pointer, write past an array, run out of stack. When it does, the
program still says so: it prints a `spite.fault` line naming the library the fault is in
(`at=fixture.dll+0x1029`), the last foreign function the thread called and the line of Spite that called it
(`foreign=read_integer_at	library=...	from=native_fault_foreign.spite:13`), and then the Spite functions on the
stack. Each foreign call names itself before it goes in, which costs one store; how to read the whole report is in
[failure.md](failure.md#what-a-native-fault-reports).

## What the compiler supplies

The members whose bodies the compiler supplies, such as opening a library, finding a symbol and `Memory.Heap`'s
allocation, are declared in Spite as a `func` with a signature and no body, which the compiler merges into the class
as its own reopening ([compiler.md](compiler.md#inspect-merged-classes)). `library/dynamic_library.spite` holds the
rest: the `singleton` line, `file_name` and `handle`, the constructor and `drop()`. Nothing the compiler supplies stays
hidden or ties Spite to C: these bodies are Spite over the few operations each backend lowers.

---

Next: [Targets, the web and isomorphic classes](targets.md), what a program can be built for.
