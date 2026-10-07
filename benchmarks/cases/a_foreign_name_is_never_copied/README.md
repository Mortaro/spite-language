# A foreign name is never copied

The first call into a foreign library opens it and looks up every function the program calls in it at once. The
library's file name, each function's name and the name of the Spite function that calls it are all known while
compiling, so each is a `String` pointing at text written into the program, never copied onto the heap and freed
a moment later.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#a-foreign-name-is-never-copied).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the names are literals the compiler
  writes, and a constant `String` is never counted, so nothing has to prove it may be shared.

## The four forms

- [`naive/`](naive/): a `Lock` and a thousand rounds of taking it, adding one to a count and letting it go. The
  first `lock()` opens the system's thread library (`kernel32.dll` on Windows).
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a lock made on the heap, the
  library opened by name on first use and its three functions looked up by name, with the names passed as the
  literals they are.
- [`expert.c`](expert.c): a static lock and direct calls to the system's lock functions, linked with the program,
  so nothing is opened or looked up while it runs.
- [`generated.c`](generated.c): the function that opens the library and looks up its functions, and the lookup.

## What to look at in generated.c

`spite_foreign_library_1` makes the `DynamicLibrary` from `SPITE_STATIC_STRING("kernel32.dll", 12)` and calls
`DynamicLibrary_find_symbol` with `SPITE_STATIC_STRING("AcquireSRWLockExclusive", 23)` and
`SPITE_STATIC_STRING("Lock.acquire", 12)`: a pointer to the text and its length, which `GetProcAddress` reads in
place. The `SpiteString___release` of both names at the end of `DynamicLibrary_find_symbol` does nothing on a
constant. The second name is only for the message a missing function prints, `the library 'kernel32.dll' has no
'...', which Lock.acquire calls`. `naive.c` passes the same literals to `GetProcAddress`, and `expert.c` names no
function at all. (The compiler leaves 43 empty lines inside `spite_foreign_library_1`, where the lookups of the
library's functions this program does not call would be; they cost nothing.) `spite_foreign_library_2` opens the C
library, `ucrtbase.dll`, for `Console` the same way, its names constant too.

## Why there is no time

The names are looked up once, when the library is opened, so what this saves is a few allocations at the first
call, not time in the loop. What to compare is the allocations, which `--debug-memory` counts on Windows (on Linux
the library the standard one opens is the C library, which the allocation table opens before it counts, so the
counts there are the same either way):

| Program | Allocations |
|---|---|
| `naive/` under `--debug-memory` | 5: the `Lock`, its handle, the library and the program's two objects |
| the same before names were constant | 10, a copy of each name of more than 15 bytes ([docs/optimizations.md](../../../docs/optimizations.md#a-foreign-name-is-never-copied)) |
| `naive.c` | 2: the lock and its handle |
| `expert.c` | none |

The first row was measured with `spite benchmarks/cases/a_foreign_name_is_never_copied/naive --debug-memory`
on Windows, which printed `allocations: 5 frees: 5`; the second is the count the section gives for the same
library opened by an older compiler.

## Timings

<!-- timings -->
<!-- /timings -->
