# Standard library

The specification of [Standard library](../docs/standard_library.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Standard library

Every value below is reference counted, exactly like a user class: a `String`/`List<T>`/`Dictionary<Key, Value>` is freed
the moment its last reference goes, not by a single-owner convention. See [Memory](../docs/memory.md#memory) for the
retain/release rules and the cycle caveat.

**What no company owns is standard; what is built on a branded product is a package.** A protocol, a file format,
an algorithm or maths that no company owns belongs in `library/`, and the library grows to cover it when a program
needs it (HTTP, TLS, SHA-256/HMAC, Argon2, secure random bytes, base64, gzip, and the maths games need). A client
for a branded product (a database such as MongoDB or Postgres, a vendor's API or service) is never added to
`library/`: it is a package, maintained by whoever owns it, loaded pinned to a commit. An operating system's
folder is how a standard class is written for that system, not a brand package. Nothing in `library/` breaks the
rule.

Every class here is Spite in `library/`, compiled with the program, and tree-shaken like the program's own code,
so a program carries only what it reaches ([Pure Spite](#pure-spite-dissolving-the-runtime) says what is still the
compiler's).

### The String class

An immutable value with a length (not a bare `char*`), sixteen bytes wherever it is kept. A literal is static and
never allocates; other text of up to 15 bytes is kept in those sixteen bytes and never allocates either; longer
text is one block on the heap, counted and freed with its last holder. **Its storage is Spite**:
`library/string.spite` declares `_bytes: Memory.Address` (where the characters are, with a 0 after the last) and
`_length: Long`, and nothing else (the compiler lays the sixteen bytes out and reads them through the form the
text takes), and every member but three is a Spite function reading them
([memory.md](../docs/memory.md#where-string-and-integer-keep-their-memory)). The three the compiler supplies are the ones
that depend on where the characters are: `code_at` (reading one), `sum` (`+`), and
`Memory.Address.text(length)`, which is how every `String` is made from bytes (`slice`, `terminated_text()` and
anything that fills a buffer itself end in it). The right side of `+` casts toward
`String` (every numeric type, `Boolean`, and enum all format to text; see
[Numeric types](values_and_types.md#numeric-types) for `Float`/`Double`'s
shortest-round-trip printing); assigning a `String` to any numeric variable parses it, defaulting to `0`/`0.0` on
failure. `==`/`!=`/`<`/`>` compare by content, and
([Operators](functions_and_operators.md#operators)'s table) also work as
`equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

**Building text in a loop is linear.** `text = text +
piece`, `text = "{text}{piece}"` and any longer join that starts with the variable it is stored back into append
to that text in place when nothing else holds it, and otherwise copy it once with room to grow, so a `String`
shared by two names never changes under the other one. Short text grows inside its sixteen bytes and moves into a
block once it passes 15 bytes. When it applies and what it saves is
[optimizations.md](../docs/optimizations.md#appending-to-text-in-place)'s; `conformance/stage6/text_building` pins the
allocation counts.

| Method | Result | Notes |
|---|---|---|
| `length()` | `Integer` | |
| `is_empty()` | `Boolean` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty/invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Integer` | `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Boolean` | |
| `index_of(text)` | `Integer` | `-1` when absent |
| `replace(from, to)` | `String` | replaces every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | splits on `\n` |
| `pluralize()` / `singularize()` | `String` | the English plural or singular of the last word, by the rules below |
| `to_tiny()` / `to_short()` / `to_integer()` / `to_long()` | `Tiny?` / `Short?` / `Integer?` / `Long?` | digits with an optional `+` or `-`, spaces around them allowed; `null` for anything else (`"12abc"`, `""`) or a number the type cannot hold |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_integer()` / `to_unsigned_long()` | `Byte?` / `UnsignedShort?` / `UnsignedInteger?` / `UnsignedLong?` | the same, and `null` for a negative number |
| `to_float()` / `to_double()` | `Float?` / `Double?` | digits with an optional sign, `.` and exponent (`-1.5e2`, `.5`), spaces around them allowed; `null` for anything else (`"1e"`, `"nan"`) |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Boolean` | what `+`/`==`/`<`/`>` call; they are only called through the operator |
| `to_string()` / `to_debug()` | `String` | the text itself / the text quoted, with `"`, `\` and a line feed escaped ([`Console.debug`](../docs/standard_library.md#console)) |
| `to_bytes()` | `List<Byte>` | the text's bytes, one per byte of its UTF-8, for [hashing, encoding and compressing](../docs/standard_library.md#bytes-base64-compression-hashes-and-passwords) |

**`pluralize()` and `singularize()` inflect English the way Rails' ActiveSupport does**, so a
name and its collection read as a pair: `"active_quest".pluralize()` is `"active_quests"`, and
`"map_names".singularize()` is `"map_name"`. Only the last word changes: the text after the last `_` or space, or
from the last capital that follows a lower-case letter (`"ActiveQuest"` → `"ActiveQuests"`). The word is inflected
in lower case and handed back in the case it came in: `"Person"` → `"People"`, `"ACTIVE_QUEST"` →
`"ACTIVE_QUESTS"`. In order, a whole last word that is **uncountable** stays as it is; an **irregular** one
swaps with its pair; otherwise the first of ActiveSupport's ending rules that matches applies (`box` → `boxes`,
`enemy` → `enemies`, `wolf` → `wolves`, `knife` → `knives`, `analysis` → `analyses`, `medium` → `media`, `index` →
`indices`, `matrix` → `matrices`, `vertex` → `vertices`, `mouse` → `mice`, `ox` → `oxen`, `octopus` → `octopi`, `quiz`
→ `quizzes`, `status` → `statuses`), and a word no rule names takes or loses an `s`. A word already in the asked
number is left alone (`"quests".pluralize()` is `"quests"`, `"people".pluralize()` is `"people"`), and `""` answers
`""`. The rules are ActiveSupport's own; the words they would get wrong are in the table.

The uncountable and irregular words are a table in the library, the singleton `String.Inflection`
(`library/string/inflection.spite`): `irregulars`, a dictionary from each singular to its plural (`person` →
`people`, `man` → `men`, `woman` → `women`, `child` → `children`, `foot` → `feet`, `tooth` → `teeth`, `goose` →
`geese`, `cactus` → `cacti`, `fungus` → `fungi`, `radius` → `radii`, `criterion` → `criteria`, `phenomenon` →
`phenomena`, `appendix` → `appendices`, `leaf` → `leaves`, `hero` → `heroes`, `potato` → `potatoes` and the rest),
and `uncountables`, a list (`data`, `equipment`, `health`, `information`, `money`, `news`, `series`, `sheep`,
`software` and the rest). A program adds a word by reopening it: a file `string/inflection.spite` in the program
that declares `irregulars` or `uncountables` replaces that list whole, so it restates the library's words it keeps:

```gdscript
var irregulars = {"person": "people", "sheaf": "sheaves"}
```

`pluralize()` reads the program's table at run time, and the compiler reads the same table where it inflects a
member's name ([metaprogramming.md](../docs/metaprogramming.md)), so `map_sheaves()` collects a `sheaf` attribute once
`sheaf` is in it (`conformance/stage6/inflection_table`). Called on a literal (`"cactus".pluralize()`), the answer is worked out while compiling
([optimizations.md](../docs/optimizations.md#a-word-inflected-while-compiling)). They are functions, like `upper_case()`, because a `String` has no attributes for a getter to answer.

### Maths

The maths of a 3D engine (skinning, animation, lighting) as members of the number classes, so it adds no
syntax and binds no singleton ([values_and_types.md](../docs/values_and_types.md#maths-functions) teaches them). On `Float` and `Double`,
each answering the receiver's type unless it says otherwise, with every other operand cast to that type like any
argument:

| Member | `Float` / `Double` in C | Answers |
|---|---|---|
| `square_root()` | `sqrtf` / `sqrt` | not-a-number below zero; `-0` for `-0` |
| `sine()`, `cosine()`, `tangent()` | `sinf`, `cosf`, `tanf` / `sin`, `cos`, `tan` | of an angle in radians |
| `arc_sine()`, `arc_cosine()` | `asinf`, `acosf` / `asin`, `acos` | not-a-number outside -1 to 1 |
| `arc_tangent()` | `atanf` / `atan` | between -pi/2 and pi/2 |
| `rise.arc_tangent_over(run)` | `atan2f` / `atan2` | the angle of the point `(run, rise)`, between -pi and pi, in the right quadrant; `0` for `(0, 0)` |
| `power(exponent)` | `powf` / `pow` | |
| `exponential()` | `expf` / `exp` | infinity once it overflows |
| `logarithm()`, `logarithm_base_2()`, `logarithm_base_10()` | `logf`, `log2f`, `log10f` / `log`, `log2`, `log10` | the natural logarithm and the other two; minus infinity at `0`, not-a-number below it |
| `floor()`, `ceiling()`, `truncate()` | `floorf`, `ceilf`, `truncf` / `floor`, `ceil`, `trunc` | a whole value, still in the receiver's type: `-2.5` gives `-3`, `-2` and `-2` |
| `round()` | `roundf` / `round` | half away from zero: `2.5` is `3`, `-2.5` is `-3` |
| `absolute()` | `fabsf` / `fabs` | |
| `minimum(other)`, `maximum(other)` | `fminf`, `fmaxf` / `fmin`, `fmax`, after an `isnan` test of each operand | not-a-number passes on: `nan.minimum(0.0)` and `(0.0).minimum(nan)` are `nan`, never a real number that hides it |
| `clamp(low, high)` | `maximum(low)` then `minimum(high)` | not-a-number in any operand gives `nan`, and `high` wins when `low` is above it (`conformance/stage6/nan_passes`) |
| `is_finite()`, `is_infinite()`, `is_not_a_number()` | `isfinite`, `isinf`, `isnan` | a `Boolean` |

On every whole number, `Tiny` to `UnsignedLong`, answering the receiver's type:

| Member | Answers |
|---|---|
| `absolute()` | the value without its sign; the smallest signed value wraps to itself, as its negation does (`Integer.smallest.absolute()` is -2147483648), and an unsigned value is itself |
| `minimum(other)`, `maximum(other)` | the smaller or larger, `other` cast first: a `Byte`'s `minimum(300)` compares with 44 |
| `clamp(low, high)` | `value` held between `low` and `high`, with `high` winning when `low` is above it, as on a float |

**Constants are get-only attributes of the class itself** (a class is an object): `Float.pi`, `tau`, `euler_number`,
`infinity`, `not_a_number`, `largest` (the largest finite value) and `smallest` (the most negative finite
one) on `Float` and `Double`, and `largest` and `smallest` on each whole number (`UnsignedLong.largest` is
18446744073709551615, its `smallest` 0). They are read without parentheses, and nothing can assign one:
`Float.pi = 3.0` is "'Float.pi' is a constant, and a constant is get-only: nothing can assign it", and
`Float.pi()` is "'Float.pi' is a constant, read as an attribute and never called: write 'Float.pi'"
(`diagnostics/maths_constant_assigned`). Each is the value itself in the C, written exactly (`0x1.921fb6p+1f`
for `Float.pi`, `INT32_MAX` for `Integer.largest`). On a value, `angle.pi` is "'pi' is a constant of the
class Float, not of a value: write 'Float.pi'"; on the class, a function of a value is "'Float.square_root()'
calls a function of a value on the class: the class Float answers only its constants, ..." and a name that is
neither is "the class Integer answers only its constants, largest and smallest, and 'pi' is not one of them"
(`diagnostics/maths_constant_on_value`).

**Nothing here halts.** Floats keep infinity and not-a-number, so every edge answers the IEEE 754 value the
C library gives (`(-1.0).square_root()` is not-a-number) and `errno` is never read. Not-a-number is never
dropped on the way: `minimum`, `maximum` and `clamp` answer it when an operand is it, where C's `fmin` and
`fmax` would answer the other operand as if nothing were wrong. The whole-number
division checks are untouched: none of these divides.

**Each is a primitive of the language, lowered by the backend** (named in Spite, lowered in one
place, so no Spite changes with the backend). `--final-classes` prints them as bodiless declarations in each number
class, and `bootstrap/source/generation/maths_primitives.spite` is the one place that says what C each becomes:
a macro written where it is called, so `angle.sine()` is `sinf(angle)` in the C, with no function of Spite's own
around it, and a whole number's `clamp` is two comparisons in one statement. None has hand-written C in a `.spite`
file of `library/`. A call whose operands are all constants, `(0.5).sine()` or `Float.pi.cosine()`, is worked out
while compiling with the same C library function, so its answer is bit for bit the one the program would have
computed ([optimizations.md](../docs/optimizations.md#maths-on-constants-is-worked-out-while-compiling);
`conformance/stage6/maths_folding`).

**Tree-shaken, and nothing at run time.** A member a program never calls is not in its C; `<math.h>` is
included only when a member that calls the C library, or a `%` on a `Float` or `Double` (`fmodf`, `fmod`),
survives tree shaking, and only then does a build on Linux or
macOS link it (`-lm`; Windows has it in the C runtime). A program that uses none of it, `examples/hello`, carries
no `#include <math.h>`. There is no `fused_multiply_add`: without a processor flag telling the C compiler the
machine has the instruction, `fmaf` is a slow exact routine in the C library, a cost a reader would not expect
from one multiply and add.

Other languages' short names are errors naming the Spite one, on the class that has it: `side.sqrt()` is "Float
has no function 'sqrt': Spite spells it 'square_root', since no name is abbreviated", and likewise `sin`, `cos`,
`tan`, `asin`, `acos`, `atan`, `atan2`, `pow`, `exp`, `log`, `ln`, `log2`, `log10`, `ceil`, `trunc`, `abs`,
`fabs`, `min`, `max`, `isnan`, `isinf` and `isfinite` (`diagnostics/maths_other_spellings`).
`conformance/stage6/maths_functions` pins the values and the edges, and `maths_precision` what `Float` loses
against `Double`.

### System classes

Ordinary classes in `library/`, each written once for every system, with the few functions that call the system
in `library/windows/`, `library/linux/` and `library/mac/`, which reopen the class and reach the system's own
library through `DynamicLibrary`. What each member answers is in the section of [the page](../docs/standard_library.md) that teaches it.

| Class | Members |
|---|---|
| `File(path)` | `path`, `name` (the last piece of the path), `to_string(): String` (the path), `read(): String?`, `write(text): Boolean`, `append(text): Boolean`, `exists(): Boolean`, `remove(): Boolean`, `move_to(path): Boolean`; bytes: `size(): Long?`, `modified(): Instant?`, `read_bytes(): List<Byte>?`, `read_bytes_at(position, count): List<Byte>?`, `write_bytes(bytes): Boolean`, `append_bytes(bytes): Long?`, `map(): MappedFile?` ([Bytes are a List<Byte>](#bytes-are-a-listbyte)); see [Read and write a file](../docs/standard_library.md#read-and-write-a-file). A `File` is a path with no open handle: each call opens and closes the file, positions replace seeking (`_fseeki64`/`_ftelli64` on Windows, so a position past 2 GB works), and `modified()` reads `GetFileAttributesExA`'s `ftLastWriteTime` on Windows and `stat`'s modification time on Linux and macOS. `move_to(path)`, on a `File` and on a `Directory`, answers `false` and changes nothing when a file or folder is already at `path`, and otherwise moves with `rename` on Linux and macOS and `MoveFileExA` on Windows (a file may be copied across drives there; a folder never is); on `true` the value names `path` |
| `MappedFile` | made by `File.map()`: a read-only mapping of the whole file (`CreateFileMappingA`/`MapViewOfFile` on Windows, `mmap` with `PROT_READ` and `MAP_PRIVATE` on Linux and macOS, the file itself closed once mapped), undone by `drop()` (`UnmapViewOfFile`, `munmap`). `size(): Long`; `get_at(position): Byte?` (`mapped[position]`), `read_short`, `read_integer`, `read_long`, `read_float`, `read_double` (each `(position): T?`) and `text(position, count): String?` answer `null` unless every byte they would read is inside the file, so a bad offset read from the file cannot read outside it; a read inside is one load from the mapping. An empty file maps to a `MappedFile` of size 0, since the operating systems refuse to map nothing. `conformance/stage6/mapped_files`; see [A file larger than memory](../docs/standard_library.md#a-file-larger-than-memory-map-it) |
| `Directory(path)` | `path`, `name` (the last piece of the path), `entries(): List<Directory.Entry>` (below), `exists(): Boolean`, `create(): Boolean`, `move_to(path): Boolean`; see [List a directory](../docs/standard_library.md#list-a-directory) |
| `Process(command, arguments)` | `working_directory`, `environment_variables`, `run(): Integer`, `output(): String`, `run_attached(): Integer`; see [Run a process](../docs/standard_library.md#run-a-process). Each argument reaches the child whole: single-quoted for the shell on Linux and macOS, and quoted by the `CommandLineToArgvW` rules on Windows, a `key=value` argument quoting only its value (`-script="a b"`). `run()` reads the child's standard output through `_popen`/`popen`; `run_attached()` is the C library's `system` |
| `Program()` | a singleton: `exit(code)`, `sleep(milliseconds)`, `environment(name): String?`, `executable_path(): String`, `live_allocations(): Integer`; see [Program](../docs/standard_library.md#program). `exit` flushes `Console` first, since the C library's `exit` would drop what the program's own standard output still buffers |
| `Clock()` | a singleton: `elapsed_nanoseconds(): Long`, `elapsed_milliseconds(): Long`, `now(): Instant`; `elapsed_nanoseconds()` is the monotonic clock, with a nanosecond unit and no allocation per reading; see [Clock](../docs/standard_library.md#clock), [Time](time.md#time-one-stored-instant-zones-for-presentation) |
| `Console()` | a singleton: `print(...values)`, `write(...values)`, `error(...values)`, `debug(...values)`, `read_line(): String?`; see [Console](../docs/standard_library.md#console), and below |
| `FileSystemWatcher()` | `watch_for_changes(target: FileSystemWatcher.Target): Boolean`, `changes(): List<String>`, `wait_for_changes()`; see [Watch files and folders](../docs/standard_library.md#watch-files-and-folders), which is the rule. `HotReload` is built on it ([REPL and live reload](../docs/repl.md#repl-and-live-reload)) |
| `Socket()` | public library surface: `listen_locally(port): Boolean`, `listen_everywhere(port): Boolean`, `listen_at(host, port): Boolean`, `connect_locally(port): Boolean`, `connect(host, port): Boolean`, `accept_client(): Socket?`, `read_line(): String?`, `read_bytes(): List<Byte>?`, `write_line(text): Boolean`, `write_bytes(bytes): Boolean`, the calls that never wait `accept_client_now(): Socket?`, `read_line_now(): String?`, `read_bytes_now(): List<Byte>` and `write_bytes_now(bytes): Integer`, `closed: Boolean`, `close()`: TCP over IPv4 and IPv6 on every system ([Socket](../docs/standard_library.md#socket), and below); `--repl-port` and `spite connect` use `listen_locally` and `connect_locally` ([REPL and live reload](../docs/repl.md#repl-and-live-reload)) |
| `Concurrent(function)`, `Parallel(function)` | the handle stands in for what the function returned, and reading it is the wait; `finished: Boolean` never waits; dropping the handle waits for it; there is no `wait()` and no `join()`; see [concurrency.md](../docs/concurrency.md) |
| `ThreadPool()` | the singleton every `Parallel` runs on: `size(): Integer` worker threads, `worker_index(): Integer` (`-1` off the pool); see [The thread pool](../docs/concurrency.md#the-thread-pool) |
| `ThreadLocal<T>()`, `Lock()`, `ThreadSlot()` | one value per thread, `get(): T?`, `set(value)`; a lock, `while_locked(function)`, `lock()`, `unlock()`; the raw per-thread `Long` both are built on, `read()`, `write(value)`; see [A value per thread, and a lock](../docs/concurrency.md#a-value-per-thread-and-a-lock) |
| `DynamicLibrary(file_name, naming, header)` | every foreign function, constant and type of a native library; see [Foreign libraries](foreign_libraries.md#foreign-libraries). `library/dynamic_library.spite` holds its `file_name` and `handle`, its constructor and `drop()`; opening, closing and finding a symbol are the compiler's reopening |
| `ForeignBytes(address, count)` | memory a foreign library hands out (an address and a size it answers): `count(): Long`, `get_at(position): Byte?` (`foreign[position]`), `set_at(position, value)`, `read_<number>(position): <number>?` and `write_<number>(position, value)` with their `_big_endian` twins, as a `List<Byte>` has them, `read_bytes(position, count): List<Byte>?` and `write_bytes(position, bytes)`; see [Bytes are a List<Byte>](#bytes-are-a-listbyte) |
| `Memory.Heap()`, `Memory.Arena(block_bytes)`, `Memory.Address` | the floor every other type is built on: allocators and the place they hand out; see [Memory](../docs/memory.md#memory-is-the-floor-and-you-can-build-on-it), whose rules they are ([Memory](../docs/memory.md#memory)) |
| `TypedMemory<$value_type>()` | `read_value(address, index)`, `write_value(address, index, value)`, `release_value(address, index)`, `value_bytes()`: values of any type in raw memory, reference counts kept right; what `List<T>` keeps its elements with, and what a container of your own uses |

**`Console` is a singleton:** `Console()` is the same instance everywhere, `Console` is an ordinary class
name rather than a reserved word, and `console.class.name` is `"Console"` like any other class. Calling a member
on `Console()` directly is an error: `'Console' is a singleton: bind it once beside the attributes, 'var console =
Console()', and use 'console.print'`. `read_line()` answers `null` only at the end of the input with nothing
read, and drops a `\r` before the line feed. The entry constructor returning normally is exit code `0`.

**Printing is `to_string()`.** `print`, `write` and `error` are Spite in
`library/console.spite`, taking `...values: List<Printable>`, where

```gdscript
type Printable {
    to_string(): String
}
```

Every number, `Boolean`, `String` (whose `to_string()` answers itself), `Symbol`, enum value, `Spite.Class` (its
`.name`) and `Spite.Namespace` (its `.name_with_namespaces`) answers it, so everything that printed before prints
the same. A class of your own prints once it declares `func to_string(): String`, and passing one that does not is
the ordinary shape error, naming the function: `'Pet' does not fit type 'Printable': it has no function
'to_string'`. A `List` or a `Dictionary` has no `to_string()` either (`'List<Integer>' does not fit type
'Printable'`), so it is printed with `debug`, or joined first (`diagnostics/print_without_to_string`,
`conformance/stage6/printable_values`). A `type` writes a required function as `to_string(): String`. What stays
the compiler's is only the floor: `_write_output(text)`, `_write_error(text)`, `_write_held()` and `_flush()` are
private, declared nowhere in Spite, and the compiler supplies their C (`fwrite` and `fflush`), as it does
`Memory.Heap`'s.

**A printed line is written out at once.**
`print`, `error` and `debug` flush after their line break, so output redirected to a file or a pipe
shows each line when it is printed instead of when the C library's buffer fills or the program exits; `write`
does not, so a line built from pieces goes out with the next line end. `read_line()` flushes first, so a prompt
written with `write` is on the screen before the program waits for the answer; `error` flushes the output first, so
the two streams keep the order they were written in when they reach one file; and `Program.exit(code)` and
`Process.run_attached()` flush first too. In a program that starts a thread, each thread gathers what one `print`,
`write`, `error` or `debug` writes and hands it to the operating system in one piece, so two threads printing at
once never split each other's lines; a program with one thread writes
each piece as it comes. The C library already writes
a terminal's output promptly, so nothing changes there. Chosen as the cheapest way that shows every line: one
`WriteFile` or `write` per line. The alternatives cost the same or do not show
every line: line buffering (`setvbuf` with `_IOLBF`) is one system call per line too, and the Windows C library
treats it as full buffering; flushing on a timer, or only where the program waits, leaves the last line of a
program that computes without waiting in the buffer. A program that prints a great deal to a file and does not
need to be watched builds its text and prints it in fewer, longer lines.

Printing a value costs what the call says: the list of values
is a `List` like any variadic call's, a number is held in the list with its class and goes through its `to_string()`, and the text is written
with its length rather than up to its first zero byte. `conformance/stage6/text_building` and
`fused_chain_allocations` pin those allocations. A `crash` writes its operands itself, through each value's
`to_string()`, because it reports on the way out of a program that is stopping, after flushing what the program
printed before it.

**`Console.debug` and `to_debug()`.** Each class has an automatic `to_debug(): String` that returns something like
`Class {attribute: value, other: value}` by nesting `to_debug()`s. `debug(...values: List<Debuggable>)`, where `type Debuggable { to_debug():
String }`, writes each value's `to_debug()` separated by a space, then a line break. Every value answers it:

```text
Player { name: "hero", scores: [3, 7], bag: {"gold": 2}, partner: null, mood: 'calm' }
```

In detail:

- **A class shows its name and its attributes**, in declaration order, as `Name { attribute: value }`, and
  `Name {}` when it has none to show. An attribute that is a class is shown by *its* `to_debug()`, so a class that
  declares its own is shown by it wherever it appears. A `List` is `[a, b]`, a `Dictionary` is `{"key": value}`,
  text is quoted with `\"`, `\\` and `\n` escaped, a `Symbol` or enum value is written the way Spite writes it
  (`'calm'`), a number and a `Boolean` as they print, and an absent `T?` is `null`. A `Spite.Class` is its name.
- **Private attributes are left out.** The walk is `value.attributes.each(debug_attribute)` run from
  `Spite.DebugInstance` ([reflection.md](../docs/reflection.md#a-class-and-an-instance-of-it)), and a walk sees every
  attribute, private ones included, so `Spite.DebugInstance` skips each one whose name starts with `_`: it is the
  class's own business. The test is on a name known while compiling, so it folds, and a private attribute costs
  nothing in the text. The JSON and binary writers and readers skip them the same way, and attributes holding a
  singleton too ([json.md](../docs/json.md#what-each-type-becomes)); `to_debug()` shows those.
- **A cycle ends at an object already being shown**: it is written `Name {...}`, so `first.next.next` pointing
  back at `first` shows `Node { value: 1, next: Node { value: 2, next: Node {...} } }`. Each class keeps the
  objects it is in the middle of showing, compared with `==` (identity, unless the class defines `equals`), and
  a tree of distinct objects is shown whole however deep it goes.
- **It is Spite, and it costs nothing unused.** `library/spite/debug.spite` (`Spite.Debug<$value_type>`) turns any
  value into its text, and `library/spite/debug_instance.spite` (`Spite.DebugInstance<$value_type>`) walks a class
  instance. The generator's whole part is that asking a class for a `to_debug()` it does not declare answers one
  that calls them, created only when something asks, so a program that never debugs a class compiles nothing for
  it. A `type` or union that does not require `to_debug()` still answers it, dispatched to the class the value is.
- **The REPL keeps its own display** (`Player { name: hero, ... }`: text unquoted, nested objects as `Name {...}`,
  lists as `List<String>(...)`), which it builds from `Spite.Attribute`'s `.value` through `value.to_string()`,
  and the documented sessions depend on it.
- **What it costs**: in a program that calls `debug`, one generated `to_debug()` per class it shows, which
  builds the text as `String`s; the walk is compiled once per attribute, so each `attribute.value` is a typed read
  of the field and nothing is boxed. Every attribute it shows counts as read.

`conformance/stage6/debug_values`.

**A directory is navigated through its entries.** `Directory` has a `path` exactly as `File` does, and `entries()` answers a `List<Directory.Entry>`, where
`Directory.Entry` is the union of `Directory` and `File` declared in `library/directory.spite`. Each entry's `path`
is its parent's joined with its name, so a `switch` tells the two apart and a folder is walked by calling the same
function on it again ([Walk a directory tree](../docs/standard_library.md#walk-a-directory-tree); `conformance/stage4/directory_entries`).

In detail:

- **The name is `Entry`**, namespaced as `Directory.Entry`, because it is what a directory listing calls each of
  its items and it says nothing the class does not: `DirectoryEntry` would repeat the class it already lives in,
  and `Path` would claim a text value it is not.
- **Folders come first, then files, each sorted by name**; `.` and `..` are never listed. Each kind's names
  are sorted before its values are made.
- Each operating system's folder lists a directory its own way, through `entry_names(want_folders)`, since
  `entries()` is the public listing and Spite has no overloading.
- `--final-classes` prints `Directory` back out with its `union Entry`, which compiles because a union declared
  again replaces the earlier one ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)).

**`Socket` does what a game server needs, on every system.**
`library/socket.spite` holds everything but the calls into each system's library, which `library/windows/`,
`linux/` and `mac/socket.spite` reopen the class with:

- **Addresses.** `listen_locally` and `connect_locally` build `127.0.0.1` themselves, and `listen_everywhere`
  builds `::` on a socket that takes IPv4 connections too (`IPV6_V6ONLY` off), or `0.0.0.0` where the system has
  no IPv6. `listen_at` and `connect` resolve the host with the system's `getaddrinfo`, asking for any family and a
  stream socket, and put the IPv4 answers first, in the order the system gives them, then the IPv6 ones: `listen_at`
  listens on the first, and `connect` tries them in turn until one connects. A name that does not resolve answers `false`. Resolving waits in place, like
  connecting. The listening queue is 64 connections deep. `UdpSocket` uses the same addresses: `open` and
  `open_everywhere` take both families on one IPv6 socket, sending to an IPv4 address as `::ffff:` and the address,
  and a sender's address is written as RFC 5952 gives it, with an IPv4 address inside `::ffff:` written as IPv4 and
  a link-local one's scope after a `%`.
- **Waiting calls.** `accept_client`, `read_line` and `read_bytes` reach `Socket.accept_handle` and
  `Socket.receive_into`, and `HttpServer.next_request` reaches `Socket.first_readable` (the system's `poll`, or
  `WSAPoll`, over the listener and every kept connection, for at most `idle_limit` or one second), which are the
  compiler's waits ([concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting)):
  in a `Concurrent` they return to the event loop while a helper thread makes the call. `write_line` and
  `write_bytes` send until everything is sent, in place, as before.
- **Calls that never wait** (`_now`) are ordinary calls, not waits, so they add nothing to a state machine. Linux
  and macOS pass `MSG_DONTWAIT` to `recv` and `send` and ask `poll` with a timeout of 0 before `accept`, so a socket
  stays in blocking mode and the waiting calls keep working on it. Windows has no such flag: a socket switches to
  non-blocking mode (`ioctlsocket` with `FIONBIO`) the first time a `_now` call reaches it and back when a waiting
  call does, so a loop that only polls pays for one switch, not one per call. A client that `accept_client_now`
  hands back is in blocking mode, like every new `Socket`. "Nothing yet" is `EWOULDBLOCK` (`WSAEWOULDBLOCK` on
  Windows, `EAGAIN` or `EINTR` elsewhere, read through `__errno_location` or `__error`); any other failure, or a
  read of 0 bytes, is the end of the connection.
- **`closed`** is an attribute: `false` from `listen_*` or `connect*`, `true` once a read sees the end of the
  connection or a failure, once a write fails, or after `close()`. A read on a closed socket answers an empty list or `null`
  without calling the system, a write sends nothing, and nothing crashes. `read_line` answers
  `null` for a closed connection, and sets `closed`.
- **Lines and bytes mix.** Text `read_line` read past its line waits in the socket, and `read_bytes` and
  `read_bytes_now` hand those bytes out first.
- **What it costs**: a `Boolean` per `Socket` (and on Windows one more for the mode), and the functions a
  program calls: one that never polls has no `_now` code, and a system function only an unused call reaches
  (`getaddrinfo`, `ioctlsocket`, ...) is never looked up. `conformance/stage6/socket_bytes` (a server and a client in one
  program, bytes both ways and a line without waiting, then the close) and `socket_waits` (the waiting calls,
  inside a `Concurrent`) are the proof.

### Bytes are a List<Byte>

Bytes a program reads or writes are a `List<Byte>`; no program-facing function takes or answers a `Memory.Address`
for them ([Numbers in bytes](../docs/standard_library.md#numbers-in-bytes)).

- **Files.** `read_bytes()` answers the whole file, `null` when it cannot be opened, when its size cannot be read, or
  when fewer bytes arrive than its size said; an empty file is an empty list. A file larger than a `List` holds (more
  than 2 147 483 647 bytes) halts, as any `Long` cut to an `Integer` does, so read it with `read_bytes_at` or `map()`.
  `read_bytes_at(position, count)` answers up to `count` bytes from `position`: fewer when the file ends first, an
  empty list at or past the end, `null` when it cannot be opened. A negative `position` or `count` halts, since it is
  the program's own mistake. `write_bytes(bytes)` replaces the content and answers whether every byte was written;
  `append_bytes(bytes)` answers the position the bytes start at, `null` when the file cannot be opened or not every
  byte was written. Each call opens and closes the file.
- **Sockets.** `read_bytes()` waits until at least one byte has arrived and answers a fresh list of what arrived (up
  to 65 536 bytes at once), `null` once the connection is closed; `read_bytes_now()` answers what has arrived, an empty
  list for nothing yet and for a closed connection, which `closed` tells apart. Bytes `read_line` read past its line are
  handed out first. `write_bytes(bytes)` sends all of it, waiting; `write_bytes_now(bytes)` hands the system what it
  takes now and answers how many. A `Socket` keeps one 64 KB block it receives into, made on the first read and freed
  by `close()`, and each read copies what arrived into its own list: one allocation per read.
- **Numbers.** On a `List<Byte>`, `read_<number>(position)` answers the number whose bytes start at `position`, `null`
  unless `position >= 0` and `position + width <= count()`; the `_big_endian` twin reads the bytes most significant
  first. `<number>` and its width: `tiny` 1, `short` and `unsigned_short` 2, `integer`, `unsigned_integer` and `float`
  4, `long`, `unsigned_long` and `double` 8; `tiny` has no twin. `append_<number>(value)` adds the bytes at the end;
  `write_<number>(position, value)` replaces them and halts unless all of them are in the list, since the position is
  the program's. `append_bytes(other, start, count)` copies a run of another list (or the same one) to the end and
  halts unless `start >= 0`, `count >= 0` and `start + count <= other.count()`. A float is written and read as its
  IEEE 754 bits, so a `not a number` keeps its bits.
- **A read the compiler proves** answers the number itself, not a `T?`: under a bound that covers its last byte
  ([proofs.md](../docs/proofs.md#a-proven-count-or-bound-proves-a-read-of-a-width)), or inside a counted loop whose
  window covers it, or at `row * stride + column` under one guard
  ([proofs.md](../docs/proofs.md#an-index-built-from-loop-counters-is-proven-by-one-guard)). Inside a counted loop it
  is one load (and one byte swap for a big-endian read) with no test. Elsewhere the list could have changed size by
  another name since the bound was proven, so the read keeps one compare the C compiler is told is never taken
  (two when nothing proves the position is not negative), and when it fails the program halts with `spite:
  'bytes.read_integer(body + 4)' is outside its list: a bound proves only the top of an index, and this one is below
  0 or the list changed, at file:line`. A proven `write_<number>` in a counted loop is one store; elsewhere it is the
  library's checked write.
- **ForeignBytes.** `ForeignBytes(address, count)` halts when `count` is negative or `address` is `0` with a
  `count` above `0`. Its reads answer `null` and its writes halt outside `0` to `count`, exactly as a `List<Byte>`'s
  do; `read_bytes(position, count)` answers a fresh list, and `write_bytes(position, bytes)` halts unless every byte
  fits. It never frees the memory, and reading after the library has taken the memory back is the library's fault,
  reported as a native fault.
- **The address forms are the library's.** A program that writes them gets an error naming the plain form:
  - `file.read_bytes(position, count, address)`: `a File reads bytes into a List<Byte>, not into memory at an address:
    'read_bytes_at(position, count)' answers a 'List<Byte>?' of up to 'count' bytes from 'position', and
    'read_bytes()' the whole file`
  - `file.write_bytes(address, count)` and `file.append_bytes(address, count)`: `a File writes bytes from a
    List<Byte>, not from memory at an address: write 'write_bytes(bytes)'` (`'append_bytes(bytes)'` for the other)
  - `socket.read_bytes(address, count)`, and the library's own `read_bytes_into(address, count)`: `a Socket reads
    bytes into a List<Byte>, not into memory at an address: write 'read_bytes()', which answers a 'List<Byte>?'`
  - `socket.read_bytes_now(address, count)` and `read_bytes_now_into`: `a Socket reads bytes into a List<Byte>, not
    into memory at an address: write 'read_bytes_now()', which answers a 'List<Byte>'`
  - `socket.write_bytes(address, count)`, `write_bytes_now(address, count)` and the library's `write_bytes_from` and
    `write_bytes_now_from`: `a Socket writes bytes from a List<Byte>, not from memory at an address: write
    'write_bytes(bytes)'` (`'write_bytes_now(bytes)'` for the ones that never wait)
  - `reader.read_memory(address, count)` on a `BinaryReader`: `a BinaryReader reads from a List<Byte>, not from
    memory at an address: write 'read(bytes)', or 'read_from(bytes, start)' to read one value and leave what follows`

  The files of `library/` call `read_bytes_into`, `read_bytes_now_into`, `write_bytes_from` and `write_bytes_now_from`
  where HTTP and WebSocket read into a block of their own. `diagnostics/address_forms`, `diagnostics/byte_reads`,
  `conformance/stage6/list_byte_numbers`, `conformance/stage6/binary_files`, `socket_bytes` and `socket_waits`.
- **What it costs.** Each function is an ordinary member of `List`, `File`, `Socket` or `ForeignBytes`, so a program
  that reads no bytes carries none of them, and `ForeignBytes` is not in a program that makes none.

**`WebSocket` is RFC 6455 written in Spite over `Socket`.** `library/web_socket.spite` holds all of it, with
`WebSocketText` and `WebSocketBinary` beside it:

- **The handshake.** `accept` takes a `GET` over HTTP/1.1 whose `upgrade` is `websocket`, whose `connection` lists
  `upgrade` (any case, among other tokens), whose `sec-websocket-version` is `13` and whose `sec-websocket-key` is
  16 bytes in base64; it answers with `sec-websocket-accept`, the key and the protocol's own suffix hashed with
  SHA-1 and written in base64. SHA-1 is private to the class: it is the protocol's checksum, not a hash to store
  anything with. A server picks no subprotocol and no extension. `HttpServer` has already let go of the
  connection: `next_request()` keeps a connection again only when `respond` is called, so a request handed to
  `accept` is never answered twice. `connect` sends a fresh key of 16 bytes from `SecureRandom`, `host` with the
  port when it is not 80, and every header of the request; a request that is not a `GET`, has a body, or sets
  `host`, `upgrade`, `connection`, `content-length` or a `sec-websocket-` header other than
  `sec-websocket-protocol` halts, since the handshake writes those itself. The answer must be
  `HTTP/1.1 101` with the right `sec-websocket-accept`, no extension, and a subprotocol only if it is one the
  request offered. Calling `accept` or `connect` a second time on one `WebSocket` halts.
- **Frames.** A message goes out as one frame, its length in 7, 16 or 64 bits as the protocol asks. A client masks
  every frame with 4 bytes from `SecureRandom`; a server masks none. Reading, a frame is refused (`1002`) when a
  reserved bit is set, its opcode is not continuation, text, binary, close, ping or pong, it is masked the wrong
  way for its side, its length is not written in the fewest bytes or sets the top bit of 64, or it is a control
  frame longer than 125 bytes or in pieces; a continuation with no message started, or a new message before the
  last one ended, is `1002` too. A message whose pieces would pass `largest_message` is `1009` before its payload is
  read. Text is checked as UTF-8 once whole (`1007`).
- **Pings, pongs and closes.** A ping is answered with a pong carrying the same bytes, unless this side has sent
  its close. A pong is kept in `last_pong`. A close's code must be 1000 to 1003, 1007 to 1014 or 3000 to 4999, and
  its reason UTF-8; a close of one byte is `1002`. Answering a close sends the same code back (or a close with no
  code for a close with none) and closes the connection. Before closing the connection, either way, `WebSocket`
  reads away what has already arrived, so the system does not reset the connection and throw away the close
  frame it just sent.
- **`drop()`** of a `WebSocket` still open sends a close with `1001` and closes the connection.
- **What it costs**: nothing for a program that makes no `WebSocket`; for one that does, a frame header and a copy
  of each message to send, one read of the system per frame piece, and 4 bytes from the operating system's
  secure source per frame a client sends. `conformance/stage6/web_socket_exchange` (a server and a client in one
  program: text, binary of each length size, an empty message, a ping, a close, a refused plain request, and a
  hand-made client sending a message in pieces with a ping between them) and `web_socket_failures` (every close
  code above, a refused version and a client of a server that does not speak WebSocket) are the proof.

### Pure Spite: dissolving the runtime

**The standard library is Spite, not hand-written C, and what the compiler supplies below it is a short list of
operations, not a file someone maintains.** `Spite.Class` is an ordinary standard library class with an ordinary
declaration, which cannot be true of a library written in C. The target is **zero hidden code**: everything the
compiler supplies ends up as explicit Spite, and nothing ties Spite to C; what cannot be Spite, the reads, writes
and atomics at an address, are language primitives each backend lowers.

- **Purity costs no performance**: the output is still C, so a Spite-written `trim()` goes through the same
  optimiser a hand-written one would. It is the same machine code from a better source.
- **Library classes stop being special**: they are reopenable like any class, and an inspectable build shows
  every one of them to the REPL, while a production build carries only those the program reaches.

**What is Spite**, in `library/`:

| Part | How |
|---|---|
| `String`, `List<T>`, `Dictionary<Key, Value>`, number to text (`library/number_text.spite`) | over `Memory.Heap` and `TypedMemory<T>`. The syntax stays the compiler's (`[]`, list literals, `List<T>()`), mapped onto these classes' functions, and per element type the compiler still writes four one-line functions a generic class cannot, plus `deep_copy`; the member templates are Spite in `library/list.spite` ([collections.md](collections.md#standard-library-metaprogramming)) |
| `File`, `Directory`, `Process`, `Program`, `Console.read_line`, `Clock`, `Socket`, `FileSystemWatcher`, `ThreadPool`, `Lock`, `ThreadSlot`, the time-zone database | written once in `library/`; each operating system's folder reopens the class with the few functions that call its own library (`ucrtbase.dll`/`kernel32.dll`, `libc.so.6`, `libSystem.dylib`) through `DynamicLibrary` |
| `--debug-memory`'s live table | `AllocationTable` (`library/allocation_table.spite`), an open-addressing set of live addresses with each object's class id beside it, which prints the leak report; the compiler emits only the small functions `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` call under `--debug-memory` |
| the REPL, live reload, the event loop | `ReadEvaluatePrintLoop`, `HotReload` and `Scheduler`, over reflection tables the compiler generates |
| printing, `to_debug()`, JSON and bytes | `Console`, `Spite.Debug<T>`, `Spite.DebugInstance<T>`, `JsonWriter<T>`, `JsonReader<T>`, `BinaryWriter<T>` and `BinaryReader<T>` ([json.md](../docs/json.md)) |

**What is the compiler's**: the floor, and the functions it supplies below it:

1. **`Memory.Address`'s reads, writes and atomics** are language primitives, each one machine operation the
   backend writes where it is called, like `+` ([memory.md](../docs/memory.md#memory-is-the-floor-and-you-can-build-on-it)).
   This is the floor that stays.
2. **Supplied bodies**: functions declared without a body whose C the compiler writes
   (`bootstrap/source/generation/prelude.spite`): `Console`'s `_write_output`, `_write_error`, `_write_held` and
   `_flush`, and `Program`'s and `Process`'s `_flush_output` (`fwrite`, `fflush`); `Memory.Heap`'s `allocate`, `resize`, `free` and `live_allocations` (the C library's
   `malloc`, or the `--debug-memory` table) and `Memory.Address`'s `copy_to` and `compare_bytes` (`memmove`,
   `memcmp`), `text(length)`, `String.sum` and `String.code_at` (which depend on whether text fits inside the
   `String`);
   `TypedMemory<T>`; the number classes' bit operations; `DynamicLibrary`'s opening, closing and
   symbol lookup; the entry points and frames of `Concurrent`, `ThreadPool` and `Scheduler`; `HotReload`'s
   compiler hand-off; and `Spite.Attribute`'s and `Spite.Function`'s dispatch.
3. **The object header**: retain, release and the class id are code the compiler emits, not functions anyone
   calls. For `String` that includes the two decisions only the header can make: a `'constant'` text is never
   counted, and `text = text + piece` grows the text in place only when its block's count is one, or inside the
   `String` itself while it is 15 bytes or fewer.
4. **Entry and exit**: `main`, and text literals, which are static data in the `'constant'` section, as symbols
   are.

**WebAssembly.** There, `Memory.Heap`'s `allocate` grows linear memory with `memory.grow`, and the
C runtime library is the JavaScript host's. **The web shim is the one honest exception, and its target is zero
hand-written lines.** A file that runs in the JavaScript virtual machine cannot be Spite by construction: it is
on the far side of a boundary Spite does not own. What is achievable is that nobody writes it: the imports come
from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both data the
compiler can emit, so the source of truth stays Spite and nothing in the repository is written in a second
language ([targets.md](../docs/targets.md)).

### examples/calculator

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop that passes each character and its index on (so it
is not a loop a member template replaces), keeping the current token's start as an attribute and slicing
it out on a token boundary; the parser is recursive, including precedence climbing written as tail recursion
(`parse_expression_rest`/`parse_term_rest`). The parser makes each `BinaryExpression(...)` on a line
of its own and hands it on by name, since no constructor is an argument:
`left_expression`/`right_expression` are ordinary by-reference parameters, stored directly into the
union-typed attribute.

---

Next: [Lists, dictionaries and member templates](collections.md).
