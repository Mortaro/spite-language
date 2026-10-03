# JSON and binary

Four classes turn any value into something to send or store, and back: `JsonWriter` and `JsonReader` for JSON
text, `BinaryWriter` and `BinaryReader` for compact bytes. None of them is a serialiser written per type. Each is
one generic class in `library/`, written with Spite's own metaprogramming, and the compiler makes the functions a
type needs only when a program uses one with it. A program that never names them carries none of it.

A JSON serializer is made once, with no arguments, and used as often as needed: `JsonWriter<Order>()`, then
`write(order)` for each order to write, and `JsonReader<Order>()`, then `read(text)` for each text to read, which
answers `Order?`, `null` when the input is not an `Order`.

Which one to use: **binary between Spite programs, JSON for everything else.** Two Spite programs compile from
the same source, so both ends already know every type and nothing has to describe itself; the bytes are about a
quarter the size of the JSON, more than ten times faster to write and twenty-five times faster to read
([measured below](#how-fast-and-how-small)). JSON is for talking to systems that are not Spite
([The wire format](targets.md#the-wire-format)).

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
    var writer = JsonWriter<Order>()
    var text = writer.write(order)
    console.print(text)
    var reader = JsonReader<Order>()
    var read = reader.read(text)
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

- `JsonWriter<T>()` names the type it writes and takes nothing; `write(value: T?): String` answers the text of one
  value, and writes `null` for an empty `T?`. One writer writes any number of values, one call each: every
  attribute has a type the compiler knows. Attributes are written in
  the order the class declares them. The one thing it cannot write is a `Float` or `Double` that is infinity or
  not-a-number, which JSON cannot hold, and there it crashes naming the attribute and the value (see below).
- `JsonReader<T>()` names the type it makes and takes nothing, and `read(text: String): T?` answers `null` when the
  text is not JSON, or not this class's JSON; one reader reads any number of texts. Narrow it like any other `T?`: `crash` when bad input is a bug, `assert` when the program should carry on,
  `if` when absence is a case.
- `read_or_crash(text: String): T` halts on bad input instead, at the point the reader finds it, and the crash line shows what
  was wanted there, the position and the text: `wanted=a number	text={"count": three}	position=10`.

## Write and read bytes

`BinaryWriter` and `BinaryReader` have the same shape as the JSON pair: each names its type, is made once and is
reused, and each call carries its own input. The bytes are a `List<Byte>`, the bytes themselves in one
block ([collections.md](collections.md#listt)):

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
    var writer = BinaryWriter<Order>()
    var bytes = writer.write(order)
    var size = bytes.count()
    console.print("bytes:", size)
    var reader = BinaryReader<Order>()
    var read = reader.read(bytes)
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

- `write(value): List<Byte>` answers a new list holding the value. `append_to(value, bytes)` writes it at the
  end of a list the program already has instead, so many values (a network frame's worth of messages, a file's
  records) go into one buffer with no copying.
- `read(bytes): T?` reads one value that fills the bytes exactly. It answers `null` when the bytes run out in the
  middle of the value, when they are not a `T` (a `Boolean` byte that is neither 0 nor 1, an enum index the enum
  does not have, a count larger than the bytes left), and when bytes are left over after it, as JSON's `read`
  does with text after the value.
- `read_from(bytes, start): T?` reads one value starting `start` bytes in and leaves the bytes after it alone:
  `position` (an attribute) is then where the value ended, so values written one after another are read one
  after another by starting each read at the last one's `position`. It answers `null` for the same bad bytes and
  when `start` is at or past the end, and a failed read leaves `position` where it was; a negative `start` halts.
- `read_memory(address, count): T?` reads one value from `count` bytes at a `Memory.Address` (a buffer a
  `Socket` filled, say) without copying them into a list first. Afterwards `position` is how many of the bytes
  the value took.

```gdscript
var received = connection.read_bytes_now(buffer, 4096)
var reader = BinaryReader<Move>()
var move = reader.read_memory(buffer, received)
```

Infinity and not-a-number are ordinary bytes: only JSON cannot hold them, so `BinaryWriter` never crashes.

### Several values in one buffer

```gdscript title=binary_stream/binary_stream.spite entry
var console = Console()

func BinaryStream() {
    var bytes = List<Byte>()
    var names = ["ada", "bo", "cy"]
    var name_writer = BinaryWriter<List<String>>()
    name_writer.append_to(names, bytes)
    var turn_writer = BinaryWriter<Integer>()
    turn_writer.append_to(12, bytes)
    var size = bytes.count()
    console.print("bytes:", size)
    var name_reader = BinaryReader<List<String>>()
    var read_names = name_reader.read_from(bytes, 0)
    crash read_names
    var turn_reader = BinaryReader<Integer>()
    var read_turn = turn_reader.read_from(bytes, name_reader.position)
    crash read_turn
    var left = size - turn_reader.position
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
| `Dictionary<T>` | an object, one key per entry; a number key as the number in quotes | its count as a count, then each key as its type is written (a `String`, or the whole number) and its value |
| `T?` | `null`, or what `T` becomes | 1 byte, `0` for empty or `1` followed by what `T` becomes |

A **count** is an unsigned LEB128 varint: seven bits per byte, low bits first, the top bit set on every byte but
the last, so a count below 128 is one byte.

Anything else a class holds (another class, a list of lists, a dictionary of classes) is one of these again,
so each writer and reader goes as deep as the class does. A `union`, a `type` such as `Anything`, or a function
value anywhere in what they see has no form in either, and making one is a compile error at your line, naming the
attribute: `JsonWriter cannot write 'Owner': 'Owner.pet' is the union Pet, and JSON does not say which member a
value is`, or `BinaryReader cannot read 'Owner': 'Owner.pet' is the union Pet, and the bytes do not say which
member a value is` (`diagnostics/json_unwritable`). Keep what they see to the types in this table.

**Private attributes are left out** by both formats: an attribute whose name starts with `_` is the class's own
business ([style.md](style.md)), as `to_debug()` leaves it out, so it is neither written nor read and keeps its
default. It is how a class chooses which of its fields travel; a component that sends only some of its state keeps
the rest in `_` attributes, or the program sends a smaller class that holds only what travels.

**So are attributes holding a singleton** (`var console = Console()`): a singleton is state the whole program
shares, so writing it out would publish it and reading it would let whoever wrote the input replace it. Such an
attribute is neither written nor read, and it is not part of the [schema hash](#the-schema-hash) either.

## The binary format

The table above is the whole format, and it is stable: it depends only on the class's attributes, their order and
their types, never on the machine, the compiler's version or the build, so bytes written today read back
tomorrow, on another machine, from a program built another way. What changes it is changing the class (adding,
removing, reordering or retyping an attribute, or reordering an enum's values), and then both ends must be built
from the new source, since nothing in the bytes says which class they are or what it held.

- **No header, no keys, no padding.** The bytes are the value and nothing else, so values can follow each other in
  one buffer and a program frames them however its transport needs.
- **Little-endian everywhere.** It is the order of every machine Spite compiles for (x86-64, ARM64 and
  WebAssembly), so a number is one store and one load with no swapping. A big-endian
  target would have to swap each number's bytes in `BinaryOutput` and `BinaryInput`; the format would not change.
- **Reading never trusts a count.** A text length or an element count larger than the bytes left fails the read
  before anything is allocated, so a corrupt or hostile buffer cannot make a reader allocate gigabytes.

### The schema hash

A handshake carries one, so an old client meeting a new server fails with a clear report instead of
misreading bytes. It belongs to the handshake or the file header, not to every value: a value's bytes stay bare so
that thousands of them can share one buffer. `writer.schema(): Long` and `reader.schema(): Long` answer a hash of
the attribute walk the bytes follow: the class's name, then each
attribute's name and type in order, nested classes the same way, and each enum's values in order. The compiler
works it out and the C holds the constant, so asking costs nothing; a program writes it once at the start of a
file or a connection and compares it before reading.

```gdscript title=schema_header/point.spite
var across = 0
var down = 0
```
```gdscript title=schema_header/place.spite
var across = 0
var down = 0
```
```gdscript title=schema_header/schema_header.spite entry
var console = Console()

func SchemaHeader() {
    var writer = BinaryWriter<Point>()
    var reader = BinaryReader<Point>()
    var other_writer = BinaryWriter<Place>()
    var written = writer.schema()
    var expected = reader.schema()
    var other = other_writer.schema()
    console.print("{written == expected} {written == other}")
    var point = Point()
    var bytes = writer.write(point)
    var read = reader.read(bytes)
    crash read
    var place = Place()
    var corner = place.across + place.down + read.across + read.down
    console.print(corner)
}
```
```output
true false
0
```

`Place` has exactly `Point`'s bytes, but it is another class, so its hash differs. The hash is the 64-bit FNV-1a
of a text the compiler writes, and which a program never sees: `Point{across:Integer;down:Integer}`, lists as
`List<...>`, dictionaries as `Dictionary<...>` (followed by `by` and the key's type when it is keyed by numbers),
`T?` with a `?`, an enum as its name and its values in brackets, and a class already being written, a recursive
one, by its name alone. The same classes give the same hash in
every build and on every machine; renaming, adding, removing, reordering or retyping an attribute changes it.

## Reading input you did not write

Text from another system rarely matches a class exactly. `JsonReader` is strict about what it cannot use and
lenient about what it does not need:

- **A key the class does not have is skipped**, whatever its value.
- **An attribute the text does not mention keeps its default**, the value the class declares for it.
- **A key in camelCase or PascalCase** (`buyPrice`, `BuyPrice`) reads into the attribute it spells, `buy_price`
  ([below](#a-camelcase-or-pascalcase-key)).
- **A key that cannot be an attribute's name** (`min_lod`, `instances`) is paired with its attribute in a map
  the serializer is given ([below](#a-key-that-is-not-an-attributes-name)).
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
    var partial_reader = JsonReader<Reading>()
    var partial = partial_reader.read("\{ \"name\": \"probe\", \"vendor\": \{ \"id\": [1, 2] } }")
    crash partial
    var tag_count = partial.tags.count()
    console.print(partial.name, partial.level, tag_count)
    var mistyped_reader = JsonReader<Reading>()
    var mistyped = mistyped_reader.read("\{\"level\": \"high\"}")
    if mistyped {
        console.print("unexpected")
    } else {
        console.print("level must be a number")
    }
    var broken_reader = JsonReader<Reading>()
    var broken = broken_reader.read("\{\"name\": \"probe\"")
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

### A camelCase or PascalCase key

Other systems write `buyPrice` or `BuyPrice`
where Spite writes `buy_price`, so a key that names no attribute as written is matched against each attribute's
name spelled in camelCase and in PascalCase. The spellings are constants the compiler writes, so snake_case input
reads exactly as fast as before and a camelCase key costs only the second, shorter look; `JsonWriter` still writes
`buy_price`:

```gdscript title=json_camel_case/price.spite
var name = ""
var buy_price = 0
var sell_price_each = 0.0
```
```gdscript title=json_camel_case/json_camel_case.spite entry
var console = Console()

func JsonCamelCase() {
    var reader = JsonReader<Price>()
    var price = reader.read("\{\"name\": \"potion\", \"buyPrice\": 30, \"SellPriceEach\": 7.5}")
    crash price
    console.print(price.name, price.buy_price, price.sell_price_each)
    var writer = JsonWriter<Price>()
    var text = writer.write(price)
    console.print(text)
}
```
```output
potion 30 7.5
{"name":"potion","buy_price":30,"sell_price_each":7.5}
```

### A key that is not an attribute's name

Some keys cannot be attribute names: `min_lod` is an abbreviation the naming rules refuse, and `instances` is a
name reflection gives every object ([reflection.md](reflection.md)). The class does not name them: the program
sets the serializer's `keys`, a map from each attribute to its key, keyed by the attribute itself, before its
first use, and `JsonWriter` then writes the attribute under that key and `JsonReader` reads that key into it, so the
text round-trips:

```gdscript
var writer = JsonWriter<MeshRecord>()
writer.keys = {MeshRecord.attributes['minimum_level_of_detail']: "min_lod", MeshRecord.attributes['instance_count']: "instances"}
```

The key is the one way to write that attribute: `minimum_level_of_detail` and `minimumLevelOfDetail` are then
unknown keys, skipped like any other. Every serializer takes the same kind of map. A map written as a literal
costs nothing: its keys are literal text in the code the serializer is compiled to, and reading matches them as it
matches an attribute's own name. A map made while the program runs is turned into a table once, when the
`keys` is set, so each value then costs what the literal case costs. A serializer whose `keys` is never set has
no map, and writes and reads every attribute under its own name. A map naming an attribute the class does
not have, a private one or one holding a singleton is an error.

A class that still declares a function `json_key_<attribute>()`, the old way to name a key, is an error at that
function naming the map, so a key it gave is never silently dropped:

```gdscript title=json_key_function/mesh_record.spite
var name = ""
var minimum_level_of_detail = 0

func json_key_minimum_level_of_detail(): String {
    return "min_lod"
}
```
```gdscript title=json_key_function/json_key_function.spite entry error
var console = Console()

func JsonKeyFunction() {
    var reader = JsonReader<MeshRecord>()
    var mesh = reader.read("\{\"name\": \"rock\"}")
    crash mesh
    console.print(mesh.name, mesh.minimum_level_of_detail)
}
```
```diagnostic
'MeshRecord.json_key_minimum_level_of_detail' would name the JSON key of 'minimum_level_of_detail', and JsonReader and JsonWriter do not read a function for it
```

Bytes have no keys to skip, so `BinaryReader` is strict throughout: the bytes are exactly the class, or the read
is `null`.

## Values that are not classes

All four take any type, not only classes: `JsonWriter<Dictionary<Integer>>()` for a dictionary,
`JsonReader<List<Double>>()` to read a list, `BinaryWriter<Integer>()`.

```gdscript title=json_values/json_values.spite entry
var console = Console()

func JsonValues() {
    var scores = Dictionary<Integer>()
    scores["ada"] = 3
    scores["bo"] = 5
    var writer = JsonWriter<Dictionary<Integer>>()
    var text = writer.write(scores)
    console.print(text)
    var numbers = JsonReader<List<Double>>()
    var read = numbers.read("[1.5, -2, 3e2]")
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

`benchmarks/serialisation` writes 100 000 small objects (an `Integer` id, a short name, two `Float`s, a `Short`,
a `Boolean` and an enum) and reads them back, as JSON (one text each) and as binary (all appended to one
`List<Byte>`). `clang -O2`, on a Windows machine, with the allocations `--debug-memory` counts for the
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

- `if $value_type == List { }` asks at compile time what `T` is, and `JsonWriter<$value_type.element_type>()`
  makes the writer for its elements, once per list: each element is given in turn to its `write(element)`. Only the
  branch that fits `T` is compiled.
- A class is written by walking the value's attributes with `each`
  ([reflection.md](reflection.md#a-class-and-an-instance-of-it)). The walk is unrolled while compiling into one
  call per attribute, and each call is compiled for its attribute, so `attribute.value` has the attribute's own
  type and the two questions on the attribute fold away:

```gdscript
func write_attribute(attribute: Spite.Attribute) {
    if not attribute.name.starts_with("_") and not attribute.is_singleton {
        var attribute_json = JsonWriter<attribute.class>()
        attribute_json.path = "{$value_type.name}.{attribute.name}"
        var attribute_text = attribute_json.write(attribute.value)
        crash _members
        _members.append("\"{attribute.name}\":{attribute_text}")
    }
}
```

`each` hands the function only the attribute, so what else the walk needs lives in the writer: `_written` puts a
fresh list in `_members`, calls `shown.attributes.each(write_attribute)` and joins the list. `JsonReader` walks
the object it is filling the same way, `created.attributes.each(read_attribute)`, with the key it just read in
`_key`; the attribute whose name matches parses the value into `attribute.value`, and only a key no name matched
walks again, comparing the camelCase and PascalCase spellings. A class of the `Spite` namespace, a reflection
object, is not walked (`if $value_type.namespace != Spite`): its attributes are all private, and its own
`.attributes` would be the members it describes. `--final-classes` does not print these copies (a
generic class's file is shared by all of its instances), but they are ordinary typed functions in the C the program
compiles to.

The binary pair keeps the walk in `BinaryFormat<T>` (`library/binary_format.spite`), a singleton that holds
nothing, so it is a static object and the walk makes no object at all; its `write_attribute` is the one above
with `BinaryFormat<attribute.class>()` in place of the `JsonWriter`. An enum is walked the same way, over its
values, to find its index. `BinaryOutput` and `BinaryInput` are the
cursors over the bytes. Reading JSON text goes through `JsonCursor` (`library/json_cursor.spite`), a cursor over the
text that remembers the first thing that went wrong.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules disagree,
the rules win.

### JSON is reflection, not a library

Writing JSON is walking a value's attributes and
reading it is a series of symbol keyed writes, so JSON needs no machinery of its own: a `type` shape is the JSON
shape, and the JSON pair is written with the language's own metaprogramming in `library/`, like the rest of the
standard library ([Pure Spite](standard_library.md#pure-spite-dissolving-the-runtime)). JSON is a format
for talking to foreign systems; it is not what Spite programs send each other
([The wire format](targets.md#the-wire-format)).

- **Writing never fails at run time**: a class that cannot be serialised is a compile error, the same check
  isomorphic classes need. Only parsing can fail, and it follows the failure model: a parse that may not succeed
  returns a `T?`, and a variant that crashes exists for callers who want the report instead. The compile-time
  check: when a program's line makes a `JsonWriter<T>`,
  `JsonReader<T>`, `BinaryWriter<T>` or `BinaryReader<T>`, the compiler walks `T` (through `T?`, list
  elements, dictionary values and every attribute of every class it reaches) and a `union`, a `type`
  (`Anything` included) or a function value is an error at that line naming the attribute path; the rest of the
  four classes' files is then not reported on, so one mistake is one error (`diagnostics/json_unwritable`).
  Compile time only. The error names the class and what it does: `<class> cannot write '<T>'` for a writer,
  `cannot read` for a reader, and the reason ends `JSON does not say which member a value is` or `the bytes do not
  say which member a value is`, and `has no JSON form` or `has no binary form`.
- The one exception to "writing never fails" is below: a `Float` or `Double` JSON cannot hold.

**Four classes: `JsonWriter`, `JsonReader`, `BinaryWriter`, `BinaryReader`.** Each names its type and is made once
and reused, and each call carries its own input. Each class is generic, and every conversion it makes is a function the compiler writes from it for
the types a program actually uses, so a program that never names one carries none of it: no table of
types, no reflection and no registry exists at run time. At run time a conversion costs what its Spite says:
`JsonWriter.write(value)` makes one `String` per member and joins them, `JsonReader.read(text)` walks the text once through
one `JsonCursor`, and the binary pair writes and reads each number with one machine store or load.

```gdscript
var writer = JsonWriter<Order>()        # made once, reused for every order
var text = writer.write(order)          # String: never fails
var reader = JsonReader<Order>()        # made once, reused for every text
var read = reader.read(text)            # Order?: null on text that is not an Order
var order = reader.read_or_crash(text)  # Order: halts, naming the position and what was expected

var packer = BinaryWriter<Order>()      # made once, reused for every order
var bytes = packer.write(order)         # List<Byte>: never fails
packer.append_to(order, frame)          # the same bytes, at the end of a List<Byte> the program has
var unpacker = BinaryReader<Order>()    # made once, reused for every buffer
var read = unpacker.read(bytes)         # Order?: null on bytes that are not exactly one Order
var next = unpacker.read_from(frame, unpacker.position)  # the next value in a buffer of many
var from_socket = unpacker.read_memory(buffer, received)
```

The names: `write`, `read` and `read_or_crash`; `append_to(value, bytes)` says where the bytes go ("append", not
"add"); `read_from(bytes, start)` and `read_memory(address, count)` say what they read from; `position` is where
the last value read ended. The old name `Json` is an error that names the new
pair: `there is no 'Json': it is two classes, 'JsonWriter<T>()' whose 'write(value)'
makes the text, and 'JsonReader<T>()' whose 'read(text)' answers a 'T?'` (`diagnostics/json_split`).

- **A JSON serializer is made with no arguments and reused**: `JsonWriter<T>()` and `JsonReader<T>()` name their
  type, and each call carries its own input: `write(value: T?): String`, which writes `null` for an empty `T?`,
  `read(text: String): T?` and `read_or_crash(text: String): T`, each reading the whole text as one value. Nothing
  is held between calls but the serializer's settings, so one writer or reader serves a whole program.
- **A binary serializer is made with no arguments and reused, as a JSON one is**: `BinaryWriter<T>()` with
  `write(value: T): List<Byte>` and `append_to(value: T, bytes: List<Byte>)`, and `BinaryReader<T>()` with
  `read(bytes: List<Byte>): T?`, which reads one value that must fill the bytes, `read_from(bytes: List<Byte>,
  start: Integer): T?`, which reads one value from `start` and leaves what follows, and `read_memory(address,
  count): T?`. The value a writer takes is a `T`, not a `T?`: an empty `T?` is narrowed by the caller, who then
  knows whether anything was written (a `T?` inside a value has its presence byte). `position` is where the last
  successful read ended, so a buffer of many values is read by starting each `read_from` there; nothing else is
  held between calls.
- **What it is written with** is reflection, a walk of the value's attributes with `each`
  ([reflection.md](reflection.md#known-while-compiling)), and
  [Codegen values (`$`)](metaprogramming.md#codegen-values-) and nothing else, as
  [How it is written](#how-it-is-written) shows. The binary walk's helper, `BinaryFormat<T>`, is a `singleton`
  that holds nothing, bound where it is used (`BinaryFormat<attribute.class>()`), which is allowed since a
  singleton made from a `$` or `.class` type cannot be an attribute; in a production build it is a static object
  ([optimizations.md](optimizations.md#singletons-that-hold-nothing-are-static-objects)).
- **An enum is written by walking its values**, which a generic may do for the enum it is given
  ([values_and_types.md](values_and_types.md#enums)): that is how `BinaryFormat` finds an enum value's
  index and how many values the enum has, with no table at run time: writing one costs one comparison per value
  of the enum, and reading one a comparison per value and a text-to-enum cast.
- **What each type becomes** is [the table above](#what-each-type-becomes), applied again to whatever a value
  holds. JSON text escapes `"`, `\`, line feed, carriage return and tab as `\"`, `\\`, `\n`, `\r`, `\t` and every
  other control character as `\u00XX`, and reading turns any `\uXXXX` back into UTF-8, surrogate pairs included. A
  class's attributes are written in declaration order, leaving out private (`_`) ones as `to_debug()` does
  ([standard_library.md](standard_library.md#system-classes)) and ones holding a singleton, which are never read
  either; a `Dictionary`'s entries in the order they were set. Both questions fold where the walk is compiled, so a
  skipped attribute costs nothing.
- **A walk reads every attribute**, so an attribute a program only ever hands to a writer counts as read, and the
  unused-attribute error does not fire for it .

The details:

- **The binary format** is [The binary format](#the-binary-format) and [the table](#what-each-type-becomes):
  little-endian fixed-width numbers, `Boolean` as one byte, an enum as its index in the smallest of 1, 2 or 4
  bytes that holds every index, text as a varint length and UTF-8, `T?` as a presence byte, lists and dictionaries
  as a varint count and the items, nested objects inline, and no header. The varint
  is unsigned LEB128, and a count or length above what an `Integer` holds, or above the bytes left, fails the read.
- **`BinaryWriter<T>.schema()` and `BinaryReader<T>.schema()` answer a `Long` the compiler works out** (the
  name is provisional): the 64-bit FNV-1a hash, as a signed `Long`, of the
  walk's text: the class's qualified name, then `name:type` for each attribute the bytes carry (not `_...`, and
  not one holding a singleton), in
  order, joined by `;` inside `{ }`, a nested class written the same way, `List<T>`, `Dictionary<T>`, `T?`, an
  enum as its name and `(` its values joined by `,` `)`, a class already being written by its name alone, and any
  other type by its name. It is a bodiless declaration the compiler supplies, a C macro that is the constant,
  so it costs nothing at run time and a program that never asks carries none of it. A writer and a reader of the
  same `T` answer the same value (`conformance/stage6/binary_schema`).
- **The bytes are a `List<Byte>`**: one block, no count per byte, and a reader borrows nothing, since each
  call reads the block of the list it is given and keeps nothing of it. `read_memory` covers memory the program did
  not put in a list. `List<T>` has `reserve(count)` for this, making room for `count` items without making any;
  the library's binary classes grow the list and write into its block directly, which only `library/` may do.
- **`BinaryReader` answers `null` for every bad input** (bad input is a normal condition, never a crash):
  bytes that end inside a value, a `Boolean` byte other than 0 or 1, a presence byte other than 0 or 1, an enum
  index past the enum's last value, a plain `Symbol` name the program does not use, and a count past the bytes
  left, and for `read`, bytes left over after the value. A failed read leaves `position` where it was, and a
  negative `start` for `read_from` halts, since it is a mistake in the program and not bad bytes. There is no `read_or_crash` for bytes: the bytes a Spite
  program reads were written by a Spite program, and what went wrong is not something a position would explain.
- **The API is a class you make once and reuse**, with `write` and `read`. The generic is on
  the class because that is where Spite puts generics. `write` needs no crashing twin, since it cannot fail.
- **`JsonReader` is strict about what it cannot use and lenient about what it does not need**
  ([Reading input you did not write](#reading-input-you-did-not-write), and
  [Failure](failure.md#failure-three-outcomes-and-no-others)'s three outcomes). An unknown key is skipped,
  whatever it holds, because foreign systems send more than a program asks for; a missing attribute keeps the
  default its class declares, the value [Variables and values](values_and_types.md#variables-and-values)
  gives anything never set. Besides a value of the wrong kind, a missing brace and anything after the value
  (`{} x`) make `read` answer `null`: the object would be wrong, and a wrong object that looks right is a
  surprise. `read_or_crash` crashes on the same inputs, and its crash line carries
  `wanted=<what>` and `position=<n>`.
- **A key in camelCase or PascalCase reads into the snake_case attribute it spells.** The exact name wins:
  `JsonReader` first compares the key with every attribute's name, as it always did, and only a key that matched
  none is compared with each attribute's name written in camelCase and in PascalCase (the words after the
  first, or every word, capitalised and the underscores dropped: `buy_price` is `buyPrice` and `BuyPrice`). A key
  that matches neither is skipped as before. A snake_case key never matches a spelling (those have no
  underscores), and only a snake_case name has one, so no key can match two attributes; when the text holds both
  `buy_price` and `buyPrice`, the later one wins, as a repeated key does. `JsonWriter` writes the attribute's own
  name. The spellings are compile-time constants a walked attribute answers, `attribute.camel_case_name` and
  `attribute.pascal_case_name` ([metaprogramming.md](metaprogramming.md#templates)),
  and `JsonReader` compares them in a second walk, `created.attributes.each(read_camel_attribute)`, that runs only
  for a key the first walk did not match: snake_case input runs the same comparisons it did before (`benchmarks/serialisation`, JSON
  read, best of eight alternating runs on a loaded machine: 269 ms before, 267 ms after), and a program that reads
  no JSON carries none of it (`conformance/stage6/json_camel_case`).
- **A key that is not an attribute's name comes from the serializer's optional `keys` attribute, a map keyed by
  attribute objects and set before the serializer's first use**: `{MeshRecord.attributes['minimum_level_of_detail']: "min_lod"}`. `JsonWriter` writes the attribute
  under its key and `JsonReader` reads that key into it, so the text round-trips. **The key replaces the
  attribute's name**: with one, neither the name nor its camelCase and PascalCase spellings read into the
  attribute any more, so each attribute has exactly one key; attributes without one keep the camelCase and
  PascalCase reading. Every serializer takes the same kind of map. A constant map folds into the code the
  serializer is compiled to: the keys are literal text, and reading matches them as it matches a name. A map made
  at run time fills a key table once per attribute when the serializer is made (an array by attribute, and a small
  hash for reading), so each record then costs what the constant case costs. Naming an attribute the class does
  not have, a private one or one holding a singleton is an error. Nothing in a class names its own keys.
- **A function `json_key_<attribute>()`**, where `<attribute>` is one of the class's attributes, is the old way
  to name a key and an error at the function, so a key it gave is never silently lost: "'MeshRecord.json_key_minimum_level_of_detail'
  would name the JSON key of 'minimum_level_of_detail', and JsonReader and JsonWriter do not read a function for
  it: a key that is not an attribute's name is given to the serializer in a map keyed by the attribute,
  '{MeshRecord.attributes['minimum_level_of_detail']: "..."}'" (`diagnostics/json_key`).
- **A number reads into whatever number type the attribute has** through the ordinary text-to-number cast, so
  `3.7` read into an `Integer` is `3`; JSON has one number type and the class already says which one it wants.
- **Infinity and not-a-number crash `JsonWriter`**: JSON
  (RFC 8259) holds neither, and a float became one through a division by zero or an overflow the program did not
  guard, which is the moron's mistake ([failure.md](failure.md)); floats themselves keep them,
  and `BinaryWriter` writes their bits like any other. The crash shows the value and the attribute's path,
  `Class.attribute`, with `[index]` or `["key"]` for an element: `shown=inf	path=Order.price` (and the
  same for negative infinity and not-a-number; `conformance/stage6/json_infinity`). A program that wants `null`
  there checks the number first.
- `--final-classes` does not print the functions `JsonWriter<Order>` generated, because a generic class's file
  is shared by all its instances.
- **A plain `Symbol` attribute** (as opposed to an enum) is written as its name and read back through
  `Symbol(text)`, so a name the program does not already use as a symbol is a value of the wrong kind: `read`
  answers `null` (`conformance/stage6/json_symbols`). The library tells the two
  apart with the codegen test `$value_type == Enum`, true for an enum only, where `$value_type == Symbol` is true
  for both ([metaprogramming.md](metaprogramming.md)).

---

Next: [Time](time.md), dates, durations, clocks and time zones.
