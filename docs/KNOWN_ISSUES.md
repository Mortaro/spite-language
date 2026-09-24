# Known issues

Where the compiler falls short of the [manual](../manual.md) today, or does something a careful reader would not
expect. Each runnable repro below pins the current behaviour, so when an issue is fixed, `check.sh` fails here and
this page gets updated in the same change. Features that are decided but simply not built yet are named on the
page that describes them, not here.

## 1. Reading `.functions` makes an instance of the class

Reading `.functions` of a class that has functions builds its `Spite.Function` values bound to a default instance
of that class, and that instance is alive -- so `.instances` counts one more than the program made:

```spite title=functions_make_an_instance/gadget.spite
var name = ""

func Gadget(new_name: String) {
    name = new_name
}

func shout(): String {
    return name.upper_case()
}
```
```spite title=functions_make_an_instance/functions_make_an_instance.spite entry
var console = Console()

func FunctionsMakeAnInstance() {
    var gadget = Gadget("wrench")
    var before = Gadget.instances.count()
    var functions = Gadget.functions
    var functions_count = functions.count()
    var after = Gadget.instances.count()
    console.print(gadget.name, functions_count, before, after)
}
```
```output
wrench 1 1 2
```

The manual says `.instances` is every live instance; a test runner that counts instances after walking
`.functions` sees the extra one.

## 2. A class of your own can hide a class the library nests

A name is resolved from the class that uses it outwards: its own namespace first, then its folder, then the whole
program. `library/directory.spite` declares the union `Entry` (`Directory.Entry`), but a program with a class of
its own named `Entry` makes the library's `Directory.entries()` resolve `Entry` to the program's class, and the
program no longer compiles:

```spite title=nested_name_hidden/entry.spite
var name = ""
```
```spite title=nested_name_hidden/nested_name_hidden.spite entry error
var console = Console()

func NestedNameHidden() {
    var entry = Entry()
    console.print(entry.name)
}
```
```diagnostic
a Directory cannot be used where a Entry is needed
```

Until it is fixed, do not name a class `Entry`.

## 3. Everything else that is known

- **Cycles leak.** Reference counting cannot free two objects that hold each other; clear one side by hand
  ([memory.md](memory.md#cycles-leak)). Weak references are the planned fix.
- **What a `Parallel` function touches is not checked.** Two threads writing one field, or one writing what
  another reads, is the program's mistake, and with reference-counted fields it can free a value another thread
  is reading ([concurrency.md](concurrency.md#parallel-work-that-computes)).
- **Only Windows runs.** The Linux and macOS folders of the library, and the concurrency built on them, are held to
  compiling by `check.sh`, not run.
- **An enum cannot be switched over**; compare it with `==` in an `if` chain.
- **`first()` and `last()` answer the default** on an empty list, where `[]` answers a `T?`.
- **The crash trace fills with library asserts.** A long-running program's failed guards inside the standard
  library (`String.matches_at`, `Dictionary` probing) take slots in the ring of asserts a crash reports.
- **`Json`** writes a `Float` or `Double` holding infinity or not-a-number as `inf`/`nan`, which is not JSON, and
  cannot read a `Symbol` attribute ([json.md](json.md)).
- **Printing an integer** with `console.print` writes its digits directly, so a program that reopens `Int.text()`
  changes interpolation (`"{count}"`) but not printing.
- **`Console.print`, `write` and `error`** take any values through a special case in the compiler rather than a
  variadic parameter, until it is decided what "printable" means.
