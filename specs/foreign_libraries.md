# Foreign libraries and operating systems

The specification of [Foreign libraries and operating systems](../docs/foreign_libraries.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Foreign libraries

`DynamicLibrary(file, naming, header)` has the three naming rules. It calls any function the library exports; its
import table holds exactly the called symbols that survive tree shaking, resolved once when the library opens; it reads
constants from a header; every row of [What crosses](../docs/foreign_libraries.md#what-crosses) works, structs and lists included, with C's
writes coming back; arguments keep their widths; and a missing library, or a missing symbol, stops the program with a
message naming the file, or the symbol and the Spite function that wanted it (`conformance/stage6/foreign_library`,
`diagnostics/foreign_library_mistakes`, `diagnostics/foreign_call_mistakes`).

A status a foreign function answers is handled while compiling. A C function that returns a C enum answers a
Spite enum that the binding writes in Spite, with Spite names, each value given its C number with `=`; no enum is ever
made with C's names. A C value the Spite enum does not list crashes at the boundary. The result must be used,
cannot be compared with a number, and cannot be the condition of a `crash` or `assert`; a `switch` over it follows
the ordinary rule, every value a written line, with no `_:`. So `crash result == 0` does not compile for a call that
answers a status. A header is not needed for any of this; the compiler may read one to check that a binding's
numbers match C's.

**A binding wraps the library in Spite shapes.** Whatever can be written in Spite is written in Spite, and a
foreign library is wrapped so that only Spite shapes reach the rest of the program: no C type, name or convention
passes the binding ([a binding speaks Spite](../docs/foreign_libraries.md#a-binding-speaks-spite)). Libraries in other languages are reached
through the C ABI they export, with the limits listed in
[what other languages cannot hand over](../docs/foreign_libraries.md#what-other-languages-cannot-hand-over).

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

### What a member of a library means

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

### The class

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

### Structs

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
  [below](#what-crosses)).

### What crosses

[The table on the page](../docs/foreign_libraries.md#what-crosses) is the rule, costs included: a number type is the C type of its width
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
as themselves ([the table on the page](../docs/foreign_libraries.md#argument-widths)). A `Double` where C wants `float`, and Apple ARM's stack
arguments past the eighth, which it packs at their own width, need the header.

**Return types.** A foreign call returns a 32-bit `Integer`, and [Variables and values](values_and_types.md#variables-and-values)'s
right-to-left casting takes it from there. Anything wider, or not an integer, is asked for by suffix, since a
`double` comes back in a different register and nothing in the call says which: `_as_long` for a 64-bit integer
or a pointer, `_as_double` for a `double`, `_as_text` for a `const char*` copied into a `String`. The suffix is
[Symbol codegen](metaprogramming.md#templates)'s ordinary segment template, not a new mechanism.

### Lifetime, and what gets linked

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
  ([failure.md](failure.md#what-a-native-fault-reports)). One store per
  call measured as nothing against the call itself ([optimizations.md](../docs/optimizations.md#the-fault-handler-is-in-every-program)).
- `drop()` closes the library at exit, when singletons are destroyed in reverse creation order (a singleton
  is not reference counted).

### Callbacks

What [Calling back into Spite](../docs/foreign_libraries.md#calling-back-into-spite) teaches, in full:

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
  owner), and sets `address` to the trampoline and, for `'context_first'` and `'context_last'`, `context` to a
  ticket: the place of the function value in a table of the living context callbacks, with a count of how many
  `ForeignCallback`s have held that place, which the trampoline turns back into the function and its owner. A
  context trampoline is written once per construction site, so it can name the function. The function
  is named directly (`owner.function`, or a function of the class itself), otherwise `ForeignCallback(...) takes
  a function named directly, like 'mixer.voice_ended', and where C passes it back its context: 'no_context',
  'context_first' or 'context_last'`; any other literal is `'somewhere' is not where C passes a callback its
  context: write 'no_context', 'context_first' or 'context_last'` (`diagnostics/foreign_callback_mistakes`).
- **What crosses into a callback**: each parameter and the result are a number, a `Boolean` or a
  `Memory.Address`, at the widths of [What crosses](#what-crosses); a `Boolean` is a 32-bit integer on the C side,
  read as `!= 0`. Anything else is `'ForeignCallback(named, 'context_last')' is called by C, which passes numbers
  and addresses: 'name' is a String, so take it as a number, a Boolean or a Memory.Address (text C passes is read
  with 'terminated_text()')`, or for the result `... which takes back a number, a Boolean or nothing: it returns a
  String`. A function value passed for one call names its parameters `'argument 1'` and on.
- **Lifetime**: the `ForeignCallback` keeps the function value; releasing the last reference runs its
  `drop()`. An attribute whose type is `ForeignCallback` or `ForeignCallback?` is never reported as unread, since
  keeping it is its use. The owner's `drop()` unregisters from C, which runs before its attributes are released.
  `drop()` empties the table's place (one atomic store), and a later `ForeignCallback` may take it with a new count,
  so a context C kept from a dropped one never reaches the new one's function: a late call stops the program
  (`spite: C called 'Ticker.ticked' (file.spite:14) after its ForeignCallback was dropped: keep the
  ForeignCallback for as long as C may call it`, `conformance/stage6/foreign_callback_context_dropped`). The table
  grows by blocks of 4096 places only when every place is taken, and a program may hold 16 777 216 context
  callbacks alive at once; one more stops the program, saying so.
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
  call already makes the plain blocking call off the program's thread ([concurrency.md](../docs/concurrency.md)).
- **Cost**: a call-site trampoline is one thread-local load and an indirect call; a context one, a table
  read, an atomic load to check the ticket and an indirect call, and making one takes a short lock; a
  `'no_context'` one, an atomic load and a direct call. A program that passes no function to C
  and makes no `ForeignCallback` compiles to C with none of it: no trampoline, no `ForeignCallback` class, no
  `SPITE_THREADS`.
- `conformance/stage6/foreign_callbacks`: a synchronous callback with and without a header, a window-procedure
  shape kept with no context, one with the context last, one with the context first called three times from a
  thread the fixture makes, and a two-entry table of functions called from another thread, memory balanced.

### What foreign calls do not cover

- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.

---

Next: [Compiler command line](compiler.md).
