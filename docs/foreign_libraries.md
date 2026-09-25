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

func measure(text: String): Int {
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
- **One instance per distinct argument list.** `DynamicLibrary` is a singleton keyed by its arguments: every class
  asking for `DynamicLibrary("ucrtbase.dll", 'identity', "")` shares one library and one table of symbols.
- **Only what is called is bound.** The symbols a program calls are looked up once, when the library opens, so
  every later call is one indirect call. A symbol named only by code the program never reaches is not looked up
  at all ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)), except in a `--development` build. A missing library or a missing symbol stops the program at that point,
  naming the file, or the symbol and the Spite function that wanted it.
- **A foreign call goes through the attribute or variable holding the library**, so the compiler knows which
  table binds it.

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

| Spite | C |
|---|---|
| every number type, `Bool` | the same C type -- a number already is one, and see the widths below |
| `String`, as an argument | `const char*`, at no cost |
| an enum value | its integer |
| `List<T>` of numbers | its element array, by address; C's writes come back into the list |
| a `type` whose attributes are all numbers | a C struct in declaration order, by address; C's writes come back |
| a class instance | never: its layout is the compiler's business |

A call returns an `Int`, and the ordinary right-to-left cast takes it from there. A result that is wider or not an
integer is asked for by suffix: `strlen_as_long(...)` for a 64-bit integer or a pointer, `half_of_as_double(...)`
for a `double`, and `greeting_as_text()` for a `const char*` copied into a `String`. An address is a
`Memory.Address`, a number of 64 bits that casts to and from a `Long`; Spite has no pointer type. Text that C
leaves at an address -- a name inside a struct C filled in, or a `_as_long` result that may be 0 -- is read with
`address.terminated_text()`, which copies up to the terminating zero.

With a header as the third argument, a snake_case read is a constant -- `user32.mouseeventf_leftdown` is
`MOUSEEVENTF_LEFTDOWN`, an `Int` -- and under the `'windows'` rule the compiler checks that a `type` passed as a
struct has the size of the header's struct of the derived name (`PointPair` against `POINT_PAIR`), so a missing
padding field fails the build at the Spite line. A header path that exists relative to the working directory is
included as a file; anything else as a system header. The reflection members every value has -- `.class`,
`.attributes`, `.functions` and `.memory` -- are never read as constants, so a generic class instantiated
over `DynamicLibrary` can still ask `current.class.name`. Neither are `DynamicLibrary`'s own members, such as
`file_name`: a `List<DynamicLibrary>`'s member templates (`find_by_file_name`, `sort_by_handle`) read the
attribute, which is what the templates mean whenever a program describes that list's functions.

### Argument widths

**With a header**, the call goes through the header's own prototype: the function has to be declared there, the
number of arguments is checked, and each argument is converted to its parameter's type the way C converts one --
an `Int` literal where C wants a 64-bit `VkDeviceSize`, a `Double` where it wants a `float`. A `Long` handle goes
where C wants a pointer, since Spite has no pointer type; that integer-to-pointer conversion is the one C
complaint the call silences. A C function that returns `void` is called the same way, and its call reads as `0`.

**Without a header**, the compiler knows only the Spite types, so it passes what the platform's calling convention
passes anyway:

| Spite argument | Passed as |
|---|---|
| `Tiny`, `Short`, `Int`, `Long`, `Bool`, an enum value | `int64_t`, sign-extended |
| `Byte`, `UnsignedShort`, `UnsignedInt`, `UnsignedLong` | `uint64_t`, zero-extended |
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

func sleep(milliseconds: Int) {
    kernel.Sleep(milliseconds)
}

func exit_process(code: Int) {
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
the `singleton` line, `file_name` and `handle`, the constructor and `drop()`.

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

**Built (milestone 11a, 2026-09-23):** `DynamicLibrary(file, naming, header)` with the three naming rules, calls
of any function the library exports, the import table of exactly the called symbols resolved once when the
library opens, and a program-stopping message naming the file, or the symbol and the Spite function that wanted
it, when either is missing (`conformance/stage6/foreign_library`, `diagnostics/foreign_library_mistakes`,
`diagnostics/foreign_call_mistakes`). What crosses is the table below, minus structs and lists. Choices Claude made
while building it (proposed, unconfirmed): one library per distinct file and naming rule, following D8's
"one instance per literal argument list", opened on first use and closed at exit; the file is named exactly as it is on disk and a name without an
extension is a compile error (D71 -- a wrapper for another platform names that platform's file), which `check.sh`
meets by building each fixture's C into `fixture.dll` beside its program on every platform; `_as_long` beside
`_as_double` and `_as_text`, since a plain call returns a 32-bit `Int` and a handle or pointer needs 64; the
naming rule is a symbol literal (D70); a foreign function is called only through the attribute or variable that
holds its `DynamicLibrary(...)`, so the compiler knows which table binds it; and `--final-classes` writes no
resolved-name comments, which D34 would reject (how to show them is open question 10). A constant reads from the header (`user32.mouseeventf_leftdown` is `MOUSEEVENTF_LEFTDOWN`, an `Int`); a header path
that exists relative to the working directory is included as a file, anything else as a system header. A `List<T>` of numbers crosses as its element array (C may write into it), and a value of a `type` whose attributes are all numbers crosses by address as a C struct
in its declared order, and C's writes come back into the value afterwards; under the `'windows'` rule with a header,
`_Static_assert` checks the layout against the header's struct, named `PointPair` -> `POINT_PAIR` (Claude: only
there, since other headers do not name structs that way, and a missing name would be a C error). Not built:
`Type.size`, reading a header's types as Spite reflection, `missing_function`/`missing_attribute` as reopenable Spite, and a
user-written naming rule (11c).

D4 (decided by Mortaro, 2026-09-19): **a native library is a class, not a keyword.** There is no `external`
keyword, no per-symbol binding string, no generated-binding step and no hand-written C shim. `DynamicLibrary`
is an ordinary standard library class, and its Symbol codegen ([Symbol codegen](metaprogramming.md#symbol-codegen--implemented)) resolves every foreign function,
constant and type at compile time.

```mouse.spite
var user32 = DynamicLibrary("user32.dll", 'windows', "windows.h")

func move_to(x_position: Int, y_position: Int): Bool {
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
| `user32.Input` | a read, PascalCase | the C type `INPUT` (reflection only -- see Structs below) |

A constant or a type is only available when the constructor was given a header; without one a library exposes
functions and nothing else, and a constant read is a diagnostic naming the missing header.
`DynamicLibrary`'s own members (`file_name`, `handle` and its functions) are never foreign symbols, as the
reflection members never are (proposed by Claude, unconfirmed, 2026-09-25): a `List<DynamicLibrary>` built its
member templates over them the moment a program read any class object's `.functions`, and `sort_by_handle` failed
inside `library/list.spite` as a constant read (`conformance/stage6/settings_and_libraries_reflected`).

#### The class

`DynamicLibrary(file_name, naming, header)`. Every argument must be a literal (the same rule `load` already
has, [Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)), because the compiler reads them during codegen.

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

func missing_function(function: Symbol, arguments: Arguments): Int {
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

`_open`/`_call`/`_resolve`/`_close` are compiler intrinsics; everything above them is ordinary Spite, and
`--final-classes` prints the whole class with every generated binding, exactly as [Decided by Mortaro, being implemented](open_questions.md#decided-by-mortaro-being-implemented--planned) item 2 requires of
all reflection.

**What is built** (2026-09-24, D81/D82): `library/dynamic_library.spite` is the `singleton` line, `file_name` and
`handle`, a constructor that stores the file and calls `open_library(file)`, and a `drop()` that calls
`close_library(handle)`; `open_library`, `close_library` and `find_symbol(name, wanted_by)` are the compiler's
reopening, declared without a body. The naming rule and the calls themselves are still the compiler's
(`missing_function`/`missing_attribute` are not built), and every call through a `DynamicLibrary` value is a
foreign call, since `remove` and `exit` are names both a class and a C library could have.

- **`missing_function` and `missing_attribute` are the only two new names in the entire foreign function
  interface.** They are Ruby's `method_missing` resolved at compile time: [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s rule ("a parameter of class
  `Symbol` whose name is a segment of its function's name turns the function into codegen for every name that
  fits") taken to its limit, where the segment is the whole name. They get reserved names rather than falling out
  of the segment rule so that a class opts in by defining them, instead of silently swallowing every misspelled
  call. [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s existing precedence is unchanged: a function with the exact name always wins.
- `missing_attribute` returning `attribute.class` is the same form as [Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s `get_attribute(attribute:
  Symbol): attribute.class` -- the return type is whatever that symbol turns out to be: an `Int` for a `#define`,
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
    kind: UnsignedInt
    padding: UnsignedInt
    x_movement: Int
    y_movement: Int
    wheel_amount: UnsignedInt
    event: UnsignedInt
    time_stamp: UnsignedInt
    extra_information: UnsignedLong
}

func _send(event: UnsignedInt, wheel_amount: UnsignedInt) {
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
  library in the expression.
- **The compiler checks your work for free.** When the library was given a header, it derives the C name through
  the same naming rule (`Input` -> `INPUT`) and emits
  `_Static_assert(sizeof(INPUT) == sizeof(Mouse_Input), "mouse.spite:12 type Input does not match INPUT");`.
  A forgotten padding field fails the build at the Spite line, naming both types. **The derived C name is used to
  verify, never to generate.** Size is checked; field order is not, because C's own field names (`dwFlags`, `dx`,
  `mouseData`) are unmappable by any rule, and a table for them is exactly what this design refuses to have.
- A `type` containing a `String`, `List<T>`, `Dictionary<T>` or class field is a diagnostic at a foreign call,
  naming the field: the all-scalars condition is enforced, not assumed.

#### What crosses

| Spite | C | Cost |
|---|---|---|
| `Tiny`..`Long`, `Byte`..`UnsignedLong`, `Float`/`Double`, `Bool` | `int8_t`..`uint64_t`, `float`/`double`, `bool` | zero -- [Variables and values](values_and_types.md#variables-and-values--implemented)'s numeric types already are the C types |
| `String` (argument) | `const char*` | zero -- every `String` buffer is already NUL terminated |
| `String` (result) | `const char*` | one copy; Spite never frees a buffer C owns |
| an enum value | its integer | zero |
| a scalar-only `type` | a struct, by address | zero |
| `List<T>` of scalars | the element array, by address | zero |
| a class instance | -- | never. Its layout (`SpiteHeader`, ref count) is the compiler's business |

**Argument widths** (proposed by Claude, unconfirmed; implemented 2026-09-24). With a header, a call goes through
the header's prototype (`__typeof__(&Symbol)`), so C checks the argument count and converts each argument to its
parameter's type; integer-to-pointer conversion is allowed, because a handle is a `Long`, and a `void` function's
call reads as `0`. Without one, every integer-like argument (`Tiny`..`Long`, `Bool`, enum values) is passed as
`int64_t`, and every unsigned one as `uint64_t`: the width x64 and 64-bit ARM pass in a register or stack slot
anyway, so a literal `0` for a 64-bit parameter no longer leaves the upper half of the register undefined.
`Float` and `Double` pass as themselves. Left open: a `Double` where C wants `float`, and Apple ARM's stack
arguments past the eighth, which it packs at their own width -- both need the header.

**Return types.** A foreign call returns `Int` (integer/pointer width) and [Variables and values](values_and_types.md#variables-and-values--implemented)'s right-to-left casting takes
it from there. A `Double` comes back in a different register and cannot be inferred from context, so it is asked
for by suffix -- `user32.get_scale_as_double(...)`, and `..._as_text()` for a copied `const char*` -- which is
[Symbol codegen](metaprogramming.md#symbol-codegen--implemented)'s ordinary segment template, not a new mechanism.

#### Lifetime, and what gets linked

- The library handle is a plain `Long`. **There is no `Pointer` type in Spite**, and this design does not add one.
- `DynamicLibrary` is a **singleton** (open question 7 -- the syntax is not decided yet, which is why the class above
  does not show one), keyed by its constructor arguments: every class asking for
  `DynamicLibrary("user32.dll", ...)` shares one object and one import table; `"gdi32.dll"` is a second one.
- The import table is the set of call sites that survived tree shaking: two symbols are resolved because two were
  called. They are resolved once, in the constructor, so every later call is one indirect call -- never a lookup
  per call.
- A missing library or a missing symbol aborts at construction, naming the file, the symbol and the Spite function
  that wanted it. This is a program-stopping error and is not reported any other way.
- `drop()` closes the library when its last reference goes, like any other class.
- `--final-classes` prints each binding with its resolved name and library as a `#` comment, the same way it
  already annotates which root won a reopened function -- so the mapping is reviewable generated output rather
  than something maintained by hand.

#### Not designed yet

- **Callbacks** (handing a Spite function to C as a function pointer). Needs a decision on function references,
  which the language does not have.
- **A C union past its first member.** Positional layout reaches the first member only; the rest needs C's own
  field names.
- **Varargs**, structs returned by value, and `#define`s that are function-like macros rather than constants.
