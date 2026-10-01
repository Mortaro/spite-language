# Foreign libraries and operating systems

A native library is a class, not a keyword. `DynamicLibrary(file, naming, header)` opens a `.dll`, `.so` or
`.dylib`, and every function it exports is called as if it were a member: there is no `extern` declaration, no
binding file and no generated shim. The standard library reaches the operating system this way: `File`,
`Directory`, `Process`, `Console` and the rest are Spite over the C runtime, reached through `DynamicLibrary`, so
what a program can do with it, the library already does.

## Calling a function

```gdscript title=foreign_call/foreign_call.spite entry
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
what the standard library does where Win32's own abbreviations follow no rule.

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
as foreign symbols ([what a member of a library means](#what-a-member-of-a-library-means)).

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

```gdscript title=sorted_by_c/sorted_by_c.spite entry
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
var windows = Dictionary<Window>()
var procedure: ForeignCallback? = null

func register(): Long {
    var handling = ForeignCallback(handle_message, 'no_context')
    procedure = handling
    return handling.address
}

func handle_message(window: Long, message: UnsignedInteger, word: UnsignedLong, long_word: Long): Long {
    var found = windows.get(window)
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
keeping it is what it is for. After a `'no_context'` one is dropped, a late call from C stops the program with a
message naming the function; a context one cannot be checked without a registry the program would carry, so it is
the owner's `drop()` that must unregister first.

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

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and how each rule is compiled. Where the teaching above and these rules disagree, the
rules win.

### Foreign libraries

`DynamicLibrary(file, naming, header)` has the three naming rules. It calls any function the library exports; its
import table holds exactly the called symbols that survive tree shaking, resolved once when the library opens; it reads
constants from a header; every row of [What crosses](#what-crosses) works, structs and lists included, with C's
writes coming back; arguments keep their widths; and a missing library, or a missing symbol, stops the program with a
message naming the file, or the symbol and the Spite function that wanted it (`conformance/stage6/foreign_library`,
`diagnostics/foreign_library_mistakes`, `diagnostics/foreign_call_mistakes`).

A status a foreign function answers is handled while compiling. With a header, a function whose C return type is a C
`enum` answers a Spite enum made from that enum's values; the result must be used, cannot be compared with a number,
and cannot be the condition of a `crash` or `assert`; it is read with a `switch` naming every value, with no `_:`. A
binding writes that switch once and answers its own small enum. So `crash result == 0` does not compile for a call
that answers a status.

The compiler also follows these rules:

- One library per distinct file, naming rule and header: the rule every singleton follows, one instance per literal
  argument list, opened on first use and closed at exit. The same file with another header is a second instance, with
  its own import table, so a constant read through the one that names the header always finds it, whichever
  construction the program reaches first (`conformance/stage6/foreign_library`).
- The arguments are literals: otherwise `DynamicLibrary(...) takes three literals the compiler reads while it
  compiles: the file, the naming rule ('identity', 'windows' or 'camel_case') and the header`. The naming rule is a
  symbol literal: otherwise `'shouting' is not a naming rule: a library's functions are named 'identity',
  'windows' or 'camel_case'`.
- The file is named exactly as it is on disk (a wrapper for another platform names that platform's file):
  otherwise `'fixture' has no extension: name the library file exactly as it is on disk, like "user32.dll" or
  \"libc.so.6\" (a wrapper for another platform names that platform's file)`.
- `_as_long` beside `_as_double` and `_as_text`, since a plain call returns a 32-bit `Integer` and a handle or
  pointer needs 64.
- A foreign function is called only through the attribute or variable that holds its `DynamicLibrary(...)`:
  otherwise `a foreign function is called through the attribute or variable that holds its DynamicLibrary(...), so
  the compiler knows which library to bind it in`.
- A header path that exists relative to the working directory is included as a file, anything else as a system
  header.
- Under the `'windows'` rule with a header, `_Static_assert` checks a struct's size against the header's struct,
  named `PointPair` -> `POINT_PAIR`, and only there, since other headers do not name structs that way, and a missing
  name would be a C error.
- `--final-classes` writes no resolved-name comments.

**A native library is a class, not a keyword.** There is no `external`
keyword, no per-symbol binding string, no generated-binding step and no hand-written C shim. `DynamicLibrary`
is an ordinary standard library class, and its Symbol codegen ([Symbol codegen](metaprogramming.md#templates)) resolves every foreign function,
constant and type at compile time.

```gdscript mouse.spite
var user32 = DynamicLibrary("user32.dll", 'windows', "windows.h")

func move_to(x_position: Integer, y_position: Integer): Boolean {
    return user32.set_cursor_position(x_position, y_position) != 0
}
```

`set_cursor_position` becomes `SetCursorPos`. Nothing in that file is spelled the way C spells it.

#### What a member of a library means

Spite already forces PascalCase on every type and snake_case on everything else, and the linter ([Style](style.md#style))
enforces it, so the language's own naming rule is what tells the compiler which kind of foreign symbol is being
asked for. No annotation is needed, and none is accepted.

| Written | Is | Emits |
|---|---|---|
| `user32.send_input(...)` | a call | `SendInput(...)` |
| `user32.input_mouse` | a read, snake_case | the constant `INPUT_MOUSE` |
| `user32.Input` | a read, PascalCase | the C type `INPUT` (reflection only, see Structs below) |

A constant or a type is only available when the constructor was given a header; without one a library exposes
functions and nothing else, and a constant read is `'fixture_answer' is a constant, and a constant comes from the
library's header: name it as DynamicLibrary's third argument`.
The reflection members every value has (`.class`, `.attributes`, `.functions`, `.memory`) are never foreign
symbols, so a generic class instantiated over `DynamicLibrary` can still ask `current.class.name`; neither are
`DynamicLibrary`'s own members (`file_name`, `handle` and its functions), so a `List<DynamicLibrary>`'s member
templates (`find_by_file_name`, `sort_by_handle`) read the attribute
(`conformance/stage6/settings_and_libraries_reflected`).

#### The class

`DynamicLibrary(file_name, naming, header)`. Every argument must be a literal, because the compiler reads them
during codegen. The class is:

```gdscript dynamic_library.spite
enum Naming {
    'identity'
    'windows'
    'camel_case'
}

var _file_name = ""
var _naming = 'identity'
var _handle = 0

func DynamicLibrary(file_name: String, naming: Naming, header: String) {
    _file_name = file_name
    _naming = naming
    _handle = _open(file_name)
    assert _handle
}

func symbol_name(symbol: Symbol): String {
    if symbol.kind == 'constant' {
        return symbol.name.upper_case()
    }
    if _naming == 'windows' {
        return symbol.name.abbreviated().pascal_case()
    }
    if _naming == 'camel_case' {
        return symbol.name.camel_case()
    }
    return symbol.name
}

func missing_function(function: Symbol, arguments: Arguments): Integer {
    var name = symbol_name(function)
    return _call(_handle, name, arguments)
}

func missing_attribute(attribute: Symbol): attribute.class {
    var name = symbol_name(attribute)
    return _resolve(name)
}

func drop() {
    _close(_handle)
}
```

In this class `_open`/`_call`/`_resolve`/`_close` are the only operations below Spite; everything above them is
ordinary Spite, and `--final-classes` prints the whole class with every generated binding, as it does for all
reflection. Loading a library is plain Spite calling the operating system, with no compiler-supplied C at all.

- **`missing_function` and `missing_attribute` are the only two new names in the entire foreign function
  interface.** They are Ruby's `method_missing` resolved at compile time: [Symbol codegen](metaprogramming.md#templates)'s rule ("a parameter of class
  `Symbol` whose name is a segment of its function's name turns the function into codegen for every name that
  fits") taken to its limit, where the segment is the whole name. They get reserved names rather than falling out
  of the segment rule so that a class opts in by defining them, instead of silently swallowing every misspelled
  call. [Symbol codegen](metaprogramming.md#templates)'s existing precedence is unchanged: a function with the exact name always wins.
- `missing_attribute` returning `attribute.class` is the same form as [Symbol codegen](metaprogramming.md#templates)'s `get_attribute(attribute:
  Symbol): attribute.class`: the return type is whatever that symbol turns out to be: an `Integer` for a `#define`,
  a `Spite.Class` for a type name.
- **The naming rule is a function, not a table.** Three rules cover a whole library, and binding one more symbol
  is one more call site, never a second line. `'windows'` runs `abbreviated()` before `pascal_case()`, so
  `set_cursor_position` -> `set_cursor_pos` -> `SetCursorPos`: the linter's abbreviation table ([Style](style.md#style), "kept
  in one place so it is easy to extend") read backwards *is* the Win32 name generator. The table that makes C's
  spellings illegal in Spite is the table that translates back into them.
- **Constant reads are the C spelling, lowercased**, with no prefix rule: `user32.mouseeventf_leftdown`. This is
  deliberate: C constant prefixes (`MOUSEEVENTF_`, `INPUT_`, `WM_`, `SW_`) follow no rule any function could infer.
  Give them Spite names once, in a `var`, and the ugly spelling stops there.

#### Structs

A `type` ([Types](values_and_types.md#types)) whose fields are all scalars is a C struct when it reaches a foreign call: passed by
address, never as a Spite heap object. There is no annotation: the value's type comes from the `var`'s own
annotation or from the parameter it feeds, exactly like any other object literal matched by shape.

```gdscript mouse.spite
type Input {
    kind: UnsignedInteger
    padding: UnsignedInteger
    x_movement: Integer
    y_movement: Integer
    wheel_amount: UnsignedInteger
    event: UnsignedInteger
    time_stamp: UnsignedInteger
    extra_information: UnsignedLong
}

func _send(event: UnsignedInteger, wheel_amount: UnsignedInteger) {
    var input: Input = {
        kind: _mouse
        padding: 0
        x_movement: 0
        y_movement: 0
        wheel_amount: wheel_amount
        event: event
        time_stamp: 0
        extra_information: 0
    }
    user32.send_input(1, input, Input.size)
}
```

- **Spite lays the struct out itself**, so the layout is yours to get right, including padding a C union leaves
  behind (the `padding` field above is there because `INPUT` is a `DWORD` followed by a union that aligns to 8).
  This is the same bargain `#[repr(C)]`, `extern struct` and `ctypes` make, and it is the price of never naming
  the C type in your own source.
- `Input.size` is `sizeof` of the struct the compiler emitted, ordinary reflection on a Spite type, with no
  library in the expression. It is a `Long` the compiler writes as a C `sizeof`, so it costs nothing at run time,
  and it is the same struct a foreign call passes, padding included (`conformance/stage6/type_size`: a `Byte`, a
  `Long` and a `Boolean` are 24 bytes). A `type` with anything but numbers in it is no C struct, so its `.size`
  is an error: `'Labelled.size' is the size of the C struct a type is at a foreign call, and only a type whose
  attributes are all numbers is one` (`diagnostics/type_size_mistakes`).
- **The compiler checks your work for free.** When the library was given a header and uses the `'windows'` rule,
  it derives the C name (`Input` -> `INPUT`) and emits
  `_Static_assert(sizeof(INPUT) == sizeof(Mouse_Input), "mouse.spite:12 type Input does not match INPUT");`.
  A forgotten padding field fails the build at the Spite line, naming both types. **The derived C name is used to
  verify, never to generate.** Size is checked; field order is not, because C's own field names (`dwFlags`, `dx`,
  `mouseData`) are unmappable by any rule, and a table for them is exactly what this design refuses to have.
- A `type` containing a `String`, `List<T>`, `Dictionary<T>` or class field is an error at a foreign call: the
  all-scalars condition is enforced, not assumed. The error names the type (`a Labelled cannot cross into C: ...`,
  [below](#what-crosses-1)).

#### What crosses

[The table above](#what-crosses) is the rule, costs included: a number type is the C type of its width
([Variables and values](values_and_types.md#variables-and-values)), text, lists of numbers and
number-only `type`s cross by address at no cost, a text result is copied, and a class instance never crosses, since
its layout (header, reference count) is the compiler's business. Anything else is an error at the call:
`a Console cannot cross into C: a foreign function takes numbers, Boolean, enum values, text and a type whose
attributes are all numbers` (`diagnostics/foreign_call_mistakes`); a `type` with a `String` field and a
`List<String>` are rejected the same way, naming the type rather than the field.

**Argument widths.** With a header, a call goes through the
header's prototype (`__typeof__(&Symbol)`), so C checks the argument count and converts each argument to its
parameter's type; integer-to-pointer conversion is allowed, because a handle is a `Long`, and a `void` function's
call reads as `0`. Without one, integer-like arguments widen to `int64_t` or `uint64_t` and `Float`/`Double` pass
as themselves ([the table above](#argument-widths)). A `Double` where C wants `float`, and Apple ARM's stack
arguments past the eighth, which it packs at their own width, need the header.

**Return types.** A foreign call returns a 32-bit `Integer`, and [Variables and values](values_and_types.md#variables-and-values)'s
right-to-left casting takes it from there. Anything wider, or not an integer, is asked for by suffix, since a
`double` comes back in a different register and nothing in the call says which: `_as_long` for a 64-bit integer
or a pointer, `_as_double` for a `double`, `_as_text` for a `const char*` copied into a `String`. The suffix is
[Symbol codegen](metaprogramming.md#templates)'s ordinary segment template, not a new mechanism.

#### Lifetime, and what gets linked

- The library handle is a plain `Long`. **There is no `Pointer` type in Spite**, and this design does not add one.
- `DynamicLibrary` is a **singleton** (its file's `singleton` line), keyed by its file, naming rule and header: every class
  asking for `DynamicLibrary("user32.dll", 'windows', ...)` shares one object and one import table;
  `"gdi32.dll"` is a second one.
- The import table is the set of call sites that survived tree shaking: two symbols are resolved because two were
  called. They are resolved once, in the constructor, so every later call is one indirect call, never a lookup
  per call.
- A missing library or a missing symbol aborts at construction, naming the file, the symbol and the Spite function
  that wanted it. This is a program-stopping error and is not reported any other way.
- Each call first stores, in a thread-local, a pointer to a fixed text naming the C function, the library file and
  the calling line, in every build; a native fault's report prints it as `foreign=`, `library=` and `from=`
  ([failure.md](failure.md#what-a-native-fault-reports-1)). One store per
  call measured as nothing against the call itself ([optimizations.md](optimizations.md#the-fault-handler-is-in-every-program)).
- `drop()` closes the library at exit, when singletons are destroyed in reverse creation order (a singleton
  is not reference counted).

#### Callbacks

What [Calling back into Spite](#calling-back-into-spite) teaches, in full:

- **A function value as an argument of a foreign call** crosses as a C function pointer valid for that
  call. The compiler writes one trampoline per call site with the C signature of the value's own
  (`Spite.Function<...>`) signature, and a thread-local slot beside it: the call puts the value in the slot
  (keeping the one it held, so a call that reaches the same site again nests), passes the trampoline, and puts the
  old value back when C returns. The trampoline calls the value through its owner, as `call_function` does.
  With a header, the trampoline passes through the header's prototype as a `void*`, which C converts to the
  declared function pointer. A call with the slot empty (after the foreign call returned, or on another thread)
  stops the program: `spite: C called 'scaled' (file.spite:13) after the foreign call that handed it over had
  returned, or on another thread: a function C keeps, or calls from a thread of its own, is handed over as a
  ForeignCallback the program keeps for as long as C may call it`. Nothing about threads is checked or changed,
  since it only runs on the calling thread.
- **`ForeignCallback(function, where_context)`** (`library/foreign_callback.spite`) is an ordinary class
  whose constructor the compiler finishes at each construction site: it keeps the function value (and so its
  owner), and sets `address` to the trampoline and, for `'context_first'` and `'context_last'`, `context` to the
  function value's own address, which the trampoline turns back into the owner. A context trampoline is written
  once per C signature and context position, and shared by every construction with that signature. The function
  is named directly (`owner.function`, or a function of the class itself), otherwise `ForeignCallback(...) takes
  a function named directly, like 'mixer.voice_ended', and where C passes it back its context: 'no_context',
  'context_first' or 'context_last'`; any other literal is `'somewhere' is not where C passes a callback its
  context: write 'no_context', 'context_first' or 'context_last'` (`diagnostics/foreign_callback_mistakes`).
- **What crosses into a callback**: each parameter and the result are a number, a `Boolean` or a
  `Memory.Address`, at the widths of [What crosses](#what-crosses-1); a `Boolean` is a 32-bit integer on the C side,
  read as `!= 0`. Anything else is `'ForeignCallback(named, 'context_last')' is called by C, which passes numbers
  and addresses: 'name' is a String, so take it as a number, a Boolean or a Memory.Address (text C passes is read
  with 'terminated_text()')`, or for the result `... which takes back a number, a Boolean or nothing: it returns a
  String`. A function value passed for one call names its parameters `'argument 1'` and on.
- **Lifetime**: the `ForeignCallback` keeps the function value; releasing the last reference runs its
  `drop()`. An attribute whose type is `ForeignCallback` or `ForeignCallback?` is never reported as unread, since
  keeping it is its use. A late call on a context trampoline is not detected (it would need a registry every such
  program carries); the owner's `drop()` unregisters from C, which runs before its attributes are
  released.
- **`'no_context'`**: the function must belong to a singleton, otherwise `'ForeignCallback(counted,
  'no_context')' gives C no context to find its 'Counter' by, so 'counted' must belong to a singleton: move it into
  a singleton that tells them apart by what C passes, or hand C the context with 'context_first' or
  'context_last'`. Its trampoline is written once per function (`<Class>_<function>___from_c`) beside one static
  slot holding the singleton, and calls the function directly. Making the `ForeignCallback` fills the slot and its
  `drop()` empties it (one atomic store); making a second for the same function while the first is alive stops the
  program (`spite: 'Mixer.voice_started' (mixer.spite:13) was handed to C by a second ForeignCallback while the
  first is alive: C calls it with no context, so a function has one at a time; keep the first and hand C its
  address`), and a call after the drop stops it too (`spite: C called 'Clicks.clicked' (file.spite:12) after its
  ForeignCallback was dropped: keep the ForeignCallback for as long as C may call it`,
  `conformance/stage6/foreign_callback_dropped`). A table of functions (COM-style) is memory the program allocates
  with `Memory.Heap` and fills with `TypedMemory<Long>`, one `'no_context'` `ForeignCallback` per entry.
- **Threads**: C may call a `ForeignCallback` from any thread, so a program that makes one is compiled as a
  program that makes a `Parallel`: atomic reference counts (`SPITE_THREADS`), and every singleton the handed-over
  function reaches made safe for threads in its cheapest form (a `'no_context'` function's own singleton included,
  whose wrapper takes its lock). A function of an ordinary object is checked with the rule for a `Parallel`'s work,
  through every function of its class it calls: `'ForeignCallback(marked, 'context_first')' may be called by C on a
  thread of its own, so 'marked' may reach only the attributes of its own 'Recorder' that hold values, its locals
  and singletons: 'marks' holds a List<Integer>, which another thread may share. Keep it in a singleton the
  callback calls, which is made safe for threads, or give the callback numbers and text of its own`. The
  handover a `Parallel` allows is not offered here, since C, not the program, decides when the callback runs. A singleton owner is
  not checked that way: its own lock covers its attributes. A callback on another thread never runs the scheduler: every waiting
  call already makes the plain blocking call off the program's thread ([concurrency.md](concurrency.md)).
- **Cost**: a call-site trampoline is one thread-local load and an indirect call; a context one, an
  indirect call; a `'no_context'` one, an atomic load and a direct call. A program that passes no function to C
  and makes no `ForeignCallback` compiles to C with none of it: no trampoline, no `ForeignCallback` class, no
  `SPITE_THREADS`.
- `conformance/stage6/foreign_callbacks`: a synchronous callback with and without a header, a window-procedure
  shape kept with no context, one with the context last, one with the context first called three times from a
  thread the fixture makes, and a two-entry table of functions called from another thread, memory balanced.

#### What foreign calls do not cover

- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.

---

Next: [Targets, the web and isomorphic classes](targets.md), what a program can be built for.
