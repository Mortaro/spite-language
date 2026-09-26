# JSON and binary

Four classes turn any value into something to send or store, and back: `JsonWriter` and `JsonReader` for JSON
text, `BinaryWriter` and `BinaryReader` for compact bytes. None of them is a serialiser written per type. Each is
one generic class in `library/`, written with Spite's own metaprogramming, and the compiler makes the functions a
type needs only when a program uses one with it. A program that never names them carries none of it.

A writer takes the value, so writing names no type: `JsonWriter(order)`, then `write()`. A reader has no value
yet, so it names the class it makes and takes what to read it from: `JsonReader<Order>(text)`, then `read()`,
which answers `Order?` -- `null` when the input is not an `Order`.

Which one to use: **binary between Spite programs, JSON for everything else.** Two Spite programs compile from
the same source, so both ends already know every type and nothing has to describe itself; the bytes are about a
quarter the size of the JSON, more than ten times faster to write and twenty-five times faster to read
([measured below](#how-fast-and-how-small)). JSON is for talking to systems that are not Spite
([The wire format](targets.md#the-wire-format--planned)).

## Write and read a class

```gdscript title=json_basics/order.spite
enum Status {
    'open'
    'shipped'
}

var id = 0
var customer = ""
var total = 0.0
var status: Status = 'open'
var items = List<Item>()
var note: String? = null
```
```gdscript title=json_basics/item.spite
var name = ""
var count = 1

func Item(starting_name: String, starting_count: Integer) {
    name = starting_name
    count = starting_count
}
```
```gdscript title=json_basics/json_basics.spite entry
var console = Console()

func JsonBasics() {
    var order = Order()
    order.id = 7
    order.customer = "Ada \"the\" first"
    order.total = 12.5
    order.status = 'shipped'
    var tea = Item("tea", 2)
    order.items.append(tea)
    var writer = JsonWriter(order)
    var text = writer.write()
    console.print(text)
    var reader = JsonReader<Order>(text)
    var read = reader.read()
    crash read
    var first_item = read.items.first()
    crash first_item
    console.print(read.customer, read.status, first_item.name, first_item.count)
}
```
```output
{"id":7,"customer":"Ada \"the\" first","total":12.5,"status":"shipped","items":[{"name":"tea","count":2}],"note":null}
Ada "the" first shipped tea 2
```

- `JsonWriter(value)` is `JsonWriter<T>` for the value's type, read from the argument like any generic class whose
  constructor takes its `$T` ([metaprogramming.md](metaprogramming.md#generics-and-codegen-values-)). A `T?`
  gives `JsonWriter<T>`, and writes `null` when it is empty.
- `write(): String` answers the text: every attribute has a type the compiler knows. Attributes are written in
  the order the class declares them. The one thing it cannot write is a `Float` or `Double` that is infinity or
  not-a-number, which JSON cannot hold, and there it crashes naming the attribute and the value (D198, below).
- `JsonReader<T>(text)` holds the text, and `read(): T?` answers `null` when it is not JSON, or not this class's
  JSON. Narrow it like any other `T?`: `crash` when bad input is a bug, `assert` when the program should carry on,
  `if` when absence is a case.
- `read_or_crash(): T` halts on bad input instead, and the crash line says where and what was expected:
  `failure=expected a number at character 10`.

## Write and read bytes

`BinaryWriter` and `BinaryReader` have the same shape. The bytes are a `Vector<Byte>`, held inline in one block
([collections.md](collections.md#vectort-items-inline)):

```gdscript title=binary_basics/order.spite
enum Status {
    'open'
    'shipped'
}

var id = 0
var customer = ""
var total = 0.0
var status: Status = 'open'
var items = List<Item>()
var note: String? = null
```
```gdscript title=binary_basics/item.spite
var name = ""
var count = 1

func Item(starting_name: String, starting_count: Integer) {
    name = starting_name
    count = starting_count
}
```
```gdscript title=binary_basics/binary_basics.spite entry
var console = Console()

func BinaryBasics() {
    var order = Order()
    order.id = 7
    order.customer = "Ada"
    order.total = 12.5
    order.status = 'shipped'
    var tea = Item("tea", 2)
    order.items.append(tea)
    var writer = BinaryWriter(order)
    var bytes = writer.write()
    var size = bytes.count()
    console.print("bytes:", size)
    var reader = BinaryReader<Order>(bytes)
    var read = reader.read()
    crash read
    var first_item = read.items.first()
    crash first_item
    console.print(read.customer, read.status, first_item.name, first_item.count)
}
```
```output
bytes: 23
Ada shipped tea 2
```

The same `Order` is 23 bytes here and 104 as JSON: `id` and `total` (a `Float`) are four bytes each, `status` one,
`"Ada"` a length byte and three, the empty `note` one, and there are no keys, quotes or commas at all. [The binary format](#the-binary-format)
says exactly what each type becomes.

- `write(): Vector<Byte>` answers a new vector holding the value. `append_to(bytes)` writes it at the end of a
  vector the program already has instead, so many values -- a network frame's worth of messages, a file's records
  -- go into one buffer with no copying.
- `BinaryReader<T>(bytes)` reads from a `Vector<Byte>`. Each `read(): T?` reads the next value from `position`
  (an attribute, starting at `0`) and moves past it, so values written one after another are read one after
  another; `remaining()` is how many bytes are left. It answers `null` when there is nothing left, when the bytes
  run out in the middle of a value, and when they are not a `T` (a `Boolean` byte that is neither 0 nor 1, an enum
  index the enum does not have, a count larger than the bytes left). A failed read leaves `position` where it
  was.
- `read_memory(address, count): T?` reads one value from `count` bytes at a `Memory.Address` -- a buffer a
  `Socket` filled, say -- without copying them into a vector first; the reader is made with `null` for its bytes.
  Afterwards `position` is how many of the bytes the value took.

```gdscript
var received = connection.read_bytes_now(buffer, 4096)
var reader = BinaryReader<Move>(null)
var move = reader.read_memory(buffer, received)
```

Infinity and not-a-number are ordinary bytes: only JSON cannot hold them (D208), so `BinaryWriter` never crashes.

### Several values in one buffer

```gdscript title=binary_stream/binary_stream.spite entry
var console = Console()

func BinaryStream() {
    var bytes = Vector<Byte>()
    var names = ["ada", "bo", "cy"]
    var name_writer = BinaryWriter(names)
    name_writer.append_to(bytes)
    var turn = 12
    var turn_writer = BinaryWriter(turn)
    turn_writer.append_to(bytes)
    var size = bytes.count()
    console.print("bytes:", size)
    var name_reader = BinaryReader<List<String>>(bytes)
    var read_names = name_reader.read()
    crash read_names
    var turn_reader = BinaryReader<Integer>(bytes)
    turn_reader.position = name_reader.position
    var read_turn = turn_reader.read()
    crash read_turn
    var left = turn_reader.remaining()
    var joined = read_names.join(" ")
    console.print(joined, read_turn, left)
}
```
```output
bytes: 15
ada bo cy 12 0
```

A reader reads one type; to read a different one from the same bytes, a second reader starts at the first one's
`position`. A header is written the same way, with a writer of its own ahead of the value.

## What each type becomes

| Spite | JSON | Binary |
|---|---|---|
| `String` | text, with `"`, `\`, and control characters escaped | its length in bytes as a count, then its UTF-8 bytes |
| `Tiny`, `Byte` | a number | 1 byte |
| `Short`, `UnsignedShort` | a number | 2 bytes, little-endian |
| `Integer`, `UnsignedInteger`, `Float` | a number | 4 bytes, little-endian (`Float` as its IEEE 754 bits) |
| `Long`, `UnsignedLong`, `Double` | a number | 8 bytes, little-endian (`Double` as its IEEE 754 bits) |
| `Boolean` | `true` or `false` | 1 byte, `1` or `0` |
| an enum | its value's name, as text | its index in the enum, in 1 byte for up to 256 values, 2 up to 65 536, else 4 |
| a `Symbol` | its name, as text; read back only as a name the program already uses as a `Symbol` | its name, as a `String` is written; read back the same way |
| a class | an object, one key per attribute | its attributes, one after another in declaration order, with nothing around them |
| `List<T>` | an array | its count as a count, then each element |
| `Dictionary<T>` | an object, one key per entry | its count as a count, then each key as a `String` and its value |
| `T?` | `null`, or what `T` becomes | 1 byte, `0` for empty or `1` followed by what `T` becomes |

A **count** is an unsigned LEB128 varint: seven bits per byte, low bits first, the top bit set on every byte but
the last, so a count below 128 is one byte.

Anything else a class holds -- another class, a list of lists, a dictionary of classes -- is one of these again,
so each writer and reader goes as deep as the class does. A `union`, a `type` such as `Anything`, or a function
value anywhere in what they see has no form in either, and making one is a compile error at your line, naming the
attribute: `JsonWriter cannot write 'Owner': 'Owner.pet' is the union Pet, and JSON does not say which member a
value is`, or `BinaryReader cannot read 'Owner': 'Owner.pet' is the union Pet, and the bytes do not say which
member a value is` (`diagnostics/json_unwritable`). Keep what they see to the types in this table.

**Private attributes are left out** by both formats: an attribute whose name starts with `_` is the class's own
business ([style.md](style.md)), as `to_debug()` leaves it out, so it is neither written nor read and keeps its
default. It is how a class chooses which of its fields travel; a component that sends only some of its state keeps
the rest in `_` attributes, or the program sends a smaller class that holds only what travels.

## The binary format

The table above is the whole format, and it is stable: it depends only on the class's attributes, their order and
their types, never on the machine, the compiler's version or the build, so bytes written today read back
tomorrow, on another machine, from a program built another way. What changes it is changing the class -- adding,
removing, reordering or retyping an attribute, or reordering an enum's values -- and then both ends must be built
from the new source, since nothing in the bytes says which class they are or what it held.

- **No header, no keys, no padding.** The bytes are the value and nothing else, so values can follow each other in
  one buffer and a program frames them however its transport needs.
- **Little-endian everywhere.** It is the order of every machine Spite compiles for (x86-64 and ARM64, and
  WebAssembly when that target is built), so a number is one store and one load with no swapping. A big-endian
  target would have to swap each number's bytes in `BinaryOutput` and `BinaryInput`; the format would not change.
- **Reading never trusts a count.** A text length or an element count larger than the bytes left fails the read
  before anything is allocated, so a corrupt or hostile buffer cannot make a reader allocate gigabytes.

**A schema hash** (proposed by Claude, unconfirmed; not built). D31 asks for one in the handshake, so an old client
meeting a new server fails with a clear report instead of misreading bytes. It belongs to the handshake or the
file header, not to every value: a value's bytes stay bare so that thousands of them can share one buffer. The
proposal is `BinaryWriter<T>.schema(): Long`, a hash of the attribute walk -- each attribute's name and type,
recursively, and each enum's values in order -- computed while compiling, so it costs one constant; a program
writes it once at the start of a file or a connection and compares it before reading. It is not built because
nothing in the language computes a value from a type walk at compile time yet, and computing it at run time would
walk the types on every call.

## Reading input you did not write

Text from another system rarely matches a class exactly. `JsonReader` is strict about what it cannot use and
lenient about what it does not need:

- **A key the class does not have is skipped**, whatever its value.
- **An attribute the text does not mention keeps its default**, the value the class declares for it.
- **A value of the wrong kind makes the whole read `null`**: text where a number belongs, `null` for an
  attribute that is not a `T?`, an enum name the enum does not have.

```gdscript title=json_input/reading.spite
var name = "unnamed"
var level = 1
var tags = List<String>()
```
```gdscript title=json_input/json_input.spite entry
var console = Console()

func JsonInput() {
    var partial_reader = JsonReader<Reading>("\{ \"name\": \"probe\", \"vendor\": \{ \"id\": [1, 2] } }")
    var partial = partial_reader.read()
    crash partial
    var tag_count = partial.tags.count()
    console.print(partial.name, partial.level, tag_count)
    var mistyped_reader = JsonReader<Reading>("\{\"level\": \"high\"}")
    var mistyped = mistyped_reader.read()
    if mistyped {
        console.print("unexpected")
    } else {
        console.print("level must be a number")
    }
    var broken_reader = JsonReader<Reading>("\{\"name\": \"probe\"")
    var broken = broken_reader.read()
    if broken {
        console.print("unexpected")
    } else {
        console.print("unclosed object")
    }
}
```
```output
probe 1 0
level must be a number
unclosed object
```

Bytes have no keys to skip, so `BinaryReader` is strict throughout: the bytes are exactly the class, or the read
is `null`.

## Values that are not classes

All four take any type, not only classes: `JsonWriter(scores)` for a `Dictionary<Integer>`,
`JsonReader<List<Double>>(text)` to read a list, `BinaryWriter(12)`.

```gdscript title=json_values/json_values.spite entry
var console = Console()

func JsonValues() {
    var scores = Dictionary<Integer>()
    scores.set("ada", 3)
    scores.set("bo", 5)
    var writer = JsonWriter(scores)
    var text = writer.write()
    console.print(text)
    var numbers = JsonReader<List<Double>>("[1.5, -2, 3e2]")
    var read = numbers.read()
    crash read
    var joined = read.join(" ")
    console.print(joined)
}
```
```output
{"ada":3,"bo":5}
1.5 -2 300
```

## How fast and how small

`benchmarks/serialisation` writes 100 000 small objects -- an `Integer` id, a short name, two `Float`s, a `Short`,
a `Boolean` and an enum -- and reads them back, as JSON (one text each) and as binary (all appended to one
`Vector<Byte>`). `clang -O2`, on Mortaro's Windows machine, with the allocations `--debug-memory` counts for the
100 000 writes and reads:

| | size | write | read | allocations |
|---|---|---|---|---|
| JSON | 9 380 963 bytes (94 per object) | 217 ms | 238 ms | 9 347 676 |
| binary | 2 389 000 bytes (24 per object) | 16 ms | 8 ms | 300 010 |

JSON's cost is making and parsing text: every number is formatted and parsed, every key written and matched, and
every member is a `String` of its own. The binary writer stores each number with one instruction and reads it
with one, and its walk makes no object at all; what it allocates is the writer and its cursor for each value, and
each value read back.

## How it is written

All four are ordinary Spite, and reading them is the best way to learn the two metaprogramming forms they rest on
([metaprogramming.md](metaprogramming.md)):

- `if $value_type == List { }` asks at compile time what `T` is, and `JsonWriter<$value_type.element_type>(null)`
  makes the writer for its elements, once per list: each element is given to it in turn as its `value`. Only the
  branch that fits `T` is compiled.
- A class is written by a Symbol codegen template that ranges over `T`'s attributes, called for every one of them
  at once by its plural:

```gdscript
func write_attribute(attribute: Symbol<$value_type>, shown: $value_type, members: List<String>) {
    var attribute_json = JsonWriter(shown.attributes[attribute])
    var attribute_text = attribute_json.write()
    members.append("\"{attribute.name}\":{attribute_text}")
}
```

`write_attributes(shown, members)` calls it once per attribute; `read_attributes` in `JsonReader` does the same
with the key it just read, and the attribute whose name matches takes the value. `--final-classes` does not print
these instances (a generic class's file is shared by all of its instances), but they are ordinary typed
functions in the C the program compiles to.

The binary pair keeps the walk in `BinaryFormat<T>` (`library/binary_format.spite`), a singleton that holds
nothing, so it is a static object and the walk makes no object at all; its `write_attribute` is the template above
with `BinaryFormat<attribute.class>()` in place of the `JsonWriter`. An enum is walked the same way, over its
values (`find_value(value: Symbol<$value_type>, ...)`), to find its index. `BinaryOutput` and `BinaryInput` are the
cursors over the bytes. Reading JSON text goes through `JsonCursor` (`library/json_cursor.spite`), a cursor over the
text that remembers the first thing that went wrong.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### JSON is reflection, not a library  **[implemented]**

D22 and D23 (decided by Mortaro, 2026-09-19), amended by D31: writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape, and the JSON pair is written with the language's own metaprogramming in `library/`, like the rest of the
standard library ([Pure Spite](standard_library.md#pure-spite-dissolving-the-runtime--partial)). JSON is a format
for talking to foreign systems; it is not what Spite programs send each other
([The wire format](targets.md#the-wire-format--planned)).

- **Writing never fails at run time**: a class that cannot be serialised is a compile error, the same check
  isomorphic classes need. Only parsing can fail, and it follows the failure model: a parse that may not succeed
  returns a `T?`, and a variant that crashes exists for callers who want the report instead. The compile-time
  check (built as proposed by Claude, unconfirmed): when a program's line makes a `JsonWriter<T>`,
  `JsonReader<T>`, `BinaryWriter<T>` or `BinaryReader<T>`, the compiler walks `T` -- through `T?`, list
  elements, dictionary values and every attribute of every class it reaches -- and a `union`, a `type`
  (`Anything` included) or a function value is an error at that line naming the attribute path; the rest of the
  four classes' files is then not reported on, so one mistake is one error (`diagnostics/json_unwritable`).
  Compile time only. The error names the class and what it does: `<class> cannot write '<T>'` for a writer,
  `cannot read` for a reader, and the reason ends `JSON does not say which member a value is` or `the bytes do not
  say which member a value is`, and `has no JSON form` or `has no binary form`.
- The one exception to "writing never fails" is D198's, below: a `Float` or `Double` JSON cannot hold.

**Four classes: `JsonWriter`, `JsonReader`, `BinaryWriter`, `BinaryReader`** (D208, decided by Mortaro,
2026-09-26, revising D138: "lets also split Json into a JsonWritter and a JsonReader it looks like it makes more
sense, but that also allows us to have a BinaryWritter and a BinaryReader that Slop can use for its network and to
serialize resources to disk").  **[implemented]** Writing takes the object, as D138 decided; reading names the
class it makes. Each class is generic, and every conversion it makes is a function the compiler writes from it for
the types a program actually uses, so a program that never names one carries none of it (D42, D177): no table of
types, no reflection and no registry exists at run time. At run time a conversion costs what its Spite says:
`JsonWriter.write()` makes one `String` per member and joins them, `JsonReader.read()` walks the text once through
one `JsonCursor`, and the binary pair writes and reads each number with one machine store or load.

```gdscript
var writer = JsonWriter(order)          # JsonWriter<Order>, read from the argument (D138)
var text = writer.write()               # String: never fails
var reader = JsonReader<Order>(text)    # no object yet, so the class is named
var read = reader.read()                # Order?: null on text that is not an Order
var order = reader.read_or_crash()      # Order: halts, naming the position and what was expected

var packer = BinaryWriter(order)        # BinaryWriter<Order>
var bytes = packer.write()              # Vector<Byte>: never fails
packer.append_to(frame)                 # the same bytes, at the end of a Vector<Byte> the program has
var unpacker = BinaryReader<Order>(bytes)
var next = unpacker.read()              # Order?: the next value from 'position', null on bad or missing bytes
var from_socket = unpacker.read_memory(buffer, received)
```

The names, as built (proposed by Claude, unconfirmed): the four class names are D208's, spelled `Writer`;
`write`, `read` and `read_or_crash` are the old `Json`'s; `append_to(bytes)` says where the bytes go ("append",
not "add", [SPITE.md](../SPITE.md)); `read_memory(address, count)` says what it reads from; `position` and
`remaining()` are the reader's cursor, as on the cursors the standard library already has. The old name is an
error that names the new pair: `there is no 'Json': it is two classes (D208), 'JsonWriter(value)' whose 'write()'
makes the text, and 'JsonReader<T>(text)' whose 'read()' answers a 'T?'` (`diagnostics/json_split`).

- **A writer takes the value in its constructor** (D138) and infers its generic from it, by the general rule of
  [Codegen values (`$`)](metaprogramming.md#codegen-values---implemented) for a constructor that takes a `$name`;
  nothing about the writers is special to the compiler. As built (proposed by Claude, unconfirmed): the constructor
  takes a `$value_type?`, so a `T?` argument gives a writer of `T`; `JsonWriter.write()` of an empty one is `null`,
  and `BinaryWriter` writes nothing for it (a `T?` inside a value has its presence byte; at the top the caller
  knows whether it wrote anything). `JsonWriter<Order>(null)` names the class when there is no value.
- **A reader names its class and takes its input in its constructor**: `JsonReader<T>(text: String)`,
  `BinaryReader<T>(bytes: Vector<Byte>?)`. `JsonReader.read()` reads the whole text as one value, every time it is
  called; `BinaryReader.read()` reads the next value from `position`, so a buffer of many values is read by
  calling it again.
- **What it is written with** is [Symbol codegen](metaprogramming.md#symbol-codegen--implemented) and
  [Codegen values (`$`)](metaprogramming.md#codegen-values---implemented) and nothing else, as
  [How it is written](#how-it-is-written) shows. The binary walk's helper, `BinaryFormat<T>`, is a `singleton`
  that holds nothing, bound where it is used (`BinaryFormat<attribute.class>()`), which D144 allows since a
  singleton made from a `$` or `.class` type cannot be an attribute; in a production build it is a static object
  ([optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **An enum is written by walking its values**, which a generic may do for the enum it is given
  ([values_and_types.md](values_and_types.md#enums--implemented)): that is how `BinaryFormat` finds an enum value's
  index and how many values the enum has, with no table at run time: writing one costs one comparison per value
  of the enum, and reading one a comparison per value and a text-to-enum cast.
- **What each type becomes** is [the table above](#what-each-type-becomes), applied again to whatever a value
  holds. JSON text escapes `"`, `\`, line feed, carriage return and tab as `\"`, `\\`, `\n`, `\r`, `\t` and every
  other control character as `\u00XX`, and reading turns any `\uXXXX` back into UTF-8, surrogate pairs included. A
  class's attributes are written in declaration order, leaving out private (`_`) ones as `to_debug()` does
  ([standard_library.md](standard_library.md#system-classes--implemented)); a `Dictionary`'s entries in the order
  they were set.
- **A walk reads every attribute**, so an attribute a program only ever hands to a writer counts as read, and the
  unused-attribute error does not fire for it ([D118](decisions.md), the ruling on walks).

What follows is Claude's reading where D22, D95 and D208 are not specific (proposed by Claude, unconfirmed):

- **The binary format** is [The binary format](#the-binary-format) and [the table](#what-each-type-becomes):
  little-endian fixed-width numbers, `Boolean` as one byte, an enum as its index in the smallest of 1, 2 or 4
  bytes that holds every index, text as a varint length and UTF-8, `T?` as a presence byte, lists and dictionaries
  as a varint count and the items, nested objects inline, and no header. D208 asked for exactly these; the varint
  is unsigned LEB128, and a count or length above what an `Integer` holds, or above the bytes left, fails the read.
- **The bytes are a `Vector<Byte>`** (D204's inline, uncounted items; D208 preferred it): one block, no count per
  byte, and a reader over it borrows nothing, since it keeps the vector itself and reads its block afresh at every
  `read()`. `read_memory` covers memory the program did not put in a vector. `Vector<T>` has `reserve(count)` for
  this, making room for `count` items without making any (proposed by Claude, unconfirmed).
- **`BinaryReader` answers `null` for every bad input** (D199: bad input is a normal condition, never a crash):
  bytes that end inside a value, a `Boolean` byte other than 0 or 1, a presence byte other than 0 or 1, an enum
  index past the enum's last value, a plain `Symbol` name the program does not use, and a count past the bytes
  left. A failed read leaves `position` where it was. There is no `read_or_crash` for bytes: the bytes a Spite
  program reads were written by a Spite program, and what went wrong is not something a position would explain.
- **The API is a class you make once per value, or once per input**, with `write` and `read`. The generic is on
  the class because that is where Spite puts generics. `write` needs no crashing twin, since it cannot fail.
- **`JsonReader` is strict about what it cannot use and lenient about what it does not need**
  ([Reading input you did not write](#reading-input-you-did-not-write), and
  [Failure](failure.md#failure-three-outcomes-and-no-others--partial)'s three outcomes). An unknown key is skipped,
  whatever it holds, because foreign systems send more than a program asks for; a missing attribute keeps the
  default its class declares, the value [Variables and values](values_and_types.md#variables-and-values--implemented)
  gives anything never set. Besides a value of the wrong kind, a missing brace and anything after the value
  (`{} x`) make `read` answer `null`: the object would be wrong, and a wrong object that looks right is the
  surprise D27 exists to prevent. `read_or_crash` crashes on the same inputs, and its crash line carries
  `failure=expected <what> at character <n>`.
- **A number reads into whatever number type the attribute has** through the ordinary text-to-number cast, so
  `3.7` read into an `Integer` is `3`; JSON has one number type and the class already says which one it wants.
- **Infinity and not-a-number crash `JsonWriter`** (D198, decided by Mortaro; D208 keeps it JSON's alone): JSON
  (RFC 8259) holds neither, and a float became one through a division by zero or an overflow the program did not
  guard, which is the developer's mistake (D199, [failure.md](failure.md)); floats themselves keep them (D200),
  and `BinaryWriter` writes their bits like any other. The crash names the attribute -- `Class.attribute`, with
  `[index]` or `["key"]` for an element -- and the value: `unwritable='Order.price' is infinity, which JSON cannot
  hold` (also `negative infinity` and `not a number`; `conformance/stage6/json_infinity`). A program that wants
  `null` there checks the number first.
- `--final-classes` does not print the functions `JsonWriter<Order>` generated, because a generic class's file
  is shared by all its instances.
- **A plain `Symbol` attribute** (as opposed to an enum) is written as its name and read back through
  `Symbol(text)` (D70), so a name the program does not already use as a symbol is a value of the wrong kind: `read`
  answers `null` (proposed by Claude, unconfirmed; `conformance/stage6/json_symbols`). The library tells the two
  apart with the codegen test `$value_type == Enum`, true for an enum only, where `$value_type == Symbol` is true
  for both (proposed by Claude, unconfirmed; [metaprogramming.md](metaprogramming.md)).

`tests/json_tests.spite`, `tests/binary_tests.spite`, `conformance/stage6/json_crash`, `docs/json.md`.
