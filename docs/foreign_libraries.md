# Foreign libraries and operating systems

A native library is a class, not a keyword. `DynamicLibrary(file, naming, header)` opens a `.dll`, `.so` or
`.dylib`, and every function it exports is called as if it were a member: there is no `extern` declaration, no
binding file and no generated shim. The standard library reaches the operating system this way -- `File`,
`Directory`, `Process`, `Console` and the rest are Spite over the C runtime, reached through `DynamicLibrary` --
so what a program can do with it, the library already does.

## Calling a function

```spite title=foreign_call/foreign_call.spite entry
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

`Build().target_operating_system` is a constant, so only the branch for the system the program is compiled for
is in it ([programs.md](programs.md#the-operating-system-operating_system-and-target_operating_system)); the
standard library itself does this more neatly, by reopening classes per system ([below](#each-operating-system-reopens-what-it-changes)).

- **The arguments are literals**, because the compiler reads them while compiling. The file is named exactly as
  it is on disk, extension included -- a name without one is an error.
- **One instance per distinct argument list.** `DynamicLibrary` is a singleton keyed by its arguments: every class
  asking for `DynamicLibrary("ucrtbase.dll", 'identity', "")` shares one library and one table of symbols.
- **Only what is called is bound.** The symbols a program calls are looked up once, when the library opens, so
  every later call is one indirect call. A missing library or a missing symbol stops the program at that point,
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
| every number type, `Bool` | the same C type -- a number already is one |
| `String`, as an argument | `const char*`, at no cost |
| an enum value | its integer |
| `List<T>` of numbers | its element array, by address; C's writes come back into the list |
| a `type` whose attributes are all numbers | a C struct in declaration order, by address; C's writes come back |
| a class instance | never: its layout is the compiler's business |

A call returns an `Int`, and the ordinary right-to-left cast takes it from there. A result that is wider or not an
integer is asked for by suffix: `strlen_as_long(...)` for a 64-bit integer or a pointer, `half_of_as_double(...)`
for a `double`, and `greeting_as_text()` for a `const char*` copied into a `String`. An address is a `Long`; Spite
has no pointer type. Text that C leaves at an address -- a name inside a struct C filled in, or a `_as_long`
result that may be 0 -- is read with `memory.terminated_text(address)`, which copies up to the terminating zero.

With a header as the third argument, a snake_case read is a constant -- `user32.mouseeventf_leftdown` is
`MOUSEEVENTF_LEFTDOWN`, an `Int` -- and under the `'windows'` rule the compiler checks that a `type` passed as a
struct has the size of the header's struct of the derived name (`PointPair` against `POINT_PAIR`), so a missing
padding field fails the build at the Spite line. A header path that exists relative to the working directory is
included as a file; anything else as a system header.

`conformance/stage6/foreign_library` calls a library of every kind above, built from its own `fixture.c`.

## Each operating system reopens what it changes

There is no platform layer. `library/` holds what every system shares, and `library/windows/`, `library/linux/`
and `library/mac/` hold only what differs: each reopens the classes it changes, with a `DynamicLibrary` of its own
naming the system's real file. `library/windows/program.spite` is all of `Program`'s Windows side:

```
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
these folders, named by `Build().target_operating_system` ([programs.md](programs.md#how-a-program-is-loaded)), so
their functions replace or add members by the ordinary reopening rule ([packages.md](packages.md#monkey-patching-mods)).
`--final_classes` prints each class as it came out, its system's functions included.

Only the Windows folder runs today; `check.sh` holds the Linux and macOS folders to compiling, by writing the
compiler out once for each.

## What the compiler supplies

The members whose bodies stay C -- opening a library, finding a symbol, `Memory`'s reads and writes -- are
declared in Spite as a `func` with a signature and no body, which the compiler merges into the class as its own
reopening ([compiler.md](compiler.md#inspect-merged-classes)). `library/dynamic_library.spite` holds the rest:
the `singleton` line, `file_name` and `handle`, the constructor and `drop()`.

**Not built yet:** the naming rule and the calls as reopenable Spite (`missing_function` and
`missing_attribute`), a naming rule of your own, `Type.size`, reading a header's types through reflection,
callbacks from C into Spite, and C's variadic functions
([manual section 17](../manual.md#17-foreign-libraries--partial)).
