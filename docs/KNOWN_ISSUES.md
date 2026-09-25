# Known issues

Where the compiler falls short of the [manual](../manual.md) today, or does something a careful reader would not
expect. Each runnable repro below pins the current behaviour, so when an issue is fixed, `check.sh` fails here and
this page gets updated in the same change. Features that are decided but simply not built yet are named on the
page that describes them, not here.

## Everything that is known

- **Cycles leak.** Reference counting cannot free two objects that hold each other; clear one side by hand
  ([memory.md](memory.md#cycles-leak)). Weak references are the planned fix.
- **What a `Parallel(function)` touches is not checked** (a `parallel_each_` pass is). Two threads writing one
  field, or one writing what another reads, is the program's mistake, and with reference-counted fields it can
  free a value another thread is reading ([concurrency.md](concurrency.md#parallel-work-that-computes)).
- **Only Windows runs.** The Linux and macOS folders of the library, and the concurrency built on them, are held to
  compiling by `check.sh`, not run.
- **An enum cannot be switched over**; compare it with `==` in an `if` chain.
- **`first()` and `last()` answer the default** on an empty list, where `[]` answers a `T?`.
- **The crash trace fills with library asserts.** A long-running program's failed guards inside the standard
  library (`String.matches_at`, `Dictionary` probing) take slots in the ring of asserts a crash reports.
- **`Json`** writes a `Float` or `Double` holding infinity or not-a-number as `inf`/`nan`, which is not JSON, and
  cannot read a `Symbol` attribute ([json.md](json.md)).
