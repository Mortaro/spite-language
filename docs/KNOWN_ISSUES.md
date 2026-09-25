# Known issues

Where the compiler falls short of the [documentation](README.md) today, or does something a careful reader would not
expect. Each runnable repro below pins the current behaviour, so when an issue is fixed, `check.sh` fails here and
this page gets updated in the same change. Features that are decided but simply not built yet are named on the
page that describes them, not here.

## Everything that is known

- **Cycles leak.** Reference counting cannot free two objects that hold each other; clear one side by hand
  ([memory.md](memory.md#cycles-leak)). Weak references are the planned fix.
- **What a `Parallel(function)` touches is not checked** (a `parallel_each_` pass is), although D179 decided that
  it may reach only its own instance, its locals and singletons the compiler makes safe (D183). Two threads writing one
  field, or one writing what another reads, is the program's mistake, and with reference-counted fields it can
  free a value another thread is reading ([concurrency.md](concurrency.md#parallel-work-that-computes)).
- **Only Windows runs.** The Linux and macOS folders of the library, and the concurrency built on them, are held to
  compiling by `check.sh`, not run.
- **An enum cannot be switched over**; compare it with `==` in an `if` chain.
- **`first()` and `last()` answer the default** on an empty list, where `[]` answers a `T?`.
- **A class object's stand-in runs its constructor.** Reading `.functions` of every class in
  `Spite.Class.instances` constructs each class whose constructor takes no arguments, printing whatever it
  prints, although [reflection.md](reflection.md) describes the stand-in as the class at its defaults.
- **A `load` in a file other than the entry file of the program's own folder is ignored** without a word; the
  program then fails on the first name the package would have supplied ([packages.md](packages.md)).
- **`spite format` deletes an empty line inside a function body** and reports the file formatted, where a compile
  of the same file refuses it with "a function body holds no empty lines" ([style.md](style.md)).
- **Some errors name no file**: `spite game --optimized=maybe` reports `:9: error: ...`, the line of
  `library/build.spite` without its path.
- **Rules decided but not checked yet**, each named on its page: a singleton bound as a local (D144), a folder
  name that is not snake_case (D181), a `while` doing what a passed function does (D171), and a `Heap<T>` or an
  old list name (`add`, `pop`) answering with the fix.
- **`Json`** writes a `Float` or `Double` holding infinity or not-a-number as `inf`/`nan`, which is not JSON,
  cannot read a `Symbol` attribute, writes a `type` or function-valued attribute as `{}`, and fails inside
  `library/json.spite` on a union attribute, where D22 wants a compile error at the program's line
  ([json.md](json.md)).
