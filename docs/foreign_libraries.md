# Foreign libraries and operating systems

A native library is a class, not a keyword. `DynamicLibrary(file, naming, header)` opens a `.dll`, `.so` or
`.dylib`, and every function it exports is called as if it were a member: there is no `extern` declaration, no
binding file and no generated shim. The standard library reaches the operating system this way -- `File`,
`Directory`, `Process`, `Console` and the rest are Spite over the C runtime, reached through `DynamicLibrary` --
so what a program can do with it, the library already does.

## Calling a function

```gdscript title=foreign_call/foreign_call.spite entry
var console = Console()
var build = Build()

func ForeignCall() {
    var text_length = measure("spite")
    console.print(text_length)
}

func measure(text: String): Integer {
    if build.target_operating_system == "windows" {
        var c_runtime = DynamicLibrary("ucrtbase.dll", 'identity', "")
        return c_runtime.strlen(text)
    } else if build.target_operating_system == "linux" {
        var c_runtime = DynamicLibrary("libc.so.6", 'identity', "")
        return c_runtime.strlen(text)
    }
    var c_runtime = DynamicLibrary("libSystem.dylib", 'identity', "")
    return c_runtime.strlen(text)
}
```
```output
5
```

`build.target_operating_system` is a constant, so only the branch for the system the program is compiled for
is in it ([programs.md](programs.md#the-operating-system-operating_system-and-target_operating_system)); the
standard library itself does this more neatly, by reopening classes per system ([below](#each-operating-system-reopens-what-it-changes)).

- **The arguments are literals**, because the compiler reads them while compiling. The file is named exactly as
  it is on disk, extension included -- a name without one is an error.
- **One instance per file and naming rule.** `DynamicLibrary` is a singleton keyed by its first two arguments:
  every class asking for `DynamicLibrary("ucrtbase.dll", 'identity', "")` shares one library and one table of
  symbols.
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

With `'identity'` a name may be written exactly as C spells it -- `kernel.GetFileAttributesA(path)` -- which is
what the standard library does where Win32's own abbreviations follow no rule.

## What crosses

| Spite | C | Cost |
|---|---|---|
| every number type, `Boolean` | the same C type -- a number already is one, and see the widths below | none |
| `String`, as an argument | `const char*` | none: every `String` buffer already ends in a zero |
| `String`, as a result (`_as_text`) | `const char*` | one copy; Spite never frees a buffer C owns |
| an enum value | its integer | none |
| `List<T>` of numbers | its element array, by address; C's writes come back into the list | none |
| a `type` whose attributes are all numbers | a C struct in declaration order, by address; C's writes come back | none |
| a class instance | never: its layout is the compiler's business | |

A call returns an `Integer`, and the ordinary right-to-left cast takes it from there. A result that is wider or not an
integer is asked for by suffix: `strlen_as_long(...)` for a 64-bit integer or a pointer, `half_of_as_double(...)`
for a `double`, and `greeting_as_text()` for a `const char*` copied into a `String`. An address is a
`Memory.Address`, a number of 64 bits that casts to and from a `Long`; Spite has no pointer type. Text that C
leaves at an address -- a name inside a struct C filled in, or a `_as_long` result that may be 0 -- is read with
`address.terminated_text()`, which copies up to the terminating zero.

With a header as the third argument, a snake_case read is a constant -- `user32.mouseeventf_leftdown` is
`MOUSEEVENTF_LEFTDOWN`, an `Integer` -- and under the `'windows'` rule the compiler checks that a `type` passed as a
struct has the size of the header's struct of the derived name (`PointPair` against `POINT_PAIR`), so a missing
padding field fails the build at the Spite line. The reflection members every value has (`.class`,
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
passes on the stack at their own width -- name the header there.

`conformance/stage6/foreign_library` calls a library of every kind above, built from its own `fixture.c`, with and
without a header.

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

Only the Windows folder runs today; `check.sh` holds the Linux and macOS folders to compiling, by writing the
compiler out once for each.

## What the compiler supplies

The members whose bodies stay C -- opening a library, finding a symbol, `Memory.Heap`'s allocation -- are
declared in Spite as a `func` with a signature and no body, which the compiler merges into the class as its own
reopening ([compiler.md](compiler.md#inspect-merged-classes)). `library/dynamic_library.spite` holds the rest:
the `singleton` line, `file_name` and `handle`, the constructor and `drop()`. This is a stopgap: D147 and D168
decide that nothing the compiler supplies stays hidden or ties Spite to C, so these bodies are to become Spite over
the few operations each backend lowers ([D178](decisions.md)) -- **not built** for these three.

**Not built yet:** the naming rule and the calls as reopenable Spite (`missing_function` and
`missing_attribute`), a naming rule of your own, `Type.size`, reading a header's types through reflection,
callbacks from C into Spite, and C's variadic functions
([the rules in full](#foreign-libraries--partial)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Foreign libraries  **[partial]**

**Built:** `DynamicLibrary(file, naming, header)` with the three naming rules; calls of any function the library
exports; the import table of exactly the called symbols that survive tree shaking, resolved once when the library
opens; constants read from a header; every row of [What crosses](#what-crosses), structs and lists included, with
C's writes coming back; argument widths; and a program-stopping message naming the file, or the symbol and the
Spite function that wanted it, when either is missing (`conformance/stage6/foreign_library`,
`diagnostics/foreign_library_mistakes`, `diagnostics/foreign_call_mistakes`).

Choices Claude made while building it (proposed, unconfirmed):

- One library per distinct file and naming rule, following D8's "one instance per literal argument list", opened
  on first use and closed at exit. The header is not part of the key: a second `DynamicLibrary` with the same file
  and naming rule but another header shares the first one, header included (a known issue: a constant read
  through the second is then `'mouseeventf_leftdown' is a constant, and a constant comes from the library's
  header: name it as DynamicLibrary's third argument` when the first named no header).
- The arguments are literals: otherwise `DynamicLibrary(...) takes three literals the compiler reads while it
  compiles: the file, the naming rule ('identity', 'windows' or 'camel_case') and the header`. The naming rule is a
  symbol literal (D70): otherwise `'shouting' is not a naming rule: a library's functions are named 'identity',
  'windows' or 'camel_case'`.
- The file is named exactly as it is on disk (D71 -- a wrapper for another platform names that platform's file):
  otherwise `'fixture' has no extension: name the library file exactly as it is on disk, like "user32.dll" or
  "libc.so.6" -- a wrapper for another platform names that platform's file`. `check.sh` meets this by building
  each fixture's C into `fixture.dll` beside its program on every platform.
- `_as_long` beside `_as_double` and `_as_text`, since a plain call returns a 32-bit `Integer` and a handle or
  pointer needs 64.
- A foreign function is called only through the attribute or variable that holds its `DynamicLibrary(...)`:
  otherwise `a foreign function is called through the attribute or variable that holds its DynamicLibrary(...), so
  the compiler knows which library to bind it in`.
- A header path that exists relative to the working directory is included as a file, anything else as a system
  header.
- Under the `'windows'` rule with a header, `_Static_assert` checks a struct's size against the header's struct,
  named `PointPair` -> `POINT_PAIR` -- only there, since other headers do not name structs that way, and a missing
  name would be a C error.
- `--final-classes` writes no resolved-name comments, which D34 would reject (how to show them is open question
  10).

**Not built:** `Type.size`, reading a header's types as Spite reflection, `missing_function`/`missing_attribute`
as reopenable Spite, a user-written naming rule, callbacks, and C's variadic functions.

D4 (decided by Mortaro, 2026-09-19): **a native library is a class, not a keyword.** There is no `external`
keyword, no per-symbol binding string, no generated-binding step and no hand-written C shim. `DynamicLibrary`
is an ordinary standard library class, and its Symbol codegen ([Symbol codegen](metaprogramming.md#symbol-codegen--implemented)) resolves every foreign function,
constant and type at compile time.

```mouse.spite
var user32 = DynamicLibrary("user32.dll", 'windows', "windows.h")

func move_to(x_position: Integer, y_position: Integer): Boolean {
    return user32.set_cursor_position(x_position, y_position) != 0
}
```

`set_cursor_position` becomes `SetCursorPos`. Nothing in that file is spelled the way C spells it.

#### What a member of a library means

Spite already forces PascalCase on every type and snake_case on everything else, and the linter ([Style](style.md#style--implemented))
enforces it, so the language's own naming rule is what tells the compiler which kind of foreign symbol is being
asked for. No annotation is needed, and none is accepted.

| Written | Is | Emits |
|---|---|---|
| `user32.send_input(...)` | a call | `SendInput(...)` |
| `user32.input_mouse` | a read, snake_case | the constant `INPUT_MOUSE` |
| `user32.Input` | a read, PascalCase | the C type `INPUT` (reflection only -- see Structs below; **not built**) |

A constant or a type is only available when the constructor was given a header; without one a library exposes
functions and nothing else, and a constant read is `'fixture_answer' is a constant, and a constant comes from the
library's header: name it as DynamicLibrary's third argument`.
The reflection members every value has (`.class`, `.attributes`, `.functions`, `.memory`) are never foreign
symbols, so a generic class instantiated over `DynamicLibrary` can still ask `current.class.name`; neither are
`DynamicLibrary`'s own members (`file_name`, `handle` and its functions), so a `List<DynamicLibrary>`'s member
templates (`find_by_file_name`, `sort_by_handle`) read the attribute (proposed by Claude, unconfirmed;
`conformance/stage6/settings_and_libraries_reflected`).

#### The class

`DynamicLibrary(file_name, naming, header)`. Every argument must be a literal, because the compiler reads them
during codegen. The class as D4 designs it -- **planned**; what is built follows it:

```dynamic_library.spite
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

In this design `_open`/`_call`/`_resolve`/`_close` are the only operations below Spite; everything above them is
ordinary Spite, and `--final-classes` prints the whole class with every generated binding, as
[Decided by Mortaro, being implemented](open_questions.md#decided-by-mortaro-being-implemented--planned) item 2
requires of all reflection. D147 and D178 go further: loading a library is to be plain Spite calling the
operating system, with no compiler-supplied C at all.

**What is built** (D81/D82): `library/dynamic_library.spite` is the `singleton` line, `file_name` and
`handle`, a constructor that stores the file and calls `open_library(file)`, and a `drop()` that calls
`close_library(handle)`; `open_library`, `close_library` and `find_symbol(name, wanted_by)` are the compiler's
reopening, declared without a body, their C written in the compiler -- the stopgap D147 rejects, **not yet
replaced**. The naming rule and the calls themselves are still the compiler's (`missing_function` and
`missing_attribute` are not built), and every call through a `DynamicLibrary` value is a foreign call, since
`remove` and `exit` are names both a class and a C library could have.

- **`missing_function` and `missing_attribute` are the only two new names in the entire foreign function
  interface.** They are Ruby's `method_missing` resolved at compile time: [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s rule ("a parameter of class
  `Symbol` whose name is a segment of its function's name turns the function into codegen for every name that
  fits") taken to its limit, where the segment is the whole name. They get reserved names rather than falling out
  of the segment rule so that a class opts in by defining them, instead of silently swallowing every misspelled
  call. [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s existing precedence is unchanged: a function with the exact name always wins.
- `missing_attribute` returning `attribute.class` is the same form as [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s `get_attribute(attribute:
  Symbol): attribute.class` -- the return type is whatever that symbol turns out to be: an `Integer` for a `#define`,
  a `Spite.Class` for a type name.
- **The naming rule is a function, not a table.** Three rules cover a whole library, and binding one more symbol
  is one more call site, never a second line. `'windows'` runs `abbreviated()` before `pascal_case()`, so
  `set_cursor_position` -> `set_cursor_pos` -> `SetCursorPos`: the linter's abbreviation table ([Style](style.md#style--implemented), "kept
  in one place so it is easy to extend") read backwards *is* the Win32 name generator. The table that makes C's
  spellings illegal in Spite is the table that translates back into them.
- **Constant reads are the C spelling, lowercased**, with no prefix rule -- `user32.mouseeventf_leftdown`. This is
  deliberate: C constant prefixes (`MOUSEEVENTF_`, `INPUT_`, `WM_`, `SW_`) follow no rule any function could infer.
  Give them Spite names once, in a `var`, and the ugly spelling stops there.

#### Structs

A `type` ([Types](values_and_types.md#types)) whose fields are all scalars is a C struct when it reaches a foreign call: passed by
address, never as a Spite heap object. There is no annotation -- the value's type comes from the `var`'s own
annotation or from the parameter it feeds, exactly like any other object literal matched by shape.

```mouse.spite
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
- `Input.size` is `sizeof` of the struct the compiler emitted -- ordinary reflection on a Spite type, with no
  library in the expression. **Not built** (`Type.size`).
- **The compiler checks your work for free.** When the library was given a header and uses the `'windows'` rule,
  it derives the C name (`Input` -> `INPUT`) and emits
  `_Static_assert(sizeof(INPUT) == sizeof(Mouse_Input), "mouse.spite:12 type Input does not match INPUT");`.
  A forgotten padding field fails the build at the Spite line, naming both types. **The derived C name is used to
  verify, never to generate.** Size is checked; field order is not, because C's own field names (`dwFlags`, `dx`,
  `mouseData`) are unmappable by any rule, and a table for them is exactly what this design refuses to have.
- A `type` containing a `String`, `List<T>`, `Dictionary<T>` or class field is an error at a foreign call: the
  all-scalars condition is enforced, not assumed. The error names the type (`a Labelled cannot cross into C: ...`,
  [below](#what-crosses-1)); naming the field is not built.

#### What crosses

[The table above](#what-crosses) is the rule, costs included: a number type is the C type of its width
([Variables and values](values_and_types.md#variables-and-values--implemented)), text, lists of numbers and
number-only `type`s cross by address at no cost, a text result is copied, and a class instance never crosses, since
its layout (header, reference count) is the compiler's business. Anything else is an error at the call:
`a Console cannot cross into C: a foreign function takes numbers, Boolean, enum values, text and a type whose
attributes are all numbers` (`diagnostics/foreign_call_mistakes`); a `type` with a `String` field and a
`List<String>` are rejected the same way, naming the type rather than the field.

**Argument widths** (proposed by Claude, unconfirmed; implemented). With a header, a call goes through the
header's prototype (`__typeof__(&Symbol)`), so C checks the argument count and converts each argument to its
parameter's type; integer-to-pointer conversion is allowed, because a handle is a `Long`, and a `void` function's
call reads as `0`. Without one, integer-like arguments widen to `int64_t` or `uint64_t` and `Float`/`Double` pass
as themselves ([the table above](#argument-widths)). Left open: a `Double` where C wants `float`, and Apple ARM's
stack arguments past the eighth, which it packs at their own width -- both need the header.

**Return types.** A foreign call returns a 32-bit `Integer`, and [Variables and values](values_and_types.md#variables-and-values--implemented)'s
right-to-left casting takes it from there. Anything wider, or not an integer, is asked for by suffix, since a
`double` comes back in a different register and nothing in the call says which: `_as_long` for a 64-bit integer
or a pointer, `_as_double` for a `double`, `_as_text` for a `const char*` copied into a `String`. The suffix is
[Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s ordinary segment template, not a new mechanism.

#### Lifetime, and what gets linked

- The library handle is a plain `Long`. **There is no `Pointer` type in Spite**, and this design does not add one.
- `DynamicLibrary` is a **singleton** (its file's `singleton` line), keyed by its file and naming rule: every class
  asking for `DynamicLibrary("user32.dll", 'windows', ...)` shares one object and one import table;
  `"gdi32.dll"` is a second one.
- The import table is the set of call sites that survived tree shaking: two symbols are resolved because two were
  called. They are resolved once, in the constructor, so every later call is one indirect call -- never a lookup
  per call.
- A missing library or a missing symbol aborts at construction, naming the file, the symbol and the Spite function
  that wanted it. This is a program-stopping error and is not reported any other way.
- `drop()` closes the library at exit, when singletons are destroyed in reverse creation order (D142: a singleton
  is not reference counted).
- D4's design had `--final-classes` print each binding with its resolved name and library as a `#` comment; as
  built it writes none, since D34 would reject it, and how to show the mapping is open question 10.

#### Not designed yet

- **Callbacks** (handing a Spite function to C as a function pointer). Spite has function values (D17, D39), but
  how one becomes a C function pointer, and on which thread C may call it, is not decided.
- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.
